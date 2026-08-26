#pragma once
// tuning_math.h — Fórmulas de sintonia (Ziegler-Nichols por relé e Cohen-Coon).
// Lógica pura, sem Arduino.h, para permitir teste em HOST.

// Resultado da sintonia (ganhos do PID).
struct TuningResult {
  float kp;
  float ki;
  float kd;
  bool  valid;   // false se os parâmetros forem inutilizáveis
};

// Ziegler-Nichols por relé: a partir do ganho crítico Ku e período crítico Tu.
inline TuningResult znRelay(float Ku, float Tu) {
  TuningResult r{0, 0, 0, false};
  if (Ku <= 0.0f || Tu <= 0.0f) return r;
  float Kp = 0.6f * Ku;
  float Ti = 0.5f * Tu;
  float Td = 0.125f * Tu;
  r.kp = Kp;
  r.ki = (Ti > 0.0f) ? (Kp / Ti) : 0.0f;
  r.kd = Kp * Td;
  r.valid = true;
  return r;
}

// Cohen-Coon (PID) a partir de ganho K, constante de tempo tau e atraso theta
// do modelo FOPDT. Retorna valid=false se os parâmetros forem degenerados.
inline TuningResult cohenCoon(float K, float tau, float theta) {
  TuningResult r{0, 0, 0, false};
  if (K == 0.0f || tau <= 0.0f || theta <= 0.0f) return r;

  float thT = theta / tau;          // razão atraso/constante de tempo

  if (thT > 10.0f) {                // planta muito atrasada — indisposta p/ CC clássico
    return r;
  }

  float Kp = (1.0f / K) * (tau / theta) * ((4.0f / 3.0f) + (theta / (4.0f * tau)));
  float Ti = theta * (32.0f + 6.0f * thT) / (13.0f + 8.0f * thT);
  float Td = (4.0f * theta) / (11.0f + 2.0f * thT);

  r.kp = Kp;
  r.ki = (Ti > 0.0f) ? (Kp / Ti) : 0.0f;
  r.kd = Kp * Td;
  r.valid = true;
  return r;
}
