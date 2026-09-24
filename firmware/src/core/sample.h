#pragma once
#include <math.h>
#include <stdint.h>

struct Sample {
  uint32_t ts = 0;
  uint32_t upMs = 0;
  uint32_t seq = 0;

  float temp = NAN;
  float hum  = NAN;
  float pres = NAN;
  float tds  = NAN;
  float ec   = NAN;
  float vbat = NAN;
  float ibat = NAN;

  bool sensorFault = false;

  static bool has(float v) { return !isnan(v) && !isinf(v); }
};
