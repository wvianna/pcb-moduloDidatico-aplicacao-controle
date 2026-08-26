# HANDSOFF — controle-termico

> Handoff para o próximo agente/operador. Feature `controle-termico` (NodeMCU ESP8266).

## Estado atual

Implementação **completa** e integrada. Firmware compila para o alvo, testes HOST passam e foi **gravado** com sucesso no ESP8266 em `/dev/ttyUSB0`. O dispositivo está operando (modo manual, MV=0, sensor OK).

## Objetivo restante

Validar as funcionalidades que exigem rede e condição de sobreaquecimento, em BANCADA.

## Arquivos relevantes

- `src/main.cpp`, `src/config.h`, `src/state.h`
- `src/sensors.h/.cpp`, `src/actuators.h/.cpp`, `src/pid.h/.cpp`
- `src/alarm.h/.cpp`, `src/control.h/.cpp`
- `src/autotune.h/.cpp`, `src/tuning_math.h`
- `src/web_server.h/.cpp`, `src/dashboard.h`
- `test/host/test_core.cpp`, `platformio.ini`, `README.md`
- Especificação/design/tarefas: `.specs/features/controle-termico/`

## Decisões tomadas

- Alarme ON ≥80 °C / OFF <78 °C (histerese 2 °C).
- PID custom com anti-windup; P e D com checkbox, I sempre ativo.
- Autotuning por relé (Ziegler-Nichols) e degrau (Cohen-Coon).
- Dashboard por polling `fetch` a cada 500 ms; tema claro industrial (Syne + IBM Plex).
- SSID `ESP8266-<chipid>`; IP 192.168.4.1, máscara /24, DHCP.
- Validação em BANCADA (`/dev/ttyUSB0`); sem HIL.

## Comandos já executados (com resultado)

| Comando | Resultado |
|---|---|
| `pio run` | ✅ Build OK — Flash 32,2% / RAM 36,3% |
| `g++ ... test/host/test_core.cpp ...` | ✅ 29/29 checks |
| `pio run -t upload --upload-port /dev/ttyUSB0` | ✅ 340 KB gravados, hash verificado |
| `pio device monitor -p /dev/ttyUSB0 -b 115200` | ✅ `[STATUS] PV=32.9 MV=0.0 SP=50.0 modo=manual alarm=OFF sensor=OK` |

## Validações físicas pendentes (responsável: operador)

> Estas exigem condições que não pude exercer com segurança de forma autônoma.

1. **Rede/AP + dashboard** — conectar um dispositivo à rede `ESP8266-<chipid>` e abrir `http://192.168.4.1`; confirmar renderização do dashboard sem rolagem e atualização a cada ~500 ms. *(CA-001 a CA-004, CA-016 a CA-020)*
2. **Endpoints** — `curl http://192.168.4.1/api/state` e `POST /api/control` (setpoint, modo, PID, autotuning). *(CA-005, CA-012 a CA-015)*
3. **Alarme a 80 °C** — elevar a temperatura (ou simular via sensor) até ≥80 °C e confirmar corte da resistência + bip 150 ms/2 s; confirmar desarme <78 °C. *(CA-009, CA-010)*
4. **PWM real** — medir a saída em D1 com MV=100% (≈1023) e MV=0%. *(CA-007)*
5. **Autotuning em planta** — iniciar autotuning relé/degrau e confirmar constantes plausíveis. *(CA-014)*

## Bloqueios / notas

- O host atual não está na sub-rede do AP, portanto `curl` ao `192.168.4.1` a partir desta máquina não foi possível; validar a partir de um cliente conectado ao AP.
- `NFR-006`/`NFR-008` (RAM/stack) sem medição formal; medir `ESP.getFreeHeap()` em execução.
- A etapa de **degrau (Cohen-Coon)** usa uma estimativa simplificada de τ (0.632·(tempo de assentamento)); pode exigir ajuste em BANCADA.

## Próximos passos

1. Executar os itens 1–5 de validação pendente em BANCADA (dispositivo conectado ao AP).
2. Medir heap/stack e ajustar `NFR-007` se necessário.
3. Atualizar `STATE.md`/`tasks.md` com os resultados de Rastreabilidade (`PASS`/`PENDENTE`).

## Critério de conclusão

Todos os critérios `CA-001` a `CA-020` com evidência `PASS` (BANCADA) e `README.md` consistente com o comportamento validado.
