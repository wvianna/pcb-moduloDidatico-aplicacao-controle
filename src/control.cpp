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
  // 1. Máquina de alarme (estado seguro sobrepõe o controle).
  cs.alarm = alarm_.update(cs.pv);
  if (cs.alarm == EstadoAlarme::ALARME) {
    heater.off();            // desliga a resistência (FR-012)
    buzzer.setAlarm(true);   // bip 150 ms / 2 s (FR-013)
    cs.mv = 0.0f;
  } else {
    buzzer.setAlarm(false);
  }

  // 2. Reset de integral na transição de modo (anti-windup — FR-022).
  if (cs.modo != prevModo_) {
    pid_.reset();
    prevModo_ = cs.modo;
    tuningStarted_ = false;   // reinicia autotuner se (re)entrar em TUNING
  }

  // 3. Aplica a ação por modo.
  switch (cs.modo) {
    case Modo::MANUAL:
      // MV definida pelo operador; PID pausado.
      heater.setMV(cs.mv);
      break;

    case Modo::AUTO:
      if (cs.alarm != EstadoAlarme::ALARME) {
        uint32_t now = millis();
        if ((uint32_t)(now - lastPidMs_) >= PID_PERIOD_MS) {
          float dt = (float)(now - lastPidMs_) / 1000.0f;
          if (firstRun_) { dt = (float)PID_PERIOD_MS / 1000.0f; firstRun_ = false; }
          lastPidMs_ = now;
          float out = pid_.compute(cs.setpoint, cs.pv, dt, cs.pid, /*integralActive=*/true);
          cs.mv = out;
          heater.setMV(out);   // aplica MV satura 0..100 (FR-021)
          cs.ts = millis();
        }
      }
      break;

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
