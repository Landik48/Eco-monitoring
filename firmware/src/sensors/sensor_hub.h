#pragma once
#include <Arduino.h>

#include "../../settings.h"
#include "../core/running_average.h"
#include "../core/sample.h"
#include "gps_sensor.h"

namespace SensorHub {
struct Health {
  bool bme = false;
  bool rtc = false;
  bool tds = false;
  bool power = false;
  bool sd = false;
  bool gps = false;
};

void begin(const Settings& cfg);
void retryInit(const Settings& cfg, bool verbose);
void applyFilterSize(uint8_t size);
Sample readRaw(const Settings& cfg);
void accept(const Sample& raw);
Sample averaged();
bool hasData();
const Health& health();

}
