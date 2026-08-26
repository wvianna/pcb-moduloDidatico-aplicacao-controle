#include "alarm.h"

// NORMAL → ALARME quando pv >= ALARM_ON_TEMP_C (80 °C).
// ALARME → NORMAL quando pv < ALARM_OFF_TEMP_C (78 °C).
// Histerese de 2 °C evita chattering (Decisão Q-001).
AlarmFSM::AlarmFSM() : state_(EstadoAlarme::NORMAL) {}

EstadoAlarme AlarmFSM::update(float pv) {
  switch (state_) {
    case EstadoAlarme::NORMAL:
      if (pv >= ALARM_ON_TEMP_C) state_ = EstadoAlarme::ALARME;
      break;
    case EstadoAlarme::ALARME:
      if (pv < ALARM_OFF_TEMP_C) state_ = EstadoAlarme::NORMAL;
      break;
  }
  return state_;
}
