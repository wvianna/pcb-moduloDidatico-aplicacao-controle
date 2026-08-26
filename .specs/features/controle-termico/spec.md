# Spec: Controle Térmico com Dashboard Web (NodeMCU ESP8266)

> **Feature ID:** `controle-termico`
> **Escopo (SDD):** Grande — múltiplos módulos (aquisição, PID, alarme, rede/AP, servidor web e IHM), máquina de estados e integração de hardware.
> **Alvo:** NodeMCU V2 (ESP12E / ESP8266MOD), Arduino Core para ESP8266 >= 3.0.0, PlatformIO.
> **Origem da intenção:** `docs/descritivo.txt` + `.specs/project/constitution.md`.
> **Status:** Especificação de comportamento (pré-design). Produto de saída: `design.md` e `tasks.md` para execução.

---

## 1. Objetivo

Implementar um sistema de controle de temperatura em malha fechada com PID, operação em malha aberta (manual), autotuning e proteção por alarme com histerese, sobre um NodeMCU ESP8266. O sistema deve expor uma interface web (dashboard) acessível em modo Access Point (AP), permitindo monitorar e ajustar o processo térmico em tempo real de forma segura e sem bloqueios no loop principal.

O controle deve gerenciar concorrentemente: aquisição do sensor de temperatura (PV), atuação por PWM na resistência de aquecimento (MV), alarme sonoro no buzzer, cálculo PID e um servidor web com dashboard responsivo.

## 2. Fora de escopo

- Não há persistência de parâmetros em flash/EEPROM (qualquer inclusão exige decisão registrada).
- Não há autenticação/criptografia na rede Wi-Fi nem no servidor HTTP (modo AP sem encriptação).
- Não há integração com nuvem, MQTT, ou servidores externos.
- Não há atualização OTA de firmware.
- Não há atuadores perigosos além da resistência de aquecimento; o estado seguro de falha é desligar a resistência e manter o servidor web respondendo.
- Não há suporte a múltiplos sensores de temperatura simultâneos.

## 3. Contexto e atores

### Atores

| Ator | Papel |
|---|---|
| **Operador** | Acessa o dashboard via navegador em `http://192.168.4.1`, ajusta setpoint, modo, constantes PID, configura autotuning e lê PV/MV. |
| **Sistema térmico** | Planta física que responde à potência aplicada (MV) com temperatura (PV). |

### Dispositivos e interfaces (conforme constituição)

| Interface | Pino | Tipo | Escala / Faixa |
|---|---|---|---|
| Sensor de temperatura (DS18B20) | D2 / GPIO4 | OneWire | °C, faixa de operação 20–90 °C |
| Resistência de aquecimento | D1 / GPIO5 | PWM (10 bits) | 0–1023 → mapeado para 0–100 % |
| Buzzer | D0 / GPIO16 | On–off | Alarme sonoro intermitente |

### Rede

- Modo **Access Point (AP)**, sem encriptação.
- SSID lido da constituição (na demonstração, derivado do MAC, ex.: `ESP8266-1A2B3C`).
- IP estático do MCU: **192.168.4.1** (default gateway).
- Máscara: **255.255.255.0** (CIDR `192.168.4.0/24`).
- DHCP do AP distribui IPs aos clientes na sub-rede `192.168.4.0/24`.
- Servidor HTTP serve o dashboard e um endpoint JSON de estado.

## 4. Estados e eventos

### 4.1 Máquina de estados de segurança (alarme)

Baseado no `descritivo.txt`, com histerese para evitar *chattering*:

| Estado | Condição de entrada | Ação de saída | Próximo estado |
|---|---|---|---|
| NORMAL | Temp ≥ 80 °C | Ativar buzzer (intermitente), desligar resistência | ALARME |
| ALARME | Temp ≥ 78 °C | Manter buzzer (intermitente), manter resistência desligada | ALARME |
| ALARME | Temp < 78 °C | Desativar buzzer | NORMAL |

> **Decisão Q-001:** o alarme ativa em **≥ 80 °C** e **cessa quando a temperatura ficar < 78 °C** (histerese de 2 °C, substituindo o limiar de 75 °C do `descritivo.txt`). Ambos os limiares devem ser centralizados em constantes únicas (ex.: `ALARM_ON_TEMP_C = 80.0f`, `ALARM_OFF_TEMP_C = 78.0f`) e verificados em design.

