#include "control.h"

// ---------------------------------------------------------------------------
// ControlSupervisor — agendamento do PID e aplicação aos atuadores
// ---------------------------------------------------------------------------
void ControlSupervisor::begin() {
  pid_.begin();
  lastPidMs_ = 0;
  prevModo_  = Modo::MANUAL;
  firstRun_  = true;
  tuningStarted_ = false;
}

void ControlSupervisor::tick(TempSensor& sensor, Heater& heater,
                             Buzzer& buzzer, ControlState& cs) {
  // 1. Máquina de alarme (histerese — Q-001).
  cs.alarm = alarm_.update(cs.pv);

  // 2. Fail-safe: se o sensor parar de atualizar (leitura congelada) o sistema
  //    corta a resistência para evitar superaquecimento (segurança).
  bool sensorStale = ((uint32_t)(millis() - cs.ts) > SENSOR_TIMEOUT_MS);
  if (sensorStale) cs.sensor_fail = true;

  bool unsafe = (cs.alarm == EstadoAlarme::ALARME) || sensorStale || cs.sensor_fail;
  if (unsafe) {
    heater.off();                                   // estado seguro (FR-012 / NFR-011)
    cs.mv = 0.0f;
    if (cs.alarm == EstadoAlarme::ALARME) buzzer.setAlarm(true);   // bip 150 ms / 2 s
    else                                  buzzer.setAlarm(false);
  } else {
    buzzer.setAlarm(false);
  }

  // 3. Reset de integral na transição de modo (anti-windup — FR-022).
  if (cs.modo != prevModo_) {
    pid_.reset();
    prevModo_ = cs.modo;
    tuningStarted_ = false;   // reinicia autotuner se (re)entrar em TUNING
  }

  // Em estado seguro, não aplica nenhuma atuação.
  if (unsafe) return;

  // 4. Aplica a ação por modo.
  switch (cs.modo) {
    case Modo::MANUAL:
      // MV definida pelo operador; PID pausado.
      heater.setMV(cs.mv);
      break;

    case Modo::AUTO: {
      uint32_t now = millis();
      if ((uint32_t)(now - lastPidMs_) >= PID_PERIOD_MS) {
        float dt = (float)(now - lastPidMs_) / 1000.0f;
        if (firstRun_) { dt = (float)PID_PERIOD_MS / 1000.0f; firstRun_ = false; }
        lastPidMs_ = now;
        float out = pid_.compute(cs.setpoint, cs.pv, dt, cs.pid, /*integralActive=*/true);
        cs.mv = out;
        heater.setMV(out);   // aplica MV satura 0..100 (FR-021)
      }
      break;
    }

    case Modo::TUNING:
      // Autotuning (T-012): inicia o tuner na primeira passagem e o itera.
      if (!tuningStarted_) {
        tuner_.begin(cs, heater);
        tuningStarted_ = true;
      }
      if (tuner_.tick(cs, heater)) {
        tuningStarted_ = false;   // concluiu (DONE_OK/BAD) ou timeout
      }
      break;
  }
}
