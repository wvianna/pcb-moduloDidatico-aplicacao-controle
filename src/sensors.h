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
  float getPV() const { return latest_; }   // leitura bruta (°C) — sem média
  bool  isFault() const { return fault_; }     // falha de leitura (após falhas consecutivas)
  bool  readReady() const { return newSample_; }  // nova amostra neste ciclo
  void  clearReady() { newSample_ = false; }

private:
  enum class Phase { IDLE, CONVERTING };

  void startConversion();

  Phase    phase_;
  uint32_t convertStartMs_;
  float    latest_;      // última leitura válida (bruta)
  uint8_t  failCount_;  // falhas consecutivas
  bool     fault_;
  bool     newSample_;
};