### 4.2 Modos de operação do controlador

| Modo | Semântica | Efeito no PID |
|---|---|---|
| **MANUAL** (malha aberta) | Operador define MV diretamente (0–100 %). | PID pausado e resetado (anti-windup / integral reset). |
| **AUTOMÁTICO** (malha fechada) | PID modula PWM para manter PV no setpoint. | PID ativo. |
| **AUTOTUNING** | Identificação automática de P, I, D via método selecionado. | PID suspenso durante a identificação; aplica constantes ao final. |

### 4.3 Eventos relevantes

- `SENSOR_OK` / `SENSOR_FAIL` — leitura válida ou falha do DS18B20.
- `ALARM_ENTER` / `ALARM_EXIT` — transições da máquina de alarme.
- `MODE_CHANGE` — troca manual/automático/autotuning.
- `SETPOINT_CHANGE`, `PID_PARAM_CHANGE`, `TUNING_METHOD_CHANGE`.
- `CLIENT_CONNECT` / `CLIENT_DISCONNECT` — ciclo do servidor web.
- `NETWORK_RESET` — após reset/brownout, reinicializar AP + servidor.

## 5. Requisitos funcionais

Cada requisito `FR-###` é observável e testável. Interfaces de hardware e comunicação têm unidade, escala, faixa e comportamento fora da faixa documentados.

### 5.1 Rede e servidor

- **FR-001** — O firmware deve iniciar o ESP8266 em modo **Access Point** (AP) com SSID obtido do arquivo de constituição.
- **FR-002** — O AP deve configurar IP estático `192.168.4.1`, gateway `192.168.4.1` e máscara `255.255.255.0`.
- **FR-003** — O AP deve habilitar DHCP para a sub-rede `192.168.4.0/24`, distribuindo IPs `192.168.4.x` aos clientes.
- **FR-004** — O servidor HTTP deve servir o dashboard web em `http://192.168.4.1` (rota `/`).
- **FR-005** — O servidor HTTP deve expor um endpoint JSON (ex.: `GET /api/state`) com: temperatura (PV °C), potência (MV %), modo, setpoint, estado de alarme e estado de falha do sensor. O contrato exato (campos, tipos, erro) é detalhado no `design.md`. **A CONFIRMAR** o formato do contrato.
- **FR-006** — Requisições HTTP inválidas (rota desconhecida, método não suportado, corpo malformado) devem retornar resposta de erro sem travar o servidor nem o loop de controle.

### 5.2 Aquisição e atuação

- **FR-007** — O sistema deve ler a temperatura do sensor DS18B20 a cada segundo (1 Hz), convertendo para graus Celsius.
- **FR-008** — A saída PWM da resistência deve ter resolução de **10 bits (0–1023)**, mapeada internamente para a escala de **0–100 %** exibida ao operador.
- **FR-009** — O valor de leitura deve passar por filtragem antes do cálculo derivativo do PID, reduzindo ruído sem atraso perceptível (`NFR-###` de filtro em design).
- **FR-010** — Em falha de leitura do DS18B20 (sensor desconectado, CRC inválido, timeout), o sistema deve: (a) marcar `SENSOR_FAIL`, (b) suspender a atuação da resistência (estado seguro), (c) exibir falha no dashboard, e (d) continuar respondendo às requisições web.

### 5.3 Máquina de alarme

- **FR-011** — O alarme deve ser **ativado** imediatamente quando a temperatura atingir **≥ 80 °C**.
- **FR-012** — Em ALARME, o MCU deve **desligar a resistência de aquecimento** e acionar o buzzer.
- **FR-013** — O sinal sonoro deve ser **150 ms ligado a cada 2 s** (cadência não-bloqueante).
- **FR-014** — O alarme/buzzer deve ser **desativado** quando a temperatura ficar **< 78 °C** (histerese de 2 °C), retornando a NORMAL. *(Decisão Q-001)*
- **FR-015** — A lógica de temporização do buzzer deve usar `millis()` / contadores não-bloqueantes, **nunca** `delay()` no caminho crítico.

### 5.4 Controlador PID

