#pragma once
// pid.h — Controlador PID custom com anti-windup (Decisão Q-002).
//
// Posicional, com saturação de saída em [0,100], anti-windup por *back-calculation*
// e reset de integral. Não depende de Arduino.h para permitir teste em HOST.

#include "config.h"
#include "state.h"

class PIDController {
public:
  void begin();

  // setpoint (°C), pv (°C), dt em segundos e parâmetros.
  // integralActive=false pausa a integral (ex.: durante transição de modo).
  float compute(float setpoint, float pv, float dtSec,
                const ParamsPID& p, bool integralActive);

  void reset();                 // zera integral e derivada (anti-windup)
  float getOutput() const { return out_; }

private:
  float integral_;
  float prevPv_;
  float out_;
};
