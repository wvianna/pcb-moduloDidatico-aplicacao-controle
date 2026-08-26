// test_core.cpp — Testes HOST da lógica pura (PID custom + máquina de alarme).
// Não usa Arduino.h. Compilar com:
//   g++ -std=c++11 -Isrc test/host/test_core.cpp src/pid.cpp src/alarm.cpp -o /tmp/test_core && /tmp/test_core

#include <cstdio>
#include <cmath>
#include <cstring>
#include "config.h"
#include "state.h"
#include "pid.h"
#include "alarm.h"
#include "tuning_math.h"

static int failures = 0;
static int checks   = 0;

#define CHECK(cond, msg)                                             \
  do {                                                               \
    ++checks;                                                        \
    if (!(cond)) {                                                   \
      ++failures;                                                    \
      std::printf("  [FAIL] %s (linha %d)\n", msg, __LINE__);        \
    } else {                                                         \
      std::printf("  [ok]   %s\n", msg);                             \
    }                                                                \
  } while (0)

static float nearly(float a, float b, float eps = 0.01f) { return std::fabs(a - b) < eps; }

static void test_pid() {
  std::printf("\n=== PID ===\n");
  PIDController pid;
  ParamsPID p;
  p.kp = 1.5f; p.ki = 0.2f; p.kd = 0.05f; p.enableP = true; p.enableD = false;
  pid.begin();

  // P-only: erro = setpoint - pv.
  float out = pid.compute(50.0f, 40.0f, 0.1f, p, /*integralActive=*/false);
  CHECK(nearly(out, 15.0f), "P-only: out = kp*error = 15");

  // Saturação em 100.
  p.kp = 50.0f;
  out = pid.compute(80.0f, 20.0f, 0.1f, p, false);
  CHECK(nearly(out, 100.0f), "saturação: out clampado em 100");

  // Anti-windup: com integral ativa, erro grande satura a saída e a integral
  // não continua crescendo além do limite; ao inverter o erro a saída reage.
  p.kp = 10.0f; p.ki = 5.0f; p.kd = 0.0f;
  pid.reset();
  float outPre = pid.compute(80.0f, 20.0f, 0.1f, p, true);  // error = +60, satura
  CHECK(nearly(outPre, 100.0f), "anti-windup: satura em 100 com integral ativa");
  // Após muitos passos de erro grande, a integral não deve explodir.
  for (int i = 0; i < 30; ++i) pid.compute(80.0f, 20.0f, 0.1f, p, true);
  outPre = pid.compute(80.0f, 20.0f, 0.1f, p, true);
  CHECK(nearly(outPre, 100.0f), "anti-windup: mantém 100 após windup (integral clampada)");

  // Reset zera a saída.
  pid.reset();
  CHECK(nearly(pid.getOutput(), 0.0f), "reset: saída volta a 0");

  // Derivado habilitado: PV subindo deve reduzir a saída (amortecimento),
  // comparado ao caso sem derivada — avaliado em faixa não-saturada.
  p.enableP = true; p.enableD = true; p.kp = 1.0f; p.kd = 0.2f;
  pid.reset();
  pid.compute(50, 30, 0.1f, p, false);                      // prevPv = 30
  float base = pid.compute(50, 30, 0.1f, p, false);         // dv=0 -> só P
  float lowered = pid.compute(50, 35, 0.1f, p, false);      // PV subiu -> D negativo
  CHECK(nearly(base, 20.0f), "derivativo: base (sem variação) = kp*erro");
  CHECK(lowered < base && lowered > 0.0f, "derivativo: PV subindo reduz a saída (amortecimento)");

  // enableD=false: a derivada NÃO deve influenciar a saída (apenas P).
  p.enableP = true; p.enableD = false; p.kp = 1.0f; p.kd = 2.0f;
  pid.reset();
  pid.compute(50, 30, 0.1f, p, false);                   // prevPv = 30
  float baseD = pid.compute(50, 30, 0.1f, p, false);     // sem variação -> P=20
  float rampD = pid.compute(50, 35, 0.1f, p, false);     // PV subiu, mas D desabilitado -> P=15
  CHECK(nearly(baseD, 20.0f) && nearly(rampD, 15.0f),
        "enableD=false: sem ação derivativa (só P)");

  // enableP=false: sem termo proporcional.
  p.enableP = false; p.enableD = false; p.ki = 0.0f;
  pid.reset();
  float noP = pid.compute(50, 20, 0.1f, p, false);
  CHECK(nearly(noP, 0.0f), "enableP=false: sem termo proporcional");
}

