#include <Arduino.h>
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

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println(F("[BOOT] Sistema de controle termico inic."));
  cs = state_init();
  sensor.begin();
  heater.begin();
  buzzer.begin();
  ctrl.begin();
  web.begin(&cs);
  Serial.printf("[BOOT] PV=%.1f MV=%.1f SP=%.1f modo=%s\n", cs.pv, cs.mv, cs.setpoint, modoToString(cs.modo));
}

void loop() {
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
    Serial.printf("[STATUS] PV=%.1f MV=%.1f SP=%.1f modo=%s alarm=%s sensor=%s\n",
                  cs.pv, cs.mv, cs.setpoint, modoToString(cs.modo),
                  (cs.alarm == EstadoAlarme::ALARME) ? "ON" : "OFF",
                  cs.sensor_fail ? "FAIL" : "OK");
  }

  delay(5);
}