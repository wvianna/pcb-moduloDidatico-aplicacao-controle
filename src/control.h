#pragma once
// control.h — Supervisor de controle + máquina de estados de alarme.
//
// AlarmFSM é lógica pura (testável em HOST). ControlSupervisor é a camada que
// liga sensor, atuadores e PID no loop com agendamento determinístico.

#include <Arduino.h>
#include "config.h"
#include "state.h"
#include "sensors.h"
#include "actuators.h"
#include "pid.h"
#include "alarm.h"
#include "autotune.h"

// ---------------------------------------------------------------------------
// Supervisor de controle (agendamento determinístico do PID)
// ---------------------------------------------------------------------------
class ControlSupervisor {
public:
  void begin();
  void tick(TempSensor& sensor, Heater& heater, Buzzer& buzzer, ControlState& cs);

private:
  PIDController pid_;
  AlarmFSM      alarm_;
  AutoTuner     tuner_;
  uint32_t      lastPidMs_;
  Modo          prevModo_;
  bool          firstRun_;
  bool          tuningStarted_;
};