static void test_alarm() {
  std::printf("\n=== ALARME (histerese 80/78) ===\n");
  AlarmFSM a;
  CHECK(a.get() == EstadoAlarme::NORMAL, "estado inicial = NORMAL");

  CHECK(a.update(79.0f) == EstadoAlarme::NORMAL, "79 °C permanece NORMAL");
  CHECK(a.update(80.0f) == EstadoAlarme::ALARME, "80 °C dispara ALARME");
  CHECK(a.get() == EstadoAlarme::ALARME, "get() reflete ALARME");

  // Histerese: em 78.5 (acima de 78) permanece ALARME.
  CHECK(a.update(78.5f) == EstadoAlarme::ALARME, "78.5 °C mantém ALARME (histerese)");
  // Em 78.0 exato: regra é desligar apenas abaixo de 78 -> permanece ALARME.
  CHECK(a.update(78.0f) == EstadoAlarme::ALARME, "78.0 °C mantém ALARME (somente <78 desliga)");
  // Abaixo de 78 desliga.
  CHECK(a.update(77.9f) == EstadoAlarme::NORMAL, "77.9 °C volta a NORMAL");

  // Anti-chattering: ciclo 77.9 -> NORMAL; sobe a 79 -> continua NORMAL; sobe a 80 -> ALARME.
  CHECK(a.update(79.0f) == EstadoAlarme::NORMAL, "após normalizar, 79 °C continua NORMAL");
  CHECK(a.update(80.0f) == EstadoAlarme::ALARME, "80 °C re-dispara ALARME");
}

static void test_tuning() {
  std::printf("\n=== SINTOMA (Fórmulas de sintonia) ===\n");

  // Ziegler-Nichols por relé.
  TuningResult zn = znRelay(10.0f, 12.0f);
  CHECK(zn.valid, "ZN relé: resultado válido");
  CHECK(nearly(zn.kp, 6.0f), "ZN relé: Kp = 0.6*Ku = 6");
  CHECK(nearly(zn.ki, 1.0f), "ZN relé: Ki = Kp/Ti = 1");
  CHECK(nearly(zn.kd, 9.0f), "ZN relé: Kd = Kp*Td = 9");

  // Entradas degeneradas.
  CHECK(!znRelay(0.0f, 10.0f).valid, "ZN relé: Ku<=0 inválido");
  CHECK(!znRelay(5.0f, 0.0f).valid, "ZN relé: Tu<=0 inválido");

  // Cohen-Coon.
  TuningResult cc = cohenCoon(5.0f, 10.0f, 2.0f);
  CHECK(cc.valid, "Cohen-Coon: resultado válido");
  CHECK(nearly(cc.kp, 1.38333f, 0.01f), "Cohen-Coon: Kp calculado");
  CHECK(nearly(cc.ki, 1.38333f / 4.54795f, 0.01f), "Cohen-Coon: Ki = Kp/Ti");
  CHECK(nearly(cc.kd, 1.38333f * 0.70175f, 0.01f), "Cohen-Coon: Kd = Kp*Td");

  // Entradas degeneradas.
  CHECK(!cohenCoon(0.0f, 10.0f, 2.0f).valid, "Cohen-Coon: K=0 inválido");
  CHECK(!cohenCoon(5.0f, 0.0f, 2.0f).valid, "Cohen-Coon: tau<=0 inválido");
  CHECK(!cohenCoon(5.0f, 1.0f, 20.0f).valid, "Cohen-Coon: planta muito atrasada inválido");
}

int main() {
  std::printf("Testes HOST — controle-termico (PID + alarme + sintonia)\n");
  test_pid();
  test_alarm();
  test_tuning();
  std::printf("\n==== %d/%d checks passaram ====\n", checks - failures, checks);
  return failures == 0 ? 0 : 1;
}
