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
static void handleRoot() {
  server.send_P(200, "text/html", DASHBOARD_HTML);
}

// ---------------------------------------------------------------------------
// GET /api/state — estado atual em JSON
// ---------------------------------------------------------------------------
static void handleState() {
  JsonDocument doc;
  doc["pv"]        = g_cs->pv;
  doc["mv"]        = g_cs->mv;
  doc["setpoint"]  = g_cs->setpoint;
  doc["mode"]      = modoToString(g_cs->modo);
  doc["alarm"]     = (g_cs->alarm == EstadoAlarme::ALARME);
  doc["sensor_fail"] = g_cs->sensor_fail;
  doc["ts"]        = g_cs->ts;

  JsonObject pid = doc["pid"].to<JsonObject>();
  pid["p"]       = g_cs->pid.kp;
  pid["i"]       = g_cs->pid.ki;
  pid["d"]       = g_cs->pid.kd;
  pid["enableP"] = g_cs->pid.enableP;
  pid["enableD"] = g_cs->pid.enableD;

  static char buf[320];
  serializeJson(doc, buf, sizeof(buf));
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
  WiFi.softAP(ssid_);

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
