#pragma once
// web_server.h — Access Point + servidor HTTP (dashboard e endpoints JSON).

#include <Arduino.h>
#include "state.h"

class WebServer {
public:
  void begin(ControlState* cs);
  void handle();               // chame a cada loop()
  const char* getSSID() const;

private:
  ControlState* cs_;
  char ssid_[32];   // SSID do AP (buffers fixos — sem String no caminho periódico)
};
