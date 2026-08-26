# HANDSOFF — controle-termico

> Handoff para o próximo agente/operador. Feature `controle-termico` (NodeMCU ESP8266).

## Estado atual

Implementação **completa** e integrada. Firmware compila para o alvo a **160 MHz**, **31/31** testes HOST passam e foi **gravado** em `/dev/ttyUSB0`. Em operação: leitura **bruta** do sensor, loop estável, controles **P/D** funcionais, **fail-safe** de sensor e **saúde do MCU** no cabeçalho do dashboard.

## Objetivo restante

Validar em BANCADA as funcionalidades que exigem rede/AP multicliente, condição de sobreaquecimento e medição física (PWM, alarme a 80 °C, autotuning em planta).

## Arquivos relevantes

- `src/main.cpp`, `src/config.h`, `src/state.h`
- `src/sensors.h/.cpp`, `src/actuators.h/.cpp`, `src/pid.h/.cpp`
- `src/alarm.h/.cpp`, `src/control.h/.cpp`
- `src/autotune.h/.cpp`, `src/tuning_math.h`
- `src/web_server.h/.cpp`, `src/dashboard.h`
- `test/host/test_core.cpp`, `platformio.ini`, `README.md`
- Especificação/design/tarefas: `.specs/features/controle-termico/`

## Decisões tomadas

- **Q-001..Q-006** e **D-001..D-010** (ver `context.md`): alarme 80/78 °C; PID custom; autotuning relé+degrau; polling **1 s**; SSID por MAC; BANCADA `/dev/ttyUSB0`; leitura **bruta**; fail-safe 3 s; multicliente (4); **160 MHz**; saúde do MCU; buffer estático no `/api/state`; **I sempre ativa**.

## Comandos já executados (com resultado)

| Comando | Resultado |
|---|---|
| `pio run` | ✅ Build OK — Flash ~32,6% / RAM ~36,7% (160 MHz) |
| `g++ ... test/host/test_core.cpp src/pid.cpp src/alarm.cpp ...` | ✅ **31/31** checks |
| `pio run -t upload --upload-port /dev/ttyUSB0` | ✅ gravado, hash verificado |
| `pio device monitor -p /dev/ttyUSB0 -b 115200` | ✅ `[STATUS] PV=… sensor=OK enP=1 enD=0`; boot `CPU=160 MHz` |

## Rastreabilidade consolidada (requisito → critério → evidência)

| Requisito(s) | Critério(s) | Nível | Evidência / Status |
|---|---|---|---|
| FR-001, FR-002, FR-003 | CA-001, CA-002 | BANCADA | **PENDENTE** (conectar cliente ao AP; verificar IP e DHCP) |
| FR-004, FR-005, FR-006 | CA-003, CA-004, CA-005 | BANCADA + HOST | **PARC** — JSON validado em HOST (254–294 chars); render/endpoints via rede **PENDENTE** |
| FR-007, NFR-002 | CA-006 | BANCADA | **PASS** — PV varia no serial (~1 Hz, jitter ok) |
| FR-008 | CA-007 | BANCADA | **PENDENTE** (medir PWM em D1: 0↔1023) |
| FR-009 (bruto) | — | BANCADA | **PASS** — leitura bruta confirmada |
| FR-010, FR-033, NFR-023 | CA-008, CA-021 | HOST/BANCADA | **PASS (lógica)** — fail-safe e falhas consecutivas; BANCADA **PENDENTE** (desconectar/congelar sensor) |
| FR-011–FR-015 | CA-009, CA-010, CA-011 | BANCADA + SIMULADOR | **PARC** — FSM/histerese **PASS (HOST)**; 80 °C físico **PENDENTE** |
| FR-016–FR-021 | CA-012, CA-015 | HOST + BANCADA | **PASS (HOST)** — PID saturação/anti-windup/flags |
| FR-022, FR-023 | CA-013 | BANCADA | **PASS (lógica)** — reset de integral no modo manual; BANCADA **PENDENTE** |
| FR-024 | CA-014 | BANCADA | **PENDENTE** (autotuning em planta) |
| FR-025–FR-032 | CA-016 a CA-019 | BANCADA | **PENDENTE** (navegador conectado ao AP) |
| NFR-015–NFR-022 | CA-020 | Inspeção | **PENDENTE** (inspeção visual) |
| FR-034, NFR-024 | CA-022 | BANCADA | **PENDENTE** (navegador; API já expõe `health`) |
| NFR-025 (160 MHz) | — | Build | **PASS** — boot `CPU=160 MHz` |
| NFR-001, NFR-003, NFR-004 | CA-011 | BANCADA | **PASS (parcial)** — loop estável/status contínuo |
| NFR-005 | — | Build | **PASS** — Flash ~32,6% (< 4 MB, margem) |
| NFR-006, NFR-008 | — | BANCADA | **PENDENTE** (heap/stack formais) |
| NFR-007 | — | Estático | **PASS** — `/api/state` em buffer estático |
| NFR-009, NFR-010 | — | Estática | **PASS (revisão)** — ownership explícito, sem data race |

**Legenda:** `PASS` = evidência coletada · `PARC` = parcial (parte em HOST/serial, parte física pendente) · `PENDENTE` = aguarda validação física/em rede.

## Validações físicas pendentes (responsável: operador)

> Exigem condições que não pude exercer com segurança de forma autônoma (cliente no AP, 80 °C, medição física).

1. **Rede/AP + dashboard multicliente** — conectar **2+ dispositivos** a `ESP8266-<chipid>`; abrir `http://192.168.4.1`; confirmar renderização (sem rolagem, **saúde do MCU** no cabeçalho) e atualização a **1 s** em ambos. *(CA-001 a CA-004, CA-016 a CA-022)*
2. **Endpoints** — `curl http://192.168.4.1/api/state` (contém `health`) e `POST /api/control` (setpoint, modo, PID, autotuning). *(CA-005, CA-012 a CA-015)*
3. **Alarme a 80 °C** — elevar PV até ≥80 °C: corte da resistência + bip 150 ms/2 s; desarme <78 °C. *(CA-009, CA-010)*
4. **PWM real** — medir D1: MV=100% → ≈1023; MV=0% → 0. *(CA-007)*
5. **Autotuning em planta** — relé/degrau com constantes plausíveis. *(CA-014)*
6. **Fail-safe físico** — congelar leitura do sensor e confirmar MV=0 + `ERRO SENSOR`. *(CA-021)*

## Bloqueios / notas

- O host atual não está na sub-rede do AP → `curl` a `192.168.4.1` a partir desta máquina não é possível; validar a partir de um cliente conectado ao AP.
- **Segurança:** verificar contato térmico do DS18B20 — leitura pode refletir o ambiente, não a superfície aquecida (o alarme de 80 °C depende do sensor).
- `NFR-006`/`NFR-008` (RAM/stack) sem medição formal; usar `ESP.getFreeHeap()`/`ESP.getMaxFreeBlockSize()` já expostos no dashboard.
- A etapa de **degrau (Cohen-Coon)** usa estimativa simplificada de τ; pode exigir ajuste em BANCADA.

## Próximos passos

1. Executar os itens 1–6 de validação física em BANCADA (dispositivo conectado ao AP).
2. Medir heap/stack e consolidar `NFR-006`/`NFR-008`.
3. Atualizar `STATE.md`/`tasks.md` com os resultados de rastreabilidade (`PASS`/`PENDENTE`).

## Critério de conclusão

Todos os critérios `CA-001` a `CA-022` com evidência `PASS` (BANCADA) e `README.md` consistente com o comportamento validado.

