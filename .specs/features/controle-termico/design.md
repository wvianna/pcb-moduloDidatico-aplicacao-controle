# Design: Controle Térmico com Dashboard Web (NodeMCU ESP8266)

> **Feature:** `controle-termico` · Escopo **Grande**.
> **Fonte de contrato:** `.specs/features/controle-termico/spec.md` (requisitos `FR-###`/`NFR-###`) e `context.md` (decisões `Q-001`–`Q-006`).
> **Alvo:** NodeMCU V2 (ESP12E), Arduino Core ESP8266 ≥ 3.0.0, PlatformIO.
> **Decisões aplicadas:** PID custom (Q-002), autotuning relé+degrau (Q-003), polling fetch (Q-004), SSID por MAC (Q-005), BANCADA em `/dev/ttyUSB0` (Q-006).

---

## 1. Visão geral da arquitetura

O firmware usa **um único loop cooperativo** (`loop()` do Arduino), sem RTOS de tarefas e sem `delay()` bloqueante. Toda temporização é feita por **máquinas de estado baseadas em `millis()`**, com o servidor HTTP atendido de forma não-bloqueante. Esta escolha minimiza RAM, stack e complexidade de concorrência, atendendo `NFR-001`, `NFR-003`, `NFR-007` e `NFR-009`.

```mermaid
flowchart TD
    subgraph "Main loop (cooperative, millis())"
        A[handleClient<br/>HTTP não-bloqueante] --> B[Aquisição DS18B20<br/>FSM 1 Hz]
        B --> C[Filtro PV]
        C --> D[Alarme<br/>FSM histerese]
        D --> E[Buzzer<br/>FSM 150ms/2s]
        E --> F[Supervisor de controle<br/>PID 100–200ms]
        F --> G[Atuador PWM<br/>0–100%]
    end

    SSD[(SSID por MAC<br/>constituição)] --> A
    H[Endpoint /api/state<br/>(GET)] --> A
    I[Endpoint /api/control<br/>(POST)] --> A
    J[Dashboard PROGMEM<br/>polling fetch] --> A
```

### Módulos

| Módulo | Responsabilidade | Requisitos |
|---|---|---|
| `config.h` | Constantes: pinos, limiares de alarme, limites PID, rede. | — |
| `state.h` | Modelo de dados compartilhado (`ControlState`). | — |
| `sensors` | Leitura não-bloqueante do DS18B20 + filtro. | FR-007, FR-009, FR-010, NFR-002, NFR-014 |
| `actuators` | PWM da resistência (0–100%) + buzzer não-bloqueante. | FR-008, FR-012, FR-013, FR-015 |
| `pid` | Controlador PID custom com anti-windup. | FR-016–FR-021 |
| `autotune` | Sintonia por relé e por resposta em degrau. | FR-024 |
| `control` | Supervisor: máquina de alarme + agendamento determinístico. | FR-011–FR-014, NFR-001, NFR-004 |
| `web_server` | AP + DHCP + HTTP (dashboard e endpoints JSON). | FR-001–FR-006, FR-031, NFR-003 |
| `dashboard` | Página web embutida (PROGMEM) com estética definida. | FR-025–FR-032, NFR-015–NFR-022 |

## 2. Estrutura de arquivos proposta

```text
src/
├── main.cpp          # setup() + loop() (orquestra todos os módulos)
├── config.h          # pinos, limites, constantes de rede e alarme
├── state.h           # struct ControlState + enum Modo/EstadoAlarme
├── sensors.h/.cpp    # DS18B20 não-bloqueante + filtro
├── actuators.h/.cpp  # PWM heater + buzzer
├── pid.h/.cpp        # PID custom
├── autotune.h/.cpp   # relé + resposta em degrau
├── control.h/.cpp    # supervisor + máquina de alarme + loop timing
├── web_server.h/.cpp # AP + HTTP + endpoints JSON
└── dashboard.h       # HTML/CSS/JS embutido (PROGMEM)
```

> Em `PlatformIO` os `.h`/`.cpp` de `src/` são compilados automaticamente; **não** criar `lib/` própria para evitar camadas desnecessárias (princípio da constituição).

## 3. Modelo de dados (`state.h`)

