# STATE — Projeto

> Estado corrente do projeto de controle térmico em NodeMCU ESP8266.
> Última atualização: 2026-08-26.

## Progresso da feature `controle-termico`

| Fase | Status |
|---|---|
| Especificação (`spec.md`) | ✅ concluída |
| Decisões (`context.md`) | ✅ concluída (Q-001 a Q-006) |
| Design (`design.md`) | ✅ concluída |
| Tarefas (`tasks.md`) | ✅ concluída (T-001 a T-014) |
| Implementação | ✅ concluída (todos os módulos) |
| Testes HOST | ✅ 29/29 passando |
| Build do alvo | ✅ OK (Flash 32,2%, RAM 36,3%) |
| Upload | ✅ gravado em `/dev/ttyUSB0` |
| BANCADA (parcial) | ✅ sensor/loop/alarme OFF; ⏳ rede/dashboard/alarme 80 °C pendentes |

## Decisões registradas (Q-001–Q-006)

- **Q-001** Alarme ON ≥80 °C / OFF <78 °C.
- **Q-002** PID custom com anti-windup.
- **Q-003** Autotuning com relé + degrau (selecionável).
- **Q-004** Dashboard via polling `fetch` (500 ms).
- **Q-005** SSID derivado do chip ID (`ESP8266-XXXXXX`).
- **Q-006** Validação em BANCADA (`/dev/ttyUSB0`), sem HIL.

## Notas e pendências

- Validação física completa (client conectado ao AP, renderização do dashboard, corte da resistência em 80 °C, cadência do buzzer e PWM) **pendente** — ver `HANDSOFF.md`.
- `NFR-006` (margem de RAM) e `NFR-008` (stack) ainda sem medição formal no BANCADA.
- Política de alocação dinâmica (`NFR-007`) permanece **A CONFIRMAR**; o endpoint de escrita usa ArduinoJson (alocação transitória em requisição de controle).
