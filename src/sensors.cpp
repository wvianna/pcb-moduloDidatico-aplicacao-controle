#include "sensors.h"
#include <OneWire.h>
#include <DallasTemperature.h>

// Instâncias do barramento OneWire e do sensor DS18B20 (estáticas, em PROGMEM
// de dados estáticos — sem alocação dinâmica no caminho periódico).
static OneWire oneWire(PIN_SENSOR_TEMP);
static DallasTemperature dallas(&oneWire);

void TempSensor::begin() {
  dallas.begin();
  dallas.setWaitForConversion(false);  // conversão assíncrona (não-bloqueante)

  // Varredura do barramento OneWire: localiza o DS18B20 e seu endereço ROM.
  deviceFound_ = false;
  int n = dallas.getDeviceCount();
  if (n > 0 && dallas.getAddress(addr_, 0)) {
    deviceFound_ = true;
    dallas.setResolution(addr_, SENSOR_RESOLUTION_BITS);
    Serial.printf("[SENSOR] OneWire devices=%d | addr=%02X%02X%02X%02X%02X%02X%02X%02X\n",
                  n, addr_[0], addr_[1], addr_[2], addr_[3],
                  addr_[4], addr_[5], addr_[6], addr_[7]);
  } else {
    Serial.printf("[SENSOR] ERRO de varredura: nenhum DS18B20 encontrado (devices=%d)\n", n);
  }

  phase_          = Phase::IDLE;
  convertStartMs_ = 0;
  latest_         = 0.0f;
  failCount_      = 0;
  fault_          = !deviceFound_;
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
        float t = DEVICE_DISCONNECTED_C;
        if (deviceFound_) t = dallas.getTempC(addr_);   // lê pelo endereço (bruto)

        // Valores de erro do DS18B20 (DEVICE_DISCONNECTED_C = -127).
        bool ok = !(t <= DEVICE_DISCONNECTED_C || t < -50.0f || t > 125.0f);
        if (ok) {
          latest_    = t;
          failCount_ = 0;
          if (fault_) Serial.printf("[SENSOR] OK recuperado: PV=%.2f\n", t);
          fault_ = false;
        } else {
          if (failCount_ < 255) ++failCount_;
          bool novo = (failCount_ >= SENSOR_FAIL_THRESHOLD);
          if (novo && !fault_) Serial.printf("[SENSOR] FALHA: t=%.1f (failCount=%u) — ver fiação/pull-up\n", t, failCount_);
          fault_ = novo;  // erro só após falhas consecutivas
        }
        newSample_ = true;
        phase_ = Phase::IDLE;  // próxima conversão inicia no próximo tick (~1 Hz)
      }
      break;
    }
  }
}
