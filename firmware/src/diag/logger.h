#pragma once
#include <Arduino.h>

namespace Logger {
typedef bool (*Publisher)(const char* json);
typedef uint32_t (*TimeSource)();
void begin(Publisher publisher, TimeSource timeSource);
void debug(const char* code, const String& msg);
void info(const char* code, const String& msg);
void warn(const char* code, const String& msg);
void error(const char* code, const String& msg);
void event(const char* level, const char* code, const String& msg);
void flush();
uint8_t pending();

}