- **FR-016** — O controlador deve calcular PID com constantes **P, I e D** ajustáveis em tempo real.
- **FR-017** — A constante **P** deve possuir checkbox para habilitar/desabilitar.
- **FR-018** — A constante **D** deve possuir checkbox para habilitar/desabilitar.
- **FR-019** — A constante **I** deve permanecer **sempre ativa** quando o PID estiver em execução (sem checkbox de desativação).
- **FR-020** — O **setpoint** deve ser ajustável de **20 a 80 °C** via entrada numérica na interface.
- **FR-021** — O PID deve rodar em intervalo determinístico (ver `NFR-001`), com atualização da MV limitada entre **0 e 100 %** (saturação anti-windup).

> **Decisão Q-002:** o controlador será implementado **custom**, com algoritmo de *anti-windup* e saturação de saída em 0–100 %, sem dependência de biblioteca externa. Detalhes de cálculo (formas P/I/D, *sample time*, reset de integral) no `design.md`.

### 5.5 Modos de operação

- **FR-022** — **Modo Manual (malha aberta):** o operador define a potência da resistência de **0 a 100 %** via ajuste gráfico (slider). Ao entrar neste modo, o cálculo PID deve ser **pausado e resetado** (integral zerada) para evitar *integral windup*.
- **FR-023** — **Modo Automático (malha fechada):** o PID assume a modulação do PWM para manter a PV no setpoint.
- **FR-024** — **Autotuning:** o sistema deve oferecer **dois métodos de sintonia selecionáveis** via interface: (a) **relé** (identificação de ganho/período críticos, ex. Ziegler-Nichols por relay) e (b) **resposta em degrau** (ex. Cohen-Coon). Ambos identificam as constantes P, I, D do processo e as aplicam ao controlador. A implementação é detalhada no `design.md`. *(Decisão Q-003)*

### 5.6 Interface homem-máquina (IHM / Dashboard)

- **FR-025** — O dashboard deve usar **tema claro** e priorizar **viewport única** (sem rolagem vertical/horizontal) para leitura instantânea de todos os dados críticos.
- **FR-026** — A **Variável de Processo (PV)** deve ter visualização **tríplice**: numérica, gauge e gráfico de tendência, na faixa de **20 a 90 °C**.
- **FR-027** — A **Variável Manipulada (MV)** deve ter visualização **tríplice**: numérica, gauge e gráfico de tendência, na faixa de **0 a 100 %**.
- **FR-028** — O **Setpoint** deve ser exibido/ajustado como valor numérico/input de **20 a 80 °C**.
- **FR-029** — Os **gráficos de tendência** devem respeitar rigorosamente os limites: PV **20–90 °C** e MV **0–100 %**.
- **FR-030** — Cada elemento de controle (botões, campos, checkboxes, seletores de modo, slider) deve exibir um **hint** explicativo de sua funcionalidade.
- **FR-031** — O dashboard deve se **atualizar em tempo real** via **polling assíncrono** (requisições `fetch` periódicas a cada ~500 ms–1 s), sem bloquear o loop principal. *(Decisão Q-004)*
- **FR-032** — A interface deve ser **responsiva**, adaptando-se a diferentes dispositivos com visualização de gráficos/gauges proporcional à tela.

## 6. Requisitos não funcionais

Cada `NFR-###` deve ter métrica, condição e método de medição.

### 6.1 Tempo real e determinismo

- **NFR-001** — O loop de controle PID deve ser executado em intervalo **determinístico de 100 a 200 ms**; o período deve ser constante e documentado. Medição: timestamp de início/fim do cálculo.
- **NFR-002** — A aquisição de temperatura e A0 deve ocorrer a **1 Hz** (período 1 s), com **deadline máximo de 1,1 s** e **jitter máximo de ±100 ms** (conforme constituição). Medição: variação do intervalo entre amostras.
- **NFR-003** — O servidor web e as rotinas de alarme **não devem bloquear** o cálculo PID nem a aquisição; qualquer espera deve ser não-bloqueante (`millis()`/tasks).
- **NFR-004** — O sinal do buzzer deve manter cadência de **150 ms ligado / 2 s desligado**, com erro máximo de **±20 ms**, sem afetar `NFR-001` nem `NFR-002`.

### 6.2 Recursos limitados (memória/flash)

