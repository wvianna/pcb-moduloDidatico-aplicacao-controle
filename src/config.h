#pragma once
// config.h — Constantes centrais do sistema de controle térmico (NodeMCU ESP8266)
//
// Todas as constantes de pinos, limiares de alarme, limites do PID e da rede
// ficam centralizadas aqui para evitar divergência entre firmware e dashboard.

// ---------------------------------------------------------------------------
// Mapeamento de I/O (conforme constitution.md / descritivo.txt)
// NodeMCU V2 (ESP12E). Pinos referenciados por GPIO (números do ESP8266).
// ---------------------------------------------------------------------------
#define PIN_SENSOR_TEMP  4   // D2  / GPIO4 — DS18B20 (OneWire)
#define PIN_HEATER_PWM   5   // D1  / GPIO5 — resistência de aquecimento (PWM 10 bits)
#define PIN_BUZZER       16  // D0  / GPIO16 — buzzer (on/off; GPIO16 NÃO suporta PWM)

// ---------------------------------------------------------------------------
// Sensor de temperatura (DS18B20)
// ---------------------------------------------------------------------------
#define SENSOR_CONVERSION_MS   1000  // tempo mínimo de conversão do DS18B20 (12 bits ~750 ms + margem)
#define SENSOR_RESOLUTION_BITS 12   // resolução do DS18B20
#define SENSOR_FILTER_N        1    // amostras da média móvel (filtro da PV)
#define SENSOR_TIMEOUT_MS      3000 // falha de segurança se nenhuma amostra chegar em 3 s
#define SENSOR_FAIL_THRESHOLD  3    // nº de falhas consecutivas para marcar erro de sensor

// Faixas operacionais
#define PV_MIN_C      20.0f   // limite inferior de exibição da PV
#define PV_MAX_C      90.0f   // limite superior de exibição da PV
#define SETPOINT_MIN_C 20.0f  // setpoint ajustável mín.
#define SETPOINT_MAX_C 80.0f  // setpoint ajustável máx.

// ---------------------------------------------------------------------------
// Alarme com histerese (Decisão Q-001)
// Ativa em >= ALARM_ON_TEMP_C e cessa quando a temperatura fica < ALARM_OFF_TEMP_C.
// ---------------------------------------------------------------------------
#define ALARM_ON_TEMP_C   80.0f   // liga alarme (>=)
#define ALARM_OFF_TEMP_C  78.0f   // desliga alarme (<) — histerese de 2 °C

// Cadência do buzzer (non-blocking, via millis())
#define BUZZER_ON_MS      150
#define BUZZER_PERIOD_MS  2000   // bip de 150 ms a cada 2 s

// ---------------------------------------------------------------------------
// Atuador PWM (resistência) — resolução 10 bits
// ---------------------------------------------------------------------------
#define PWM_MAX           1023   // resolução de 10 bits (0 a 1023)
#define MV_MIN_PERCENT    0.0f
#define MV_MAX_PERCENT    100.0f

// ---------------------------------------------------------------------------
// Controlador PID (Decisão Q-002) — custom com anti-windup
// ---------------------------------------------------------------------------
#define PID_PERIOD_MS     100    // intervalo determinístico do PID (100 ms)
#define PID_KI_MAX        100.0f // limite do termo integral (anti-windup)

// Ganhos padrão (sobrescritos em runtime / autotuning)
#define PID_DEFAULT_KP    1.1f
#define PID_DEFAULT_KI    0.2f
#define PID_DEFAULT_KD    0.05f

// ---------------------------------------------------------------------------
// Autotuning (Decisão Q-003)
// ---------------------------------------------------------------------------
#define TUNING_TIMEOUT_MS     120000  // limite de tempo (relé)
#define TUNING_RELAY_H        10.0f   // amplitude do relé (%)
#define TUNING_RELAY_HYST     2.0f    // histerese do relé (°C)
#define TUNING_STEP_SIZE      10.0f   // degrau de MV (%) — método Cohen-Coon

// ---------------------------------------------------------------------------
// Rede / Access Point (Q-005 — SSID derivado do MAC)
// ---------------------------------------------------------------------------
#define AP_IP_0  192
#define AP_IP_1  168
#define AP_IP_2  4
#define AP_IP_3  1
#define AP_GW_0  192
#define AP_GW_1  168
#define AP_GW_2  4
#define AP_GW_3  1
#define AP_MASK_0 255
#define AP_MASK_1 255
#define AP_MASK_2 255
#define AP_MASK_3 0
#define AP_SSID_PREFIX "ESP8266-"   // prefixo do SSID (sufixo = chip ID hex)
#define AP_SOFTAP_CHANNEL  1
#define AP_MAX_CONNECTIONS 4

// ---------------------------------------------------------------------------
// Servidor HTTP / dashboard
// ---------------------------------------------------------------------------
#define POLLING_INTERVAL_MS  1000   // intervalo de polling do dashboard (fetch)
#define HTTP_SERVER_PORT     80
