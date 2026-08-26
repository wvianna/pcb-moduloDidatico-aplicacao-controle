# Contexto — Decisões de especificação (controle-termico)

> Registro das respostas às perguntas bloqueadoras da `spec.md` (`Q-001` a `Q-006`). As decisões aqui registradas **alteram o contrato** e foram refletidas nos requisitos e critérios de aceite.

## Decisões registradas

| ID | Pergunta | Decisão | Refletido em |
|---|---|---|---|
| Q-001 | Semântica do limiar de desativação do alarme | Alarme ativa em **≥ 80 °C**; **cessa apenas quando a temperatura ficar < 78 °C** (histerese de 2 °C). Substitui o limiar de 75 °C do `descritivo.txt`. | §4.1 (tabela e nota), FR-011/FR-014, CA-009/CA-010 |
| Q-002 | Implementação do PID | **Custom** com controle de anti-windup e saturação (sem dependência de biblioteca externa). | FR-016, FR-021, design.md (a produzir) |
| Q-003 | Tipo de autotuning | **Dois métodos selecionáveis**: (a) **relé** (identificação de ganho/período críticos, ex. Ziegler-Nichols por relay) e (b) **resposta em degrau** (ex. Cohen-Coon). | FR-024 |
| Q-004 | Comunicação dashboard | **Polling assíncrono** via `fetch` periódico (a cada ~500 ms–1 s), sem WebSocket. | FR-031 |
| Q-005 | SSID do Access Point | **Derivado do MAC** (ex.: `ESP8266-1A2B3C`), mantendo a constituição atual. | FR-001, §3 Rede |
| Q-006 | Validação física | Validação em **BANCADA**: ESP8266 conectado em `/dev/ttyUSB0`; rede Wi-Fi disponível para os testes. Sem automação HIL nesta fase. | §9 Premissas |

## Observações

- A histerese de **2 °C** (80 °C on / 78 °C off) foi escolhida pelo operador para reduzir o risco de *chattering* em torno do limite.
- O limiar de desativação deve ser implementado como constante única no código (ex.: `ALARM_OFF_TEMP_C = 78.0f`), evitando divergência entre firmware e dashboard.
- `/dev/ttyUSB0` será a porta de upload/serial utilizada; o firmware permanece acessível via AP para os testes de dashboard.