```cpp
enum class Modo { MANUAL, AUTO, TUNING };
enum class EstadoAlarme { NORMAL, ALARME };
enum class MetodoTuning { RELAY, STEP };
enum class EstadoTuning { IDLE, RUNNING, DONE_BAD, DONE_OK };

struct ParamsPID {
  float kp, ki, kd;      // ganhos
  bool  enableP, enableD; // P e D habilitáveis; I sempre ativo
};

struct ControlState {
  float pv;               // temperatura (°C), 20–90
  float mv;               // potência (%), 0–100
  float setpoint;         // 20–80 °C
  Modo  modo;
  ParamsPID pid;
  MetodoTuning metodo;
  EstadoTuning tuning;
  EstadoAlarme alarm;
  bool  sensor_fail;
  uint32_t ts;            // timestamp (millis) da última atualização
};
```

Acesso compartilhado entre `loop()` e o servidor HTTP: **todas as escritas ocorrem no `loop()`**; o `handleClient()` apenas **lê** o estado (constância). No caso de requisição que altera parâmetros, a escrita é feita por um **flag** (`atomic`/`volatile` + atualização no próximo ciclo), evitando *data race* (`NFR-010`). Não se usa `malloc`/`String` no caminho periódico (`NFR-007`).

## 4. Máquinas de estado

### 4.1 Alarme (histerese — Decisão Q-001)

| Atual | Condição | Ação | Próximo |
|---|---|---|---|
| NORMAL | `pv >= ALARM_ON_TEMP_C (80.0)` | desliga resistência, ativa buzzer | ALARME |
| ALARME | `pv >= ALARM_OFF_TEMP_C (78.0)` | mantém buzzer, resistência desligada | ALARME |
| ALARME | `pv < ALARM_OFF_TEMP_C (78.0)` | desativa buzzer | NORMAL |

Constantes únicas em `config.h`: `ALARM_ON_TEMP_C = 80.0f`, `ALARM_OFF_TEMP_C = 78.0f`. Qualquer divergência firmware/dashboard é um bug.

### 4.2 Modo do controlador

| Atual | Evento | Ação | Próximo |
|---|---|---|---|
| MANUAL | seleciona AUTO | reseta integral; PID assume MV | AUTO |
| AUTO | seleciona MANUAL | **reseta integral** (anti-windup), PID pausado, MV = valor manual | MANUAL |
| AUTO/MANUAL | inicia TUNING | PID pausado, acumula amostras | TUNING |
| TUNING | método conclui | aplica P/I/D; PID retoma | AUTO |
| TUNING | falha/timeout | mantém parâmetros; volta a AUTO com aviso | AUTO |

### 4.3 Buzzer (não-bloqueante — Decisão/requisito FR-013/FR-015)

FSM baseada em `millis()`: `OFF→ON (150ms)→OFF (2000ms)→...`. Nenhum `delay()`; usa `buzzer_last_ms` e duração, verificados a cada ciclo. Observância de `NFR-004` (±20 ms).

### 4.4 Aquisição do sensor (não-bloqueante)

O DS18B20 em 12 bits demanda ~750 ms de conversão. Para **não** bloquear PID/HTTP, a leitura é dividida:

1. `sensor.requestTemperatures()` inicia conversão (retorna de imediato);
2. registro `sensor_start_ms`;
3. a cada ciclo, se `millis() - sensor_start_ms >= CONVERSION_TIME`, lê o valor e atualiza `pv`.

Assim a aquisição ocorre a ~1 Hz (`NFR-002`) sem bloquear o loop. (*Alternativa:* resolução de 9 bits, ~94 ms, se a resposta do processo for lenta — decisão reservada ao teste de BANCADA.)

## 5. Contexto de execução e orçamento de tempo

Todo o trabalho ocorre no `loop()`. Ordem sugerida e orçamento:

| Etapa | Frequência | Custo estimado | Deadline/métrica |
|---|---|---|---|
| `handleClient()` | contínuo | ~1–5 ms | não-bloqueante |
| Aquisição DS18B20 | 1 Hz | ~0 ms (assíncrono) | período 1 s, jitter ±100 ms |
| Filtro PV | 1 Hz | <0.1 ms | — |
| Alarme + Buzzer | contínuo | <0.1 ms | ±20 ms (buzzer) |
| **Cálculo PID** | **100–200 ms** | <0.5 ms | período determinístico (NFR-001) |
| Atuador PWM | 100–200 ms | <0.1 ms | 0–100% |

O **PID** é executado a intervalos fixos via `if (millis() - lastPid >= PID_PERIOD_MS)` (PID_PERIOD_MS = 100). Isso atende `NFR-001`. O servidor HTTP usa `ESP8266WebServer` (leve) para não competir por RAM com a rede (`NFR-003`).

## 6. Antiwindup e saturação (Decisão Q-002)

