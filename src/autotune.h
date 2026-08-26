#pragma once
// autotune.h — Sintonia automática (relé + resposta em degrau; Decisão Q-003).
// Opera no Modo::TUNING, excita a planta por PWM e aplica P/I/D ao concluir.

#include <Arduino.h>
#include "config.h"
#include "state.h"
#include "actuators.h"
#include "tuning_math.h"

class AutoTuner {
public:
  void begin(ControlState& cs, Heater& heater);  // chamado ao entrar em TUNING
  // Chamado a cada loop durante Modo::TUNING. Retorna true quando termina
  // (DONE_OK ou DONE_BAD), momento em que o supervisor deve voltar a AUTO.
  bool tick(ControlState& cs, Heater& heater);

private:
  void finishOk(ControlState& cs, const TuningResult& r);
  void finishBad(ControlState& cs);
  void recordCross();
  bool tickRelay(ControlState& cs, Heater& heater);
  bool tickStep(ControlState& cs, Heater& heater);

  MetodoTuning method_;

  uint32_t startMs_;
  float    mvBase_;

  // relatômetro (relé)
  bool     relayHigh_;
  float    relMin_, relMax_;
  uint32_t relFirstCrossMs_, relLastCrossMs_;
  uint16_t relCrossings_;

  // degrau
  bool     stepInit_;
  float    pvStart_, pvEnd_;
  float    stepThetaMs_;
  float    stepTau63Ms_;
  uint32_t stepStartMs_;
  uint32_t lastMoveMs_;
};
