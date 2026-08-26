#include "web_server.h"
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include "config.h"
#include "dashboard.h"

static ESP8266WebServer server(HTTP_SERVER_PORT);
static ControlState* g_cs = nullptr;

// ---------------------------------------------------------------------------
// Rota / — dashboard (PROGMEM)
// ---------------------------------------------------------------------------
// Envia conteúdo PROGMEM de forma confiável em pedaços (evita "short send" do
// ESP8266WebServer, que trunca respostas grandes como a página do dashboard).
static void sendProgmemStream(WiFiClient& client, const char* src, size_t total) {
  const size_t CHUNK = 512;
  static char buf[CHUNK];
  size_t off = 0;
  while (off < total && client.connected()) {
    size_t n = (total - off < CHUNK) ? (total - off) : CHUNK;
    memcpy_P(buf, src + off, n);
    size_t sent = 0;
    unsigned tries = 0;
    while (sent < n && client.connected()) {
      int w = client.write((const uint8_t*)buf + sent, n - sent);
      if (w > 0) { sent += (size_t)w; tries = 0; }
      else {
        client.flush();
        if (++tries > 3000) break;   // segurança: evita travamento
        delay(1);
      }
    }
    client.flush();
    off += n;
    yield();
  }
}

static void handleRoot() {
  const size_t total = strlen_P(DASHBOARD_HTML);
  server.setContentLength(total);
  server.send(200, "text/html", "");          // cabeçalho com Content-Length correto
  sendProgmemStream(server.client(), DASHBOARD_HTML, total);
}

// ---------------------------------------------------------------------------
// GET /api/state — estado atual em JSON
// ---------------------------------------------------------------------------
static void handleState() {
  // JSON em buffer estático (snprintf) — sem alocação dinâmica no caminho de
  // polling (NFR-007). Evita fragmentação de heap no ESP8266, causa de travamento.
  static char buf[384];
  snprintf(buf, sizeof(buf),
    "{\"pv\":%.2f,\"mv\":%.1f,\"setpoint\":%.1f,\"mode\":\"%s\",\"alarm\":%s,"
    "\"sensor_fail\":%s,\"ts\":%lu,"
    "\"pid\":{\"p\":%.3f,\"i\":%.3f,\"d\":%.3f,\"enableP\":%s,\"enableD\":%s},"
    "\"health\":{\"cpu\":%.0f,\"load\":%.1f,\"idle\":%.1f,\"flash\":%.1f,\"sketch\":%.1f,"
    "\"heap\":%lu,\"mblock\":%lu,\"frag\":%u,\"up\":%lu,\"wifi\":%u}}",
    g_cs->pv, g_cs->mv, g_cs->setpoint, modoToString(g_cs->modo),
    (g_cs->alarm == EstadoAlarme::ALARME) ? "true" : "false",
    g_cs->sensor_fail ? "true" : "false",
    (unsigned long)g_cs->ts,
    g_cs->pid.kp, g_cs->pid.ki, g_cs->pid.kd,
    g_cs->pid.enableP ? "true" : "false",
    g_cs->pid.enableD ? "true" : "false",
    (double)g_cs->health.cpuFreqMHz,
    (double)g_cs->health.busyPct,
    (double)g_cs->health.idlePct,
    (double)g_cs->health.flashMB,
    (double)g_cs->health.sketchKB,
    (unsigned long)g_cs->health.freeHeap,
    (unsigned long)g_cs->health.maxFreeBlock,
    (unsigned int)g_cs->health.heapFragPct,
    (unsigned long)g_cs->health.uptimeSec,
    (unsigned int)g_cs->health.wifiClients);
  server.send(200, "application/json", buf);
}

