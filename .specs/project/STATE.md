# STATE — Projeto

> Estado corrente do projeto de controle térmico em NodeMCU ESP8266.
> Última atualização: 2026-08-26.

## Progresso da feature `controle-termico`

| Fase | Status |
|---|---|
| Especificação (`spec.md`) | ✅ concluída e atualizada (FR/NFR/CA novos) |
| Decisões (`context.md`) | ✅ concluída (Q-001..Q-006 + D-001..D-010) |
| Design (`design.md`) | ✅ concluída e atualizada |
| Tarefas (`tasks.md`) | ✅ concluída (T-001 a T-014) |
| Implementação | ✅ concluída (todos os módulos) |
| Testes HOST | ✅ **31/31** passando |
| Build do alvo | ✅ OK — **160 MHz** (Flash ~32,6%, RAM ~36,7%) |
| Upload | ✅ gravado em `/dev/ttyUSB0` |
| BANCADA (parcial) | ✅ sensor/loop/alarme OFF/controles P/D; ⏳ rede/dashboard multicliente e alarme a 80 °C pendentes |

## Decisões registradas

### Q-001–Q-006
- **Q-001** Alarme ON ≥80 °C / OFF <78 °C.
- **Q-002** PID custom com anti-windup.
- **Q-003** Autotuning com relé + degrau (selecionável).
- **Q-004** Dashboard via polling `fetch` (**1 s**).
- **Q-005** SSID derivado do chip ID (`ESP8266-XXXXXX`).
- **Q-006** Validação em BANCADA (`/dev/ttyUSB0`), sem HIL.

### Incrementais (D-001..D-010)
- **D-001** Leitura **bruta** do sensor (sem média móvel).
- **D-002** Erro de sensor só após 3 falhas consecutivas.
- **D-003** **Fail-safe**: corta a resistência se a leitura congelar por 3 s.
- **D-004** Conversão OneWire de 800 ms.
- **D-005** Polling do dashboard a **1 s**.
- **D-006** **Multicliente** (AP até 4 clientes + servidor concorrente).
- **D-007** Clock do ESP8266 a **160 MHz**.
- **D-008** **Saúde do MCU** no cabeçalho (CPU, carga/idle, heap, bloco, flash, uptime, clientes).
- **D-009** `/api/state` em buffer estático (`snprintf`) — sem alocação dinâmica.
- **D-010** Controles **P/D verificados** (testes HOST); **I sempre ativa**.

## Notas e pendências

- Validação física completa (client conectado ao AP, renderização do dashboard com saúde do MCU, corte da resistência em 80 °C, cadência do buzzer e PWM) **pendente** — ver `HANDSOFF.md`.
- `NFR-006` (margem de RAM) e `NFR-008` (stack) ainda sem medição formal no BANCADA.
- `NFR-007` **resolvido** para leitura (`/api/state` em buffer estático); o endpoint de escrita (`/api/control`) usa ArduinoJson (alocação transitória em requisição de controle).
- **Segurança:** sensor pode medir o ambiente e não a superfície aquecida — validar contato térmico do DS18B20; o alarme de 80 °C depende disso.

