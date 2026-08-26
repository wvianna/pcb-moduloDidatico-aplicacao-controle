#pragma once
// alarm.h — Máquina de estados de alarme com histerese (Q-001).
// Lógica pura, independente de Arduino.h, para permitir teste em HOST.

#include "config.h"
#include "state.h"

class AlarmFSM {
public:
  AlarmFSM();
  EstadoAlarme update(float pv);   // avalia a PV e retorna o novo estado
  EstadoAlarme get() const { return state_; }

private:
  EstadoAlarme state_;
};