PID **custom**, forma **positional** com *anti-windup por saturação e resgate da integral*:

- `error = setpoint - pv`;
- `P = kp * error` (só se `enableP`);
- `I += ki * error * dt`; **clamp** da integral em `[-I_max, +I_max]` (ex.: `I_max = 100/ki` com folga);
- `D = kd * (pv - pv_prev) / dt` (derivada sobre a PV para evitar *derivative kick*); só se `enableD`;
- `out = P + I + D`; **clamp** em `[0, 100]`;
- **resgate da integral**: se `out` saturou, reduzir `I` pelo excesso (`I -= (out_sat - out_raw)`), evitando *windup*;
- `I` **sempre ativo** (sem checkbox), P e D condicionados aos flags (`FR-017`, `FR-018`, `FR-019`).

Ao entrar em MANUAL ou TUNING, `I = 0` e `pv_prev = pv` (reset de integral, `FR-022`).

## 7. Autotuning (Decisão Q-003)

### 7.1 Método de relé (Ziegler–Nichols por relay)

- Aplica sinal *relay* com histerese em torno do setpoint (ex.: +u / -u invertendo quando `pv` cruza ±hs).
- Mede o **período crítico** `Tu` (intervalo entre cruzamentos) e a **amplitude** `a` da oscilação → ganho crítico `Ku = 4h/(π a)`.
- Aplica Ziegler–Nichols: `Kp=0.6Ku`, `Ti=0.5Tu` → `Ki=Kp/Ti`, `Td=0.125Tu` → `Kd=Kp*Td`.
- Proteções: limite de tempo (ex.: 120 s) e contador de cruzamentos; se não oscilar, `DONE_BAD`.

### 7.2 Método de resposta em degrau (Cohen–Coon)

- Aguarda regime inicial, aplica degrau de MV (ex.: +10%), registra resposta da PV (FOPDT).
- Extrai ganho estático `K`, atraso `θ` e constante `τ` → aplica Cohen–Coon.

Ambos atualizam `ControlState::pid` ao concluir e voltam a AUTO (`FR-024`).

## 8. Rede e servidor HTTP

### 8.1 Access Point (FR-001–FR-003, Q-005)

```cpp
const char* ssid = ("ESP8266-" + String(ESP.getChipId(), HEX)).c_str();
WiFi.softAP(ssid);                      // sem senha
IPAddress ip(192,168,4,1), gw(192,168,4,1), mask(255,255,255,0);
WiFi.softAPConfig(ip, gw, mask);        // IP estático + DHCP na sub-rede
```

SSID **derivado do MAC/chip ID** (`ESP8266-XXXXXX`).

### 8.2 Endpoints e contrato JSON

| Método | Rota | Descrição | Corpo/Query |
|---|---|---|---|
| GET | `/` | Dashboard HTML | — |
| GET | `/api/state` | Estado atual (leitura) | — |
| POST | `/api/control` | Aplica configuração | JSON |

Exemplo `GET /api/state`:

```json
{
  "pv": 25.5, "mv": 40.0, "setpoint": 50.0,
  "mode": "auto", "alarm": false, "sensor_fail": false,
  "pid": { "p": 1.5, "i": 0.2, "d": 0.05, "enableP": true, "enableD": false },
  "ts": 1234567
}
```

`POST /api/control` (corpo) — campos opcionais:

```json
{
  "setpoint": 55.0,
  "mode": "manual",
  "mv": 35.0,
  "pid": { "p": 1.5, "i": 0.2, "d": 0.05, "enableP": true, "enableD": false },
  "tuning": { "method": "relay" }
}
```

Regras: `setpoint` saturado em 20–80; `mv` em 0–100; valores inválidos → resposta `400` com `{"error":"..."}` e **estado inalterado** (`FR-006`). Escritas aplicadas por *flag* e confirmadas no próximo ciclo do `loop()`.

### 8.3 Atualização em tempo real (Q-004)

O dashboard faz **`fetch('/api/state')` a cada ~500 ms–1 s** (polling assíncrono). Nota: como a página é servida do mesmo MCU, o *polling* é a opção mais leve — não há push de WebSocket (`NFR-003`, `FR-031`).

## 9. IHM — Direção estética

> Atende `NFR-015`–`NFR-022`. Direção: **instrumento de bancada de laboratório / estética industrial-precisa**, tema **claro**, coeso e memorável — sem estética genérica de IA.

