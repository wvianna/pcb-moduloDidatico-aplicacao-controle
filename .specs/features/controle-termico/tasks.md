# Tasks: Controle Térmico com Dashboard Web (NodeMCU ESP8266)

> **Feature:** `controle-termico` · Escopo **Grande**.
> Base: `spec.md` (requisitos/critérios) + `design.md` (arquitetura) + `context.md` (decisões).
> Ordenação por **risco**: primeiro contrato/teste, depois implementação, integração e validação física.

---

## T-001 — Configuração do projeto e constantes

- **Requisitos:** FR-001 (base), FR-008 (base), FR-020 (base); prepara `NFR-005`.
- **Onde:** `platformio.ini`, `src/config.h` (novo).
- **Depende de:** nenhum.
- **Reutiliza:** nada.
- **Feito quando:** `config.h` define pinos (D2/GPIO4 sensor, D1/GPIO5 PWM, D0/GPIO16 buzzer), limiares de alarme `ALARM_ON_TEMP_C=80.0f`/`ALARM_OFF_TEMP_C=78.0f`, limites PID e rede; `platformio.ini` com `lib_deps` (ArduinoJson, OneWire, DallasTemperature) e portas `/dev/ttyUSB0`.
- **Testes:** build do alvo.
- **Gate:** `pio run` compila sem erros e o binário é gerado.

## T-002 — Modelo de dados (`state.h`)

- **Requisitos:** base para FR-005, FR-016, FR-031.
- **Onde:** `src/state.h` (novo).
- **Depende de:** T-001.
- **Reutiliza:** nada.
- **Feito quando:** define `Modo`, `EstadoAlarme`, `MetodoTuning`, `EstadoTuning`, `ParamsPID` e `ControlState` conforme `design.md` §3.
- **Testes:** host — compilar com `static_assert` de tamanho/limites; revisão de campos.
- **Gate:** `pio run`.

## T-003 — Leitura não-bloqueante do DS18B20

- **Requisitos:** FR-007, FR-009, FR-010, NFR-002, NFR-014.
- **Onde:** `src/sensors.h/.cpp` (novo).
- **Depende de:** T-002.
- **Reutiliza:** OneWire/DallasTemperature.
- **Feito quando:** inicia conversão sem bloquear, lê após `CONVERSION_TIME`, aplica filtro (ex.: média móvel), marca `sensor_fail` em timeout/CRC inválido.
- **Testes:** HOST (lógica de filtro/estado com stub) + BANCADA (valor de PV no serial).
- **Gate:** `pio run` + monitor serial mostrando PV a ~1 Hz com jitter ±100 ms.

## T-004 — Atuador PWM da resistência

- **Requisitos:** FR-008, FR-012.
- **Onde:** `src/actuators.h/.cpp` (novo).
- **Depende de:** T-002.
- **Reutiliza:** nada.
- **Feito quando:** mapeia MV 0–100% → `analogWrite` 0–1023; desliga (0) em ALARME.
- **Testes:** HOST (mapeamento) + BANCADA (medir DPWM no pino D1).
- **Gate:** `pio run` + multímetro confirma 0↔1023.

## T-005 — Buzzer não-bloqueante

- **Requisitos:** FR-013, FR-015, NFR-004.
- **Onde:** `src/actuators.h/.cpp`.
- **Depende de:** T-004.
- **Reutiliza:** nada.
- **Feito quando:** FSM `millis()` produz bip de 150 ms a cada 2 s, sem `delay()`.
- **Testes:** HOST (FSM de tempo com mock de `millis`) + BANCADA.
- **Gate:** `pio run` + cronômetro confirma 150 ms on / 2 s off (±20 ms).

## T-006 — Controlador PID custom com anti-windup

- **Requisitos:** FR-016–FR-019, FR-021, NFR-001.
- **Onde:** `src/pid.h/.cpp` (novo).
- **Depende de:** T-002.
- **Reutiliza:** nada.
- **Feito quando:** implementa positional PID com saturação [0,100], anti-windup por resgate de integral, reset de integral, P e D condicionados por flags, I sempre ativo.
- **Testes:** **HOST** (unitário): resposta a degrau, saturação, anti-windup, reset. (nível HOST)
- **Gate:** testes host passam + `pio run`.

## T-007 — Máquina de estados de alarme

- **Requisitos:** FR-011, FR-012, FR-014; NFR-011.
- **Onde:** `src/control.h/.cpp` (novo).
- **Depende de:** T-004, T-005, T-006.
- **Reutiliza:** nada.
- **Feito quando:** FSM com histerese ON ≥80 / OFF <78; em ALARME desliga a resistência e liga o buzzer.
- **Testes:** **HOST** (unitário): transições, chattering no limiar, retorno a NORMAL.
- **Gate:** testes host passam + `pio run`.

