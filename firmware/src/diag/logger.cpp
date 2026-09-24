#include "logger.h"

#include <ArduinoJson.h>

#include "../../config.h"

namespace {
Logger::Publisher publisher = nullptr;
Logger::TimeSource timeSource = nullptr;

String queue[LOG_QUEUE_SIZE];
uint8_t count = 0;

String makeJson(const char* level, const char* code, const String& msg) {
  JsonDocument d;
  d["ts"] = timeSource ? timeSource() : 0;
  d["level"] = level;
  d["code"] = code;
  d["msg"] = msg;
  String out;
  serializeJson(d, out);
  return out;
}

void enqueue(const String& json) {
  if (count < LOG_QUEUE_SIZE) {
    queue[count++] = json;
    return;
  }

  for (uint8_t i = 1; i < LOG_QUEUE_SIZE; i++) queue[i - 1] = queue[i];
  queue[LOG_QUEUE_SIZE - 1] = json;
}
}

namespace Logger {
void begin(Publisher p, TimeSource t) {
  publisher = p;
  timeSource = t;
}

void event(const char* level, const char* code, const String& msg) {
  Serial.printf("[%s] %s: %s\n", level, code, msg.c_str());

  const String json = makeJson(level, code, msg);
  if (publisher && publisher(json.c_str())) return;
  enqueue(json);
}

void debug(const char* code, const String& msg) { event("debug", code, msg); }
void info(const char* code, const String& msg) { event("info", code, msg); }
void warn(const char* code, const String& msg) { event("warn", code, msg); }
void error(const char* code, const String& msg) { event("error", code, msg); }

void flush() {
  if (!publisher) return;
  while (count) {
    if (!publisher(queue[0].c_str())) return;
    for (uint8_t i = 1; i < count; i++) queue[i - 1] = queue[i];
    count--;
  }
}

uint8_t pending() { return count; }

}
