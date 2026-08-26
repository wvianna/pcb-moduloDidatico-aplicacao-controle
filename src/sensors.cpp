#include "sensors.h"
#include <OneWire.h>
#include <DallasTemperature.h>

// Instâncias do barramento OneWire e do sensor DS18B20 (estáticas, em PROGMEM
// de dados estáticos — sem alocação dinâmica no caminho periódico).
static OneWire oneWire(PIN_SENSOR_TEMP);
static DallasTemperature dallas(&oneWire);

void TempSensor::begin() {
  dallas.begin();
  dallas.setResolution(SENSOR_RESOLUTION_BITS);
  dallas.setWaitForConversion(false);  // conversão assíncrona (não-bloqueante)

  phase_         = Phase::IDLE;
  convertStartMs_ = 0;
  filtered_      = 0.0f;
  fault_         = false;
  newSample_     = false;
  idx_           = 0;
  for (unsigned i = 0; i < SENSOR_FILTER_N; ++i) buffer_[i] = 0.0f;

  startConversion();
}

void TempSensor::startConversion() {
  dallas.requestTemperatures();  // inicia conversão e retorna de imediato
  convertStartMs_ = millis();
  phase_          = Phase::CONVERTING;
}

void TempSensor::tick() {
  switch (phase_) {
    case Phase::IDLE:
      startConversion();
      break;

    case Phase::CONVERTING: {
      // Aguarda a conversão terminar sem bloquear o loop.
      if ((uint32_t)(millis() - convertStartMs_) >= SENSOR_CONVERSION_MS) {
        float t = dallas.getTempCByIndex(0);

        // Valores de erro do DS18B20 (DEVICE_DISCONNECTED_C = -127).
        if (t <= DEVICE_DISCONNECTED_C || t < -50.0f || t > 125.0f) {
          fault_     = true;
          filtered_  = 0.0f;
        } else {
          fault_ = false;
          pushSample(t);
          filtered_ = average();
          newSample_ = true;
        }
        phase_ = Phase::IDLE;  // próxima conversão inicia no próximo tick (~1 Hz)
      }
      break;
    }
  }
}

void TempSensor::pushSample(float t) {
  buffer_[idx_] = t;
  idx_ = (uint8_t)((idx_ + 1) % SENSOR_FILTER_N);
}

float TempSensor::average() const {
  float sum = 0.0f;
  for (unsigned i = 0; i < SENSOR_FILTER_N; ++i) sum += buffer_[i];
  return sum / (float)SENSOR_FILTER_N;
}