- **NFR-005** — O binário deve caber em **4 MB de flash**, com margem mínima de **10 %**. Medição: relatório de tamanho do build (`pio run`).
- **NFR-006** — O uso de **RAM** deve ser medido/estimado para loop, servidor web e tasks; margem mínima `A CONFIRMAR` (constituição).
- **NFR-007** — **Alocação dinâmica** (malloc/new/String) deve ser **evitada** no caminho periódico de aquisição/PID e no atendimento de requisições; buffers devem ser pré-alocados. **A CONFIRMAR** política exata.
- **NFR-008** — O **stack** deve ser medido/estimado para o loop e as tasks; critério `A CONFIRMAR` (constituição).

### 6.3 Concorrência e robustez

- **NFR-009** — Acesso compartilhado entre aquisição (1 Hz), servidor HTTP e loop de controle deve usar **ownership explícito** e evitar bloqueios prolongados (conforme constituição).
- **NFR-010** — Não deve haver **data race** entre ISR, loop e tasks; variáveis compartilhadas protegidas por mecanismo documentado (ex.: `volatile` + seção crítica curta).

### 6.4 Segurança e estado seguro

- **NFR-011** — Em falha de sensor, comunicação, alimentação ou software, o estado seguro é **desligar a resistência** e **manter o servidor web respondendo** (constituição).
- **NFR-012** — A temperatura não deve exceder **80 °C** em operação normal sob falha simples do controlador; a máquina de alarme deve cortar a atuação de forma **garantida** (hardware-independente de atrasos de rede).

### 6.5 Diagnóstico e recuperação

- **NFR-013** — Após reset/brownout, o sistema deve **reinicializar** AP, servidor HTTP e aquisições sem intervenção.
- **NFR-014** — Falha do **DS18B20** deve ser representada **explicitamente** no dashboard (ex.: “ERRO SENSOR”), e o servidor deve continuar ativo.

### 6.6 IHM — qualidade estética (Frontend Aesthetics Guidelines)

> Diretrizes da skill `frontend-design`. Estas são requisitos de **design/UX** verificáveis por inspeção visual e por testes de viewport. Complementam `FR-025` a `FR-032`.

- **NFR-015** — **Tipografia:** usar pares de fontes **distintivos e com personalidade** (display + corpo), evitando fontes genéricas do tipo Arial, Inter, Roboto ou fontes de sistema. A escolha deve ser deliberada e coesa com o tema.
- **NFR-016** — **Cor e tema:** usar **tema claro** coeso via **CSS variables**, com cor dominante e acentos nítidos (não paletas tímidas/equilibradas). Evitar gradientes roxos sobre fundo branco.
- **NFR-017** — **Movimento:** incluir micro-interações/animações **CSS-first** (ex.: *page load* com *staggered reveals* por `animation-delay`, estados de *hover* surpreendentes), de alto impacto e sem prejudicar desempenho.
- **NFR-018** — **Composição espacial:** layout com **composição intencional** (assimetria, sobreposição, fluxo diagonal, quebra de grid) OU densidade controlada, gerando identidade visual memorável.
- **NFR-019** — **Fundo e detalhes:** criar **atmosfera e profundidade** (gradientes suaves, texturas/ruído, padrões geométricos, sombras dramáticas, bordas decorativas) em vez de cor sólida simples, mantendo legibilidade.
- **NFR-020** — **Acessibilidade/legibilidade:** contraste adequado WCAG AA para texto e elementos de controle; *hints* legíveis; foco visível via teclado.
- **NFR-021** — **Sem estética genérica de IA:** proibido reutilizar combinações clichês (Inter/Space Grotesk, gradientes roxos, padrões previsíveis). Cada geração deve ter direção estética própria.
- **NFR-022** — **Desempenho visual:** o dashboard deve manter fluidez (sem *scroll* e com animações a 60 fps) mesmo com **atualização a cada 500 ms–1 s** de PV/MV.

## 7. Critérios de aceitação

Formato `DADO / QUANDO / ENTÃO`. Cada critério é rastreável a um ou mais requisitos.

### 7.1 Rede e servidor

