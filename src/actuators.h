#pragma once
// actuators.h — Atuadores: resistência de aquecimento (PWM) e buzzer (on/off).
// A cadência do buzzer é feita por FSM baseada em millis() — nunca delay()
// no caminho crítico (FR-015).

#include <Arduino.h>
#include "config.h"

// ---------------------------------------------------------------------------
// Resistência de aquecimento — PWM 10 bits (0..1023), MV em 0..100 %
// ---------------------------------------------------------------------------
class Heater {
public:
  void begin();
  void setMV(float percent);   // satura em [0,100] e mapeia para PWM
  void off() { setMV(0.0f); }  // desliga (estado seguro em ALARME)
  int   getRawPWM() const { return raw_; }
  float getMV() const { return mv_; }

private:
  int   raw_;   // valor PWM aplicado (0..1023)
  float mv_;    // valor em % (0..100)
};

// ---------------------------------------------------------------------------
// Buzzer — cadência intermitente non-blocking (150 ms on / 2 s off)
// ---------------------------------------------------------------------------
class Buzzer {
public:
  void begin();
  void setAlarm(bool on);  // habilita/desabilita a cadência (entrada do alarme)
  void tick();             // chame a cada loop() para atualizar a FSM
  bool isEnabled() const { return alarmEnabled_; }

private:
  bool     alarmEnabled_;  // alarme ativo
  bool     outputOn_;      // estado atual do pino
  uint32_t lastMs_;        // último tick do estado
};
