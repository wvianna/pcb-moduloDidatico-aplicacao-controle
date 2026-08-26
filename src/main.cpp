#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "config.h"
#include "state.h"
#include "sensors.h"
#include "actuators.h"
#include "control.h"
#include "web_server.h"

// Estado compartilhado do sistema (escritas apenas no loop; servidor lê).
ControlState cs;
TempSensor sensor;
Heater heater;
Buzzer buzzer;
ControlSupervisor ctrl;
WebServer web;

// Medição de carga de trabalho do MCU (exibida no cabeçalho do dashboard).
static uint32_t loadWindowStart = 0;  // início da janela de 1 s
static uint32_t busyMicros      = 0;  // tempo "ocupado" acumulado (µs)

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.printf("[BOOT] Sistema de controle termico inic. | CPU=%lu MHz\n",
                (unsigned long)ESP.getCpuFreqMHz());
  cs = state_init();
  sensor.begin();
  heater.begin();
  buzzer.begin();
  ctrl.begin();
  web.begin(&cs);
  Serial.printf("[BOOT] PV=%.1f MV=%.1f SP=%.1f modo=%s\n", cs.pv, cs.mv, cs.setpoint, modoToString(cs.modo));
}

void loop() {
  uint32_t t0 = micros();   // inicia a medição de carga

  // 1. Servidor HTTP (não-bloqueante).
  web.handle();

  // 2. Aquisição não-bloqueante da temperatura (~1 Hz).
  sensor.tick();
  if (sensor.readReady()) {
    cs.pv = sensor.getPV();
    cs.sensor_fail = sensor.isFault();
    cs.ts = millis();
    sensor.clearReady();
  }

  // 3. Supervisor de controle (alarme + PID + atuadores).
  ctrl.tick(sensor, heater, buzzer, cs);

  // 4. FSM do buzzer (cadência non-blocking).
  buzzer.tick();

  // 5. Log de status periódico (diagnóstico / evidência em BANCADA).
  static uint32_t lastStatusMs = 0;
  if ((uint32_t)(millis() - lastStatusMs) >= 2000) {
    lastStatusMs = millis();
    Serial.printf("[STATUS] PV=%.1f MV=%.1f SP=%.1f modo=%s alarm=%s sensor=%s enP=%d enD=%d\n",
                  cs.pv, cs.mv, cs.setpoint, modoToString(cs.modo),
                  (cs.alarm == EstadoAlarme::ALARME) ? "ON" : "OFF",
                  cs.sensor_fail ? "FAIL" : "OK",
                  cs.pid.enableP ? 1 : 0, cs.pid.enableD ? 1 : 0);
  }

  busyMicros += (uint32_t)(micros() - t0);   // acumula o tempo de trabalho

  // 6. Atualiza a saúde do MCU a cada 1 s.
  uint32_t nowMs = millis();
  if ((uint32_t)(nowMs - loadWindowStart) >= 1000) {
    uint32_t winMs  = (uint32_t)(nowMs - loadWindowStart);
    float busy = (winMs > 0)
        ? (((float)busyMicros) / ((float)winMs * 1000.0f)) * 100.0f
        : 0.0f;
    if (busy > 100.0f) busy = 100.0f;

    cs.health.cpuFreqMHz  = (float)ESP.getCpuFreqMHz();
    cs.health.busyPct     = busy;
    cs.health.idlePct     = 100.0f - busy;
    cs.health.flashMB     = (float)ESP.getFlashChipSize() / 1048576.0f;
    cs.health.sketchKB    = (float)ESP.getSketchSize() / 1024.0f;
    cs.health.freeHeap    = ESP.getFreeHeap();
    cs.health.maxFreeBlock = ESP.getMaxFreeBlockSize();
    cs.health.heapFragPct = (uint16_t)ESP.getHeapFragmentation();
    cs.health.uptimeSec   = nowMs / 1000UL;
    cs.health.wifiClients = WiFi.softAPgetStationNum();

    busyMicros     = 0;
    loadWindowStart = nowMs;
  }

  delay(5);
}