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

  phase_          = Phase::IDLE;
  convertStartMs_ = 0;
  latest_         = 0.0f;
  failCount_      = 0;
  fault_          = false;
  newSample_      = false;

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
        float t = dallas.getTempCByIndex(0);   // leitura bruta (sem média)

        // Valores de erro do DS18B20 (DEVICE_DISCONNECTED_C = -127).
        bool ok = !(t <= DEVICE_DISCONNECTED_C || t < -50.0f || t > 125.0f);
        if (ok) {
          latest_    = t;
          failCount_ = 0;
          fault_     = false;
        } else {
          if (failCount_ < 255) ++failCount_;
          fault_ = (failCount_ >= SENSOR_FAIL_THRESHOLD);  // erro só após falhas consecutivas
        }
        newSample_ = true;
        phase_ = Phase::IDLE;  // próxima conversão inicia no próximo tick (~1 Hz)
      }
      break;
    }
  }
}
