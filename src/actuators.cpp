#include "actuators.h"

// ---------------------------------------------------------------------------
// Heater (PWM)
// ---------------------------------------------------------------------------
void Heater::begin() {
  pinMode(PIN_HEATER_PWM, OUTPUT);
  raw_ = 0;
  mv_  = 0.0f;
  analogWrite(PIN_HEATER_PWM, 0);
}

void Heater::setMV(float percent) {
  // Satura em [0,100] (FR-008 / FR-021) e mapeia para 0..1023.
  mv_ = constrain(percent, MV_MIN_PERCENT, MV_MAX_PERCENT);
  raw_ = (int)lroundf((mv_ / 100.0f) * (float)PWM_MAX);
  analogWrite(PIN_HEATER_PWM, raw_);
}

// ---------------------------------------------------------------------------
// Buzzer (FSM non-blocking) — bip de BUZZER_ON_MS a cada BUZZER_PERIOD_MS
// ---------------------------------------------------------------------------
void Buzzer::begin() {
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);
  alarmEnabled_ = false;
  outputOn_     = false;
  lastMs_       = 0;
}

void Buzzer::setAlarm(bool on) {
  if (on && !alarmEnabled_) {
    alarmEnabled_ = true;
    outputOn_     = true;
    digitalWrite(PIN_BUZZER, HIGH);
    lastMs_ = millis();
  } else if (!on && alarmEnabled_) {
    alarmEnabled_ = false;
    outputOn_     = false;
    digitalWrite(PIN_BUZZER, LOW);
  }
}

void Buzzer::tick() {
  if (!alarmEnabled_) return;

  uint32_t now = millis();
  uint32_t elapsed = (uint32_t)(now - lastMs_);

  if (outputOn_ && elapsed >= BUZZER_ON_MS) {
    // termina o bip de 150 ms
    outputOn_ = false;
    digitalWrite(PIN_BUZZER, LOW);
    lastMs_ = now;
  } else if (!outputOn_ && elapsed >= (BUZZER_PERIOD_MS - BUZZER_ON_MS)) {
    // reinicia o bip após completar o período de 2 s
    outputOn_ = true;
    digitalWrite(PIN_BUZZER, HIGH);
    lastMs_ = now;
  }
}
