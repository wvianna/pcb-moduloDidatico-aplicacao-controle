#pragma once
// state.h — Modelo de dados compartilhado do sistema de controle térmico.
//
// Acesso compartilhado: TODAS as escritas ocorrem no loop() (main). O servidor
// HTTP apenas lê o estado (constância); alterações via requisições são aplicadas
// por flags e confirmadas no próximo ciclo (evita data race — NFR-010).
//
// Não usa alocação dinâmica no caminho periódico (NFR-007).

#include <stdint.h>
#include "config.h"

// ---------------------------------------------------------------------------
// Enums de estado
// ---------------------------------------------------------------------------
enum class Modo {
  MANUAL,   // malha aberta — operador define MV; PID pausado/resetado
  AUTO,     // malha fechada — PID modula a MV para manter PV no setpoint
  TUNING    // autotuning em execução — PID suspenso
};

enum class EstadoAlarme {
  NORMAL,   // operação segura
  ALARME    // temperatura >= ALARM_ON_TEMP_C; resistência cortada + buzzer
};

enum class MetodoTuning {
  RELAY,    // identificação por relé (Ziegler-Nichols / relay)
  STEP      // resposta em degrau (Cohen-Coon)
};

enum class EstadoTuning {
  IDLE,     // sem autotuning
  RUNNING,  // em execução
  DONE_OK,  // concluído — constantes aplicadas
  DONE_BAD  // falhou/timeout — parâmetros mantidos
};

// ---------------------------------------------------------------------------
// Parâmetros do PID
// ---------------------------------------------------------------------------
struct ParamsPID {
  float kp;              // ganho proporcional
  float ki;              // ganho integral (sempre ativo — sem checkbox)
  float kd;              // ganho derivativo
  bool  enableP;         // habilita termo proporcional (checkbox)
  bool  enableD;         // habilita termo derivativo (checkbox)
};

// ---------------------------------------------------------------------------
// Estado completo do sistema (compartilhado entre módulos)
// ---------------------------------------------------------------------------
struct ControlState {
  float          pv;         // variável de processo (°C) — faixa PV_MIN..PV_MAX
  float          mv;         // variável manipulada (%) — faixa 0..100
  float          setpoint;   // setpoint (°C) — faixa SETPOINT_MIN..MAX
  Modo           modo;
  ParamsPID      pid;
  MetodoTuning   metodo;
  EstadoTuning   tuning;
  EstadoAlarme   alarm;
  bool           sensor_fail;  // falha de leitura do DS18B20
  uint32_t       ts;           // timestamp (millis) da última atualização
};

// ---------------------------------------------------------------------------
// Conversores de enum para string (uso no JSON / serial)
// ---------------------------------------------------------------------------
inline const char* modoToString(Modo m) {
  switch (m) {
    case Modo::MANUAL:  return "manual";
    case Modo::AUTO:    return "auto";
    case Modo::TUNING:  return "tuning";
  }
  return "auto";
}

inline const char* estadoAlarmeToString(EstadoAlarme a) {
  return (a == EstadoAlarme::ALARME) ? "alarm" : "normal";
}

inline const char* metoTuningToString(MetodoTuning m) {
  return (m == MetodoTuning::RELAY) ? "relay" : "step";
}

// ---------------------------------------------------------------------------
// Inicialização do estado (valores seguros por padrão)
// ---------------------------------------------------------------------------
inline ControlState state_init() {
  ControlState s;
  s.pv          = 25.0f;
  s.mv          = 0.0f;
  s.setpoint    = 50.0f;
  s.modo        = Modo::MANUAL;
  s.pid.kp      = PID_DEFAULT_KP;
  s.pid.ki      = PID_DEFAULT_KI;
  s.pid.kd      = PID_DEFAULT_KD;
  s.pid.enableP = true;
  s.pid.enableD = false;
  s.metodo      = MetodoTuning::RELAY;
  s.tuning      = EstadoTuning::IDLE;
  s.alarm       = EstadoAlarme::NORMAL;
  s.sensor_fail = false;
  s.ts          = 0;
  return s;
}
