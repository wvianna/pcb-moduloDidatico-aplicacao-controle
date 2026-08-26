#pragma once
// sensors.h — Leitura não-bloqueante do DS18B20 + filtro da PV.
//
// A conversão de ~750 ms é tratada de forma assíncrona (FSM por millis()) para
// NÃO bloquear o loop de controle/PID nem o servidor HTTP (NFR-002, NFR-003).

#include <Arduino.h>
#include "config.h"

class TempSensor {
public:
  void begin();                 // inicializa barramento e resolução
  void tick();                  // chame a cada loop(); gerencia a conversão
  float getPV() const { return filtered_; }   // temperatura filtrada (°C)
  bool  isFault() const { return fault_; }     // falha de leitura
  bool  readReady() const { return newSample_; }  // nova amostra neste ciclo
  void  clearReady() { newSample_ = false; }

private:
  enum class Phase { IDLE, CONVERTING };

  void startConversion();
  void pushSample(float t);
  float average() const;

  Phase    phase_;
  uint32_t convertStartMs_;
  float    filtered_;
  bool     fault_;
  bool     newSample_;
  float    buffer_[SENSOR_FILTER_N];  // média móvel — sem alocação dinâmica
  uint8_t  idx_;
};