- **Tipografia (NFR-015):** display **“Syne”** (títulos/identidade) + **“IBM Plex Mono”** para leituras numéricas de PV/MV (caráter técnico de medidor) + **“IBM Plex Sans”** para corpo/controles. Evita Inter/Roboto/Arial/Space Grotesk.
- **Cor e tema (NFR-016):** fundo **osso/marfim** (`#F6F2EB`), dominante **grafite** (`#1B1B1B`), acento **âmbar queimado** (`#E5600C`) remetendo ao calor (alarme), secundário **teal** (`#0F7D6B`) para estado normal/ok. Via CSS variables.
- **Movimento (NFR-017):** *page load* com *staggered reveals* (`animation-delay`); ponteiro do *gauge* em transição suave; estados de *hover* nos controles; tudo CSS-first.
- **Composição espacial (NFR-018):** grids assimétricos com *cards* de PV/MV em destaque, sobreposição de *gauge* e gráfico; fluxo diagonal sutil nos cabeçalhos.
- **Fundo e detalhes (NFR-019):** gradiente radial ameno atrás dos *gauges*, textura de ruído/granulado sutil, sombras dramáticas nos *cards* e bordas decorativas finas.
- **Acessibilidade (NFR-020):** contraste AA, *hints* legíveis, foco visível via teclado.
- **Sem estética genérica (NFR-021):** sem gradientes roxos sobre branco, sem layout previsível de dashboard padrão.
- **Desempenho (NFR-022):** sem rolagem (*single viewport*) e animações a 60 fps com dados atualizando a cada 500 ms–1 s.

**Componentes da IHM:** header (título/modo/alarme), card PV (numérico + *gauge* radial + gráfico), card MV (numérico + *gauge* + gráfico), painel de controle (setpoint, slider manual, checkboxes P/D, seletor de modo, seletor/start de autotuning), com *hints* em todos (`FR-030`).

## 10. Recursos e memória

- **Flash:** dashboard embutido em `PROGMEM` (~15–25 KB HTML), código ~150–200 KB → bem abaixo de 4 MB (`NFR-005`).
- **RAM (NFR-006):** uso de buffers estáticos, `String` evitada no caminho periódico; heap alvo reservado para servidor HTTP. Margem `A CONFIRMAR` após o build.
- **Stack (NFR-008):** sem recursão; botão de *handleClient* e PID leves; estimar por `ESP.getFreeHeap()` no BANCADA.
- **Alocação dinâmica (NFR-007):** proibida no caminho de aquisição/PID e no atendimento de requisições; usar buffers fixos.

## 11. Dependências (`platformio.ini`)

```ini
[env:nodemcuv2]
platform = espressif8266
board = nodemcuv2
framework = arduino
lib_deps =
    bblanchon/ArduinoJson@^7.0.0
    paulstoffregen/OneWire@^2.3.8
    milesburton/DallasTemperature@^3.9.0
board_build.filesystem =
```
> Adicionar `lib_deps` ao `platformio.ini` existente. Monitor serial de upload: `/dev/ttyUSB0` (`env:upload_port = /dev/ttyUSB0`, `monitor_port = /dev/ttyUSB0`).

## 12. Alternativas rejeitadas

| Alternativa | Motivo da rejeição |
|---|---|
| ESPAsyncWebServer | Maior uso de RAM; complexidade desnecessária para polling. |
| RTOS de tarefas (tasks) | Custo de stack/prioridade; loop cooperativo é suficiente. |
| WebSocket | Maior RAM/complexidade; requisito de polling atende (Q-004). |
| Biblioteca PID externa | Menos controle de anti-windup/saturação (Q-002). |
| SSID fixo | Menos reprodutível entre dispositivos; MAC garante unicidade (Q-005). |
| HIL | Aparato não disponível; validação em BANCADA (Q-006). |

## 13. Riscos residuais e validação

- **Sobrecarga do ESP8266** (rede+HTTP+PID+aquisição): mitigada por polling leve e buffers estáticos; validar heap/tempo no BANCADA.
- **Conversão do DS18B20 (750 ms):** tratada de forma assíncrona para não bloquear; se causar atraso na resposta, reduzir resolução.
- **Autotuning por relé** pode excitar a planta além do aceitável: limitar amplitude/tempo e exigir estado seguro (resistência cortada acima de 80 °C).
- **Validação:** BANCADA em `/dev/ttyUSB0` + Wi-Fi. Testes de lógica pura (PID, alarme, filtro, autotune) em **HOST** via Python/pytest ou Arduino Unit. Nada de HIL nesta fase (Q-006).