## T-008 — Supervisor de controle (agendamento determinístico)

- **Requisitos:** FR-022, FR-023, NFR-001, NFR-010.
- **Onde:** `src/control.h/.cpp`.
- **Depende de:** T-003, T-007.
- **Reutiliza:** PID (T-006).
- **Feito quando:** roda PID a cada `PID_PERIOD_MS` (100 ms), orquestra aquisição/alarme/reset de integral na troca de modo, com ownership explícito.
- **Testes:** HOST (agendamento com mock de millis) + BANCADA (período estável).
- **Gate:** `pio run` + timestamps no serial confirmam período.

## T-009 — Access Point com IP estático

- **Requisitos:** FR-001, FR-002, FR-003; Q-005.
- **Onde:** `src/web_server.h/.cpp` (novo).
- **Depende de:** T-002.
- **Reutiliza:** ESP8266WiFi.
- **Feito quando:** `softAP(ssid-MAC)`, `softAPConfig(192.168.4.1, gw, mask)`, DHCP ativo.
- **Testes:** BANCADA (conectar cliente, verificar IP/sub-rede).
- **Gate:** `pio run` + cliente associa e recebe IP `192.168.4.x`.

## T-010 — Servidor HTTP: dashboard + `/api/state`

- **Requisitos:** FR-004, FR-005, FR-031; NFR-003.
- **Onde:** `src/web_server.h/.cpp`, `src/dashboard.h` (novo).
- **Depende de:** T-009.
- **Reutiliza:** ESP8266WebServer.
- **Feito quando:** serve `/` (HTML PROGMEM) e `GET /api/state` (JSON com PV, MV, setpoint, modo, alarme, sensor_fail, PID).
- **Testes:** BANCADA (`curl`) + navegador.
- **Gate:** `curl http://192.168.4.1/api/state` retorna JSON válido.

## T-011 — Endpoint de configuração `/api/control`

- **Requisitos:** FR-006, FR-020, FR-022.
- **Onde:** `src/web_server.h/.cpp`.
- **Depende de:** T-010.
- **Reutiliza:** ArduinoJson.
- **Feito quando:** via POST JSON aplica setpoint (20–80), modo, mv manual, PID (p/i/d, enableP/D), método de tuning; valida e retorna erro sem alterar estado; escrita por flag.
- **Testes:** BANCADA (`curl` com payload válidos/inválidos).
- **Gate:** `curl -X POST` aplica e retorna `200`; payload inválido retorna `400` e estado inalterado.

## T-012 — Autotuning (relé + resposta em degrau)

- **Requisitos:** FR-024; Q-003.
- **Onde:** `src/autotune.h/.cpp` (novo), integração em `control`.
- **Depende de:** T-006, T-008, T-011.
- **Reutiliza:** PID (T-006).
- **Feito quando:** seleciona método (RELAY/STEP), executa relé ou degrau, identifica K/τ/θ ou Ku/Tu, aplica P/I/D e volta a AUTO; trata timeout/falha.
- **Testes:** HOST (cálculo das fórmulas) + BANCADA (validação da planta).
- **Gate:** testes host passam + `pio run`; BANCADA confirma constantes plausíveis.

## T-013 — Dashboard web (estética + polling)

- **Requisitos:** FR-025–FR-032, NFR-015–NFR-022.
- **Onde:** `src/dashboard.h`.
- **Depende de:** T-010, T-011.
- **Reutiliza:** nada (HTML/CSS/JS puro, PROGMEM).
- **Feito quando:** tema claro industrial, tipografia Syne + IBM Plex Mono/Sans, CSS variables, *single viewport* sem rolagem, *gauges* + gráficos de PV (20–90) e MV (0–100), setpoint, modo, slider manual, checkboxes P/D, seletor/start de autotuning, *hints* em todos; *polling* `fetch` a cada 500 ms–1 s.
- **Testes:** BANCADA (navegador, viewports) + inspeção visual.
- **Gate:** `pio run` + dashboard renderiza sem rolagem e atualiza via polling.

## T-014 — Integração final, README e validação BANCADA

