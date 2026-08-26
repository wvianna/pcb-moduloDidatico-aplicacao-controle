#include "pid.h"

// clampf — limita v ao intervalo [lo, hi]. Independente de Arduino.h.
static inline float clampf(float v, float lo, float hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

void PIDController::begin() {
  reset();
}

void PIDController::reset() {
  integral_ = 0.0f;
  prevPv_   = 0.0f;
  out_      = 0.0f;
}

float PIDController::compute(float setpoint, float pv, float dtSec,
                             const ParamsPID& p, bool integralActive) {
  if (dtSec <= 0.0f) dtSec = 1e-3f;  // proteção contra divisão por zero

  float error = setpoint - pv;

  // Termo proporcional (habilitável — FR-017)
  float pTerm = (p.enableP) ? (p.kp * error) : 0.0f;

  // Termo integral (sempre ativo — FR-019) com clamp anti-windup
  float iTerm = 0.0f;
  if (integralActive) {
    integral_ += p.ki * error * dtSec;
    integral_ = clampf(integral_, -PID_KI_MAX, PID_KI_MAX);
    iTerm = integral_;
  }

  // Termo derivativo (habilitável — FR-018); derivada sobre a PV p/ evitar
  // *derivative kick* no setpoint.
  float dTerm = 0.0f;
  if (p.enableD) {
    float dPv = (pv - prevPv_) / dtSec;
    dTerm = -p.kd * dPv;
  }

  float raw = pTerm + iTerm + dTerm;

  // Saturação da saída em [0,100] (FR-021) + anti-windup por back-calculation.
  float out = clampf(raw, 0.0f, 100.0f);
  if (integralActive) {
    float excess = raw - out;          // quanto a saída excedeu o limite
    integral_ -= excess;               // resgata a integral
    integral_ = clampf(integral_, -PID_KI_MAX, PID_KI_MAX);
  }

  prevPv_ = pv;
  out_    = out;
  return out_;
}