// ---------------------------------------------------------------------------
// POST /api/control — aplica configuração (setpoint, modo, mv, PID, autotuning)
// ---------------------------------------------------------------------------
static void handleControl() {
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"no body\"}");
    return;
  }
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, server.arg("plain"));
  if (err) {
    server.send(400, "application/json", "{\"error\":\"invalid json\"}");
    return;
  }

  // Setpoint: satura em [SETPOINT_MIN, SETPOINT_MAX] (CA-015).
  if (doc.containsKey("setpoint")) {
    g_cs->setpoint = constrain(doc["setpoint"] | g_cs->setpoint,
                               SETPOINT_MIN_C, SETPOINT_MAX_C);
  }

  // Modo.
  if (doc.containsKey("mode")) {
    const char* m = doc["mode"] | "auto";
    if (strcmp(m, "manual") == 0)      g_cs->modo = Modo::MANUAL;
    else if (strcmp(m, "tuning") == 0) g_cs->modo = Modo::TUNING;
    else                               g_cs->modo = Modo::AUTO;
  }

  // MV manual: satura em [0,100].
  if (doc.containsKey("mv")) {
    g_cs->mv = constrain(doc["mv"] | g_cs->mv, MV_MIN_PERCENT, MV_MAX_PERCENT);
  }

  // Parâmetros PID.
  if (doc["pid"].is<JsonObject>()) {
    JsonObject pj = doc["pid"].as<JsonObject>();
    if (pj.containsKey("p")) g_cs->pid.kp = pj["p"] | g_cs->pid.kp;
    if (pj.containsKey("i")) g_cs->pid.ki = pj["i"] | g_cs->pid.ki;
    if (pj.containsKey("d")) g_cs->pid.kd = pj["d"] | g_cs->pid.kd;
    if (pj.containsKey("enableP")) g_cs->pid.enableP = pj["enableP"] | g_cs->pid.enableP;
    if (pj.containsKey("enableD")) g_cs->pid.enableD = pj["enableD"] | g_cs->pid.enableD;
    Serial.printf("[CTRL] pid p=%.3f i=%.3f d=%.3f enP=%d enD=%d\n",
                  g_cs->pid.kp, g_cs->pid.ki, g_cs->pid.kd,
                  g_cs->pid.enableP ? 1 : 0, g_cs->pid.enableD ? 1 : 0);
  }

  // Autotuning: seleciona método e inicia (T-012).
  if (doc.containsKey("tuning")) {
    JsonObject t = doc["tuning"].as<JsonObject>();
    if (t.containsKey("method")) {
      const char* mt = t["method"] | "relay";
      g_cs->metodo = (strcmp(mt, "step") == 0) ? MetodoTuning::STEP : MetodoTuning::RELAY;
    }
    g_cs->tuning = EstadoTuning::RUNNING;
    g_cs->modo   = Modo::TUNING;
  }

  server.send(200, "application/json", "{\"ok\":true}");
}

// ---------------------------------------------------------------------------
// WebServer
// ---------------------------------------------------------------------------
void WebServer::begin(ControlState* cs) {
  g_cs = cs;

  // SSID derivado do chip id (Q-005).
  String ssid = String(AP_SSID_PREFIX) + String(ESP.getChipId(), HEX);
  ssid.toUpperCase();
  snprintf(ssid_, sizeof(ssid_), "%s", ssid.c_str());  // copia para buffer fixo

  IPAddress ip(AP_IP_0, AP_IP_1, AP_IP_2, AP_IP_3);
  IPAddress gw(AP_GW_0, AP_GW_1, AP_GW_2, AP_GW_3);
  IPAddress mask(AP_MASK_0, AP_MASK_1, AP_MASK_2, AP_MASK_3);

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(ip, gw, mask);       // IP estático + DHCP na sub-rede
  // Acesso multicliente: múltiplos clientes podem se associar ao AP
  // (AP_MAX_CONNECTIONS) e o ESP8266WebServer atende conexões concorrentes.
  WiFi.softAP(ssid_, NULL, AP_SOFTAP_CHANNEL, 0, AP_MAX_CONNECTIONS);

  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/state", HTTP_GET, handleState);
  server.on("/api/control", HTTP_POST, handleControl);
  server.begin();

  Serial.printf("[NET] AP: %s | IP: %u.%u.%u.%u\n", ssid_,
                ip[0], ip[1], ip[2], ip[3]);
}

void WebServer::handle() {
  server.handleClient();
}

const char* WebServer::getSSID() const {
  return ssid_;
}