- **Requisitos:** todos (regressão); NFR-005, NFR-006, NFR-007, NFR-008, NFR-011–NFR-014.
- **Onde:** `src/`, `README.md`, `platformio.ini`.
- **Depende de:** T-001 a T-013.
- **Reutiliza:** todos.
- **Feito quando:** build final do alvo, testes HOST passam, alarme/buzzer/PWM/servidor validados no BANCADA, heap/stack medidos, `README.md` atualizado (instalação, build, teste, execução).
- **Testes:** BANCADA completo em `/dev/ttyUSB0` + Wi-Fi; regressão dos critérios `CA-001`–`CA-020`.
- **Gate:** `pio run` (sem erros), testes HOST, e checklist BANCADA assinado com evidência.

---

## Entregáveis e aceite

### Arquivos de código esperados
- `src/main.cpp`, `src/config.h`, `src/state.h`, `src/sensors.h/.cpp`, `src/actuators.h/.cpp`, `src/pid.h/.cpp`, `src/autotune.h/.cpp`, `src/control.h/.cpp`, `src/web_server.h/.cpp`, `src/dashboard.h`.
- `platformio.ini` (lib_deps + portas).
- `test/host/test_core.cpp` (HOST): unitários de PID, alarme, sintonia e flags P/D (**31/31**).
- `README.md` atualizado.

### Comando de compilação
```bash
pio run                 # compila para o alvo (nodemcuv2)
pio run -e nodemcuv2    # explícito
pio test -e native      # testes HOST (se configurado)
```

### Comando de validação BANCADA
```bash
pio run -t upload --upload-port /dev/ttyUSB0
pio device monitor -p /dev/ttyUSB0
```

### Níveis de evidência
| Tipo | Onde se aplica |
|---|---|
| **HOST** | Lógica pura: PID, alarme, filtro, autotune, mapeamento PWM. |
| **SIMULADOR** | (opcional) FSM de tempo com mock de `millis`. |
| **BANCADA** | Sensor, PWM, buzzer, AP/HTTP, dashboard, reset/brownout (em `/dev/ttyUSB0`). |
| **HIL** | **Não previsto** nesta fase (decisão Q-006). |

### Critérios de aceite rastreados por ID
| Critério | Tarefa(s) | Nível |
|---|---|---|
| CA-001, CA-002 | T-009 | BANCADA |
| CA-003, CA-004, CA-005 | T-010, T-011 | BANCADA |
| CA-006, CA-007 | T-003, T-004 | BANCADA + HOST |
| CA-008 | T-003, T-010 | BANCADA |
| CA-009, CA-010, CA-011 | T-005, T-007 | BANCADA + SIMULADOR |
| CA-012, CA-015 | T-006, T-008 | HOST + BANCADA |
| CA-013, CA-014 | T-008, T-012 | BANCADA |
| CA-016–CA-019 | T-013 | BANCADA |
| CA-020 | T-013 | inspeção visual |

### Pendências, riscos residuais e responsável
- **Limiar exato de 78 °C** e **resolução do sensor** — verificar no BANCADA; centralizar em `config.h`.
- **Margem de RAM/stack** (`NFR-006`, `NFR-008`) — medir heap/stack no BANCADA pós-integração.
- **Autotuning por relé** — validar amplitude/tempo para não exceder o estado seguro.
- **Responsável pela validação física:** operador com acesso ao `/dev/ttyUSB0`.

---

## Ajustes incrementais pós-entrega (D-001..D-010)

> Executados após a T-014, conforme decisões do operador. Ver `context.md`.

| ID | Ajuste | Onde | Evidência |
|---|---|---|---|
| D-001 | Leitura **bruta** (sem média móvel) | `sensors.h/.cpp` | BANCADA (leitura direta) |
| D-002 | Erro de sensor só após 3 falhas consecutivas | `sensors`, `config.h` | BANCADA |
| D-003 | **Fail-safe**: corta a resistência se a leitura congelar 3 s | `control.cpp` | HOST + BANCADA |
| D-004 | Conversão OneWire de **800 ms** | `config.h` | BANCADA |
| D-005 | Polling do dashboard em **1 s** | `dashboard.h` | BANCADA (navegador) |
| D-006 | **Multicliente** (AP até 4 clientes) | `web_server.cpp` | BANCADA (2+ clientes) |
| D-007 | **Clock 160 MHz** | `platformio.ini` | Build + boot `CPU=160 MHz` |
| D-008 | **Saúde do MCU** no cabeçalho | `main.cpp`, `state.h`, `dashboard.h` | BANCADA (navegador) |
| D-009 | `/api/state` em **buffer estático** | `web_server.cpp` | HOST (JSON válido) + BANCADA |
| D-010 | Controles **P/D** verificados (**I** sempre ativa) | `pid`, `dashboard.h` | HOST (31/31) + serial `[CTRL]` |