- **CA-001** — DADO o firmware iniciado, QUANDO o MCU entra em modo AP, ENTÃO o SSID é o da constituição, o IP é `192.168.4.1`, o gateway é `192.168.4.1` e a máscara é `255.255.255.0`. *(FR-001, FR-002)*
- **CA-002** — DADO um cliente associado ao AP, QUANDO ele solicita IP via DHCP, ENTÃO recebe um endereço na sub-rede `192.168.4.0/24`. *(FR-003)*
- **CA-003** — DADO o servidor HTTP ativo, QUANDO um navegador acessa `http://192.168.4.1`, ENTÃO o dashboard é servido com status 200. *(FR-004)*
- **CA-004** — DADO o endpoint JSON, QUANDO `GET /api/state` é chamado, ENTÃO retorna PV, MV, modo, setpoint, alarme e falha do sensor em JSON válido. *(FR-005)*
- **CA-005** — DADO uma requisição inválida (rota/método/corpo), QUANDO ela é recebida, ENTÃO o servidor responde erro (4xx/5xx) e **continua** respondendo requisições subsequentes. *(FR-006)*

### 7.2 Aquisição e atuação

- **CA-006** — DADO o sistema em operação, QUANDO 1 s decorre, ENTÃO uma nova amostra de temperatura é obtida (1 Hz) dentro de ±100 ms de jitter. *(FR-007, NFR-002)*
- **CA-007** — DADO o PWM em 100 %, QUANDO a MV é 100 %, ENTÃO a saída analógica no pino é `1023`; e `0` quando MV é 0 %. *(FR-008)*
- **CA-008** — DADO o sensor DS18B20 desconectado/falho, QUANDO a leitura falha, ENTÃO o dashboard exibe falha de sensor, a resistência é desligada e o servidor continua respondendo. *(FR-010, NFR-011, NFR-014)*

### 7.3 Alarme

- **CA-009** — DADO a temperatura ≥ 80 °C, QUANDO a leitura é atualizada, ENTÃO o alarme é ativado, a resistência é desligada e o buzzer inicia bip de 150 ms a cada 2 s. *(FR-011, FR-012, FR-013)*
- **CA-010** — DADO o estado ALARME, QUANDO a temperatura cai para **< 78 °C**, ENTÃO o buzzer é desligado e o estado retorna a NORMAL. *(FR-014)*
- **CA-011** — DADO o alarme ativo, QUANDO o PID e o servidor web estão em execução, ENTÃO o tempo de ciclo do PID permanece dentro do intervalo determinístico (sem bloqueio por `delay`). *(FR-015, NFR-003, NFR-004)*

### 7.4 PID e modos

- **CA-012** — DADO o modo AUTOMÁTICO, QUANDO P e D estão habilitados e I ativo, ENTÃO o PID calcula MV que satura entre 0 e 100 % e o setpoint é mantido. *(FR-016, FR-017, FR-018, FR-019, FR-021)*
- **CA-013** — DADO o modo MANUAL, QUANDO o operador ajusta a potência via slider, ENTÃO a MV é aplicada diretamente e o PID é pausado/resetado (integral zerada). *(FR-022)*
- **CA-014** — DADO o modo AUTOTUNING, QUANDO um método é selecionado e iniciado, ENTÃO o sistema identifica e aplica as constantes P, I, D. *(FR-024)*
- **CA-015** — DADO o setpoint, QUANDO o operador seleciona valor abaixo de 20 °C ou acima de 80 °C, ENTÃO o valor é rejeitado ou satura no limite. *(FR-020)*

### 7.5 IHM

- **CA-016** — DADO o dashboard carregado, QUANDO a viewport é de um dispositivo alvo, ENTÃO todos os dados críticos são visíveis sem rolagem vertical/horizontal. *(FR-025)*
- **CA-017** — DADO o dashboard, QUANDO PV e MV são atualizados, ENTÃO cada um é exibido de forma **tríplice** (numérico, gauge, gráfico) dentro dos limites (PV 20–90 °C, MV 0–100 %). *(FR-026, FR-027, FR-029)*
- **CA-018** — DADO cada controle da interface, QUANDO o operador passa o mouse/foca, ENTÃO um **hint** explica sua funcionalidade. *(FR-030)*
- **CA-019** — DADO o dashboard em diferentes dispositivos, QUANDO a tela muda de tamanho, ENTÃO gráficos e gauges se adaptam proporcionalmente. *(FR-032)*
- **CA-020** — DADO o dashboard, QUANDO inspecionado visualmente, ENTÃO usa tipografia distinta, tema coeso via CSS variables, micro-interações/fundo com profundidade e não apresenta estética genérica de IA. *(NFR-015 a NFR-022)*

## 8. Matriz de rastreabilidade (requisito → critério → evidência)

