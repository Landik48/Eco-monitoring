#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

namespace CommandRouter {
struct Handlers {
  void (*publishStatus)() = nullptr;
  void (*publishNow)() = nullptr;
  void (*clearSpool)() = nullptr;
  void (*factoryReset)() = nullptr;
  void (*scheduleReboot)(uint32_t delayMs) = nullptr;
  void (*resetStability)() = nullptr;
  void (*syncTime)() = nullptr;
  bool (*setTime)(uint32_t epoch) = nullptr;
};

void begin(const Handlers& handlers);
void execute(JsonObjectConst cmd);
void sendAck(const String& id, bool ok, const String& error);

}
