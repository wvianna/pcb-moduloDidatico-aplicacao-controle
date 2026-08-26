# Contexto — Decisões de especificação (controle-termico)

> Registro das respostas às perguntas bloqueadoras da `spec.md` (`Q-001` a `Q-006`). As decisões aqui registradas **alteram o contrato** e foram refletidas nos requisitos e critérios de aceite.

## Decisões registradas

| ID | Pergunta | Decisão | Refletido em |
|---|---|---|---|
| Q-001 | Semântica do limiar de desativação do alarme | Alarme ativa em **≥ 80 °C**; **cessa apenas quando a temperatura ficar < 78 °C** (histerese de 2 °C). Substitui o limiar de 75 °C do `descritivo.txt`. | §4.1 (tabela e nota), FR-011/FR-014, CA-009/CA-010 |
| Q-002 | Implementação do PID | **Custom** com controle de anti-windup e saturação (sem dependência de biblioteca externa). | FR-016, FR-021, design.md (a produzir) |
| Q-003 | Tipo de autotuning | **Dois métodos selecionáveis**: (a) **relé** (identificação de ganho/período críticos, ex. Ziegler-Nichols por relay) e (b) **resposta em degrau** (ex. Cohen-Coon). | FR-024 |
| Q-004 | Comunicação dashboard | **Polling assíncrono** via `fetch` periódico; intervalo definido em **1 s** (alinhado à aquisição), sem WebSocket. | FR-031 |
| Q-005 | SSID do Access Point | **Derivado do MAC** (ex.: `ESP8266-1A2B3C`), mantendo a constituição atual. | FR-001, §3 Rede |
| Q-006 | Validação física | Validação em **BANCADA**: ESP8266 conectado em `/dev/ttyUSB0`; rede Wi-Fi disponível para os testes. Sem automação HIL nesta fase. | §9 Premissas |

## Decisões incrementais (durante a implementação)

| ID | Decisão | Refletido em |
|---|---|---|
| D-001 | **Leitura bruta do sensor** — removida a média móvel; o DS18B20 é lido e apresentado diretamente. | FR-009, sensors |
| D-002 | **Erro de sensor após falhas consecutivas** (`SENSOR_FAIL_THRESHOLD = 3`) para evitar falso alarme por glitch do OneWire. | FR-010, config.h |
| D-003 | **Fail-safe de sensor**: se nenhuma amostra chegar em `SENSOR_TIMEOUT_MS` (3 s), a resistência é desligada e `SENSOR_FAIL` é marcado (proteção contra superaquecimento com leitura congelada). | FR-033, NFR-023, control.cpp |
| D-004 | **Tempo de conversão OneWire** de `SENSOR_CONVERSION_MS = 800 ms` (12 bits ≈ 750 ms + margem). | config.h |
| D-005 | **Polling do dashboard em 1 s** (antes 500 ms), alinhado à aquisição de 1 Hz. | FR-031, NFR-022, dashboard.h |
| D-006 | **Multicliente**: AP aceita até `AP_MAX_CONNECTIONS` (4) e o `ESP8266WebServer` atende requisições concorrentes. | FR-003, web_server.cpp |
| D-007 | **Clock a 160 MHz** (`board_build.f_cpu = 160000000L`), confirmado no log de boot via `ESP.getCpuFreqMHz()`. | NFR-025, platformio.ini |
| D-008 | **Saúde do MCU no cabeçalho** do dashboard (CPU, carga/idle %, heap + fragmentação, maior bloco, flash/sketch, uptime, clientes Wi-Fi), atualizada a cada 1 s. | FR-034, NFR-024, main.cpp, dashboard.h |
| D-009 | **`/api/state` em buffer estático** (`snprintf`) sem alocação dinâmica, para evitar fragmentação de heap (causa do travamento do dashboard). | FR-005, NFR-007, web_server.cpp |
| D-010 | **Controles P/D verificados e robustos**: flags respeitadas pelo PID (testes HOST); `updateUI` não sobrescreve checkboxes durante a interação; log `[CTRL]`/`enP`/`enD` no serial para diagnóstico. A **integral permanece sempre ativa** (sem checkbox, por design). | FR-017/FR-018/FR-019, test/host |

## Observações

- A histerese de **2 °C** (80 °C on / 78 °C off) foi escolhida pelo operador para reduzir o risco de *chattering* em torno do limite.
- O limiar de desativação deve ser implementado como constante única no código (ex.: `ALARM_OFF_TEMP_C = 78.0f`), evitando divergência entre firmware e dashboard.
- `/dev/ttyUSB0` será a porta de upload/serial utilizada; o firmware permanece acessível via AP para os testes de dashboard.
- **Segurança:** a leitura do DS18B20 pode refletir o ambiente (não a superfície aquecida) — validar contato térmico; o alarme depende do sensor medir o ponto quente.