| Requisito | Critérios | Tipo de evidência (nível) |
|---|---|---|
| FR-001, FR-002, FR-003 | CA-001, CA-002 | BANCADA (conectar cliente e inspecionar DHCP) |
| FR-004, FR-005, FR-006 | CA-003, CA-004, CA-005 | BANCADA (requisições HTTP/JSON) |
| FR-007, FR-008 | CA-006, CA-007 | BANCADA (medir período e PWM) |
| FR-010 | CA-008 | BANCADA (desconectar sensor) |
| FR-011–FR-015 | CA-009, CA-010, CA-011 | BANCADA + SIMULADOR (temporização) |
| FR-016–FR-021 | CA-012, CA-015 | HOST (cálculo) + BANCADA |
| FR-022–FR-024 | CA-013, CA-014 | BANCADA |
| FR-025–FR-032 | CA-016 a CA-019 | BANCADA (navegador) |
| NFR-015–NFR-022 | CA-020 | Inspeção visual (BANCADA / revisão de design) |
| NFR-001–NFR-004 | CA-006, CA-011 | BANCADA (timestamps) + HOST |
| NFR-005, NFR-006 | — | Build (tamanho/RAM) |
| NFR-007–NFR-010 | — | Revisão estática + medição stack |
| NFR-011–NFR-014 | CA-008, CA-010 | BANCADA (falha/reset/brownout) |

## 9. Premissas, riscos e perguntas bloqueadoras

### Premissas

- O sensor de temperatura é **DS18B20** (OneWire) em D2/GPIO4, conforme a constituição.
- O loop de controle pode rodar no `loop()` do Arduino com temporização não-bloqueante, ou em task dedicada, **a definir** no design.
- O dashboard será servido como página **estática embutida** no firmware (PROGMEM), reduzindo dependências externas.
- **Validação em BANCADA:** ESP8266 conectado via serial em **`/dev/ttyUSB0`** e rede Wi-Fi disponível durante os testes. *(Decisão Q-006)*

### Riscos

- **Sobrecarga do ESP8266:** conciliar rede + HTTP + cálculo PID + aquisição pode estourar memória/tempo se não houver pré-alocação de buffers e tarefas bem separadas. Mitigação: `NFR-001`, `NFR-007`, `NFR-008`, `NFR-009`.
- **Autotuning em malha fechada:** o método de **relé** excita a planta e pode desestabilizá-la; precisa de validação em BANCADA e limite de tempo/amostras em `design.md`.
- **Semântica do limite de histerese:** decidido em **80 °C on / 78 °C off** (histerese de 2 °C) para reduzir o risco de *chattering* — ver `context.md`.
- **Tema claro + estética não-genérica:** equilibrar identidade visual forte com legibilidade e viewport única pode ser desafiador; validar em vários viewports.

### Perguntas bloqueadoras resolvidas

| ID | Decisão (registrada em `context.md`) |
|---|---|
| Q-001 | Alarme ativa em ≥ 80 °C; **cessa em < 78 °C** (histerese 2 °C). |
| Q-002 | PID **custom** com anti-windup e saturação. |
| Q-003 | Autotuning com **dois métodos selecionáveis** (relé + resposta em degrau). |
| Q-004 | Atualização do dashboard via **polling assíncrono (fetch)**. |
| Q-005 | SSID **derivado do MAC**. |
| Q-006 | Validação **BANCADA** em `/dev/ttyUSB0` + Wi-Fi. |

> As decisões foram registradas em `context.md` e refletidas nos requisitos e critérios de aceite. Decisões que permanecerem em aberto não devem ser fabricadas — devem virar item de design/`tasks.md` ou bloqueio explícito.

## 10. Entregáveis esperados (próximos artefatos)

Conforme o escopo **Grande**, após a especificação:

1. `design.md` — arquitetura de módulos, máquina de estados, ownership/buffers, contexto de execução (loop/task/ISR), orçamento de tempo/memória, contrato do endpoint JSON, estratégia de anti-windup e direção estética do dashboard.
2. `tasks.md` — tarefas atômicas ordenadas por risco, com `Requisitos`, `Onde`, `Depende de`, `Feito quando`, `Testes` e `Gate`.
3. Implementação com verificação: build (`pio run`), testes HOST/SIMULADOR e validação BANCADA.
4. Atualização de `README.md` (instalação/configuração/compilação/teste/execução).
5. `STATE.md` / `HANDSOFF.md` para continuidade.
