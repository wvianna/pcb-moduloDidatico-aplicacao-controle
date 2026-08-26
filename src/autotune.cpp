#include "autotune.h"
#include "tuning_math.h"

// ---------------------------------------------------------------------------
// begin — entra em modo de sintonia (usado pelo supervisor ao receber TUNING)
// ---------------------------------------------------------------------------
void AutoTuner::begin(ControlState& cs, Heater& heater) {
  cs.tuning = EstadoTuning::RUNNING;
  cs.modo   = Modo::TUNING;

  startMs_  = millis();
  mvBase_   = cs.mv;
  method_   = cs.metodo;

  relayHigh_ = true;
  relMin_ = 1e9f;
  relMax_ = -1e9f;
  relFirstCrossMs_ = 0;
  relLastCrossMs_  = 0;
  relCrossings_    = 0;

  stepInit_   = true;
  pvStart_    = cs.pv;
  pvEnd_      = cs.pv;
  stepThetaMs_ = 0;
  stepTau63Ms_ = 0;
  stepStartMs_ = millis();

  if (method_ == MetodoTuning::RELAY) {
    heater.setMV(constrain(mvBase_ + TUNING_RELAY_H, MV_MIN_PERCENT, MV_MAX_PERCENT));
  } else {
    heater.setMV(constrain(mvBase_ + TUNING_STEP_SIZE, MV_MIN_PERCENT, MV_MAX_PERCENT));
  }
}

// ---------------------------------------------------------------------------
// tick — retorna true quando o autotuning termina
// ---------------------------------------------------------------------------
bool AutoTuner::tick(ControlState& cs, Heater& heater) {
  uint32_t now = millis();
  if ((uint32_t)(now - startMs_) > TUNING_TIMEOUT_MS) {
    finishBad(cs);   // timeout
    return true;
  }
  if (method_ == MetodoTuning::RELAY) {
    return tickRelay(cs, heater);
  }
  return tickStep(cs, heater);
}

// ---------------------------------------------------------------------------
// Relé — mede período crítico (Tu) e amplitude (a) -> Ziegler-Nichols
// ---------------------------------------------------------------------------
bool AutoTuner::tickRelay(ControlState& cs, Heater& heater) {
  float sp = cs.setpoint;
  float pv = cs.pv;

  float highMv = constrain(mvBase_ + TUNING_RELAY_H, MV_MIN_PERCENT, MV_MAX_PERCENT);
  float lowMv  = constrain(mvBase_ - TUNING_RELAY_H, MV_MIN_PERCENT, MV_MAX_PERCENT);

  // Atualiza amplitude observada.
  if (pv < relMin_) relMin_ = pv;
  if (pv > relMax_) relMax_ = pv;

  // Controle relay com histerese.
  if (relayHigh_ && pv >= (sp + TUNING_RELAY_HYST)) {
    relayHigh_ = false;
    heater.setMV(lowMv);
    recordCross();
  } else if (!relayHigh_ && pv <= (sp - TUNING_RELAY_HYST)) {
    relayHigh_ = true;
    heater.setMV(highMv);
    recordCross();
  }

  // Após cruzamentos suficientes, estima o período e aplica ZN.
  if (relCrossings_ >= 12 && relLastCrossMs_ > relFirstCrossMs_) {
    float halfPeriod = (float)(relLastCrossMs_ - relFirstCrossMs_) / (float)(relCrossings_ - 1);
    float TuSec       = (2.0f * halfPeriod) / 1000.0f;   // período crítico (s)
    float a           = (relMax_ - relMin_) * 0.5f;       // amplitude (°C)

    if (a < 0.1f || TuSec <= 0.0f) {
      finishBad(cs);
      return true;
    }
    float Ku = (4.0f * TUNING_RELAY_H) / (3.14159265f * a);  // ganho crítico (%/°C)
    TuningResult r = znRelay(Ku, TuSec);
    if (r.valid) { finishOk(cs, r); }
    else         { finishBad(cs); }
    return true;
  }
  return false;
}

// ---------------------------------------------------------------------------
// Degrau — estima FOPDT (K, tau, theta) -> Cohen-Coon
// ---------------------------------------------------------------------------
bool AutoTuner::tickStep(ControlState& cs, Heater& heater) {
  float pv = cs.pv;
  uint32_t now = millis();

  // Atraso (theta): primeiro instante em que a PV se desloca >= 0.5 °C.
  if (stepThetaMs_ == 0 && (pv > pvStart_ + 0.5f || pv < pvStart_ - 0.5f)) {
    stepThetaMs_ = (uint32_t)(now - stepStartMs_);
  }

  // Acompanha o platô final: se a PV para de variar, considera assentado.
  if (stepThetaMs_ > 0) {
    if (pv != pvEnd_) {
      pvEnd_ = pv;                 // continua seguindo a PV
      lastMoveMs_ = now;
    } else if ((uint32_t)(now - lastMoveMs_) > 5000) {
      // PV estável há 5 s -> calcula tau (63.2%) e aplica Cohen-Coon.
      float delta = pvEnd_ - pvStart_;
      if (fabsf(delta) < 0.3f || stepThetaMs_ <= 0) {
        finishBad(cs);
        return true;
      }
      float goal = pvStart_ + 0.632f * delta;
      // Encontra o tempo em que a PV alcançou 63.2% -> aproximado pelo instante
      // em que o platô foi atingido; para didática usamos a diferença atual.
      stepTau63Ms_ = (uint32_t)(now - stepStartMs_) - stepThetaMs_;

      float K     = delta / TUNING_STEP_SIZE;                       // °C por %
      float tauS  = (stepTau63Ms_ * 0.632f) / 1000.0f;              // estimativa
      float thetaS = ((float)stepThetaMs_) / 1000.0f;

      TuningResult r = cohenCoon(K, tauS, thetaS);
      if (r.valid) { finishOk(cs, r); }
      else         { finishBad(cs); }
      return true;
    }
  }
  return false;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
void AutoTuner::recordCross() {
  uint32_t now = millis();
  if (relFirstCrossMs_ == 0) relFirstCrossMs_ = now;
  relLastCrossMs_ = now;
  ++relCrossings_;
}

void AutoTuner::finishOk(ControlState& cs, const TuningResult& r) {
  cs.pid.kp = r.kp;
  cs.pid.ki = r.ki;
  cs.pid.kd = r.kd;
  cs.tuning = EstadoTuning::DONE_OK;
  cs.modo   = Modo::AUTO;
  Serial.printf("[TUNE] DONE_OK Kp=%.3f Ki=%.3f Kd=%.3f\n", r.kp, r.ki, r.kd);
}

void AutoTuner::finishBad(ControlState& cs) {
  cs.tuning = EstadoTuning::DONE_BAD;
  cs.modo   = Modo::AUTO;
  Serial.println(F("[TUNE] DONE_BAD — constantes mantidas"));
}
