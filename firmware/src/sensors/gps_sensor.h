#pragma once
#include <Arduino.h>

namespace GpsSensor {
struct Fix {
  bool   valid = false;
  double lat = 0;
  double lon = 0;
  float  alt = 0;
  float  speed = 0;
  uint8_t sats = 0;
  float  hdop = 0;
};

void begin(uint32_t baud, int8_t rxPin, int8_t txPin);
void pump(uint32_t budgetMs);
bool alive();
Fix fix(uint8_t minSats, uint32_t maxAgeMs);
uint32_t utc(uint32_t maxAgeMs);
uint8_t satellites();

}
