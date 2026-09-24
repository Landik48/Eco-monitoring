#pragma once
#include <Arduino.h>

#include "../core/sample.h"

namespace Bme280Sensor {
bool begin();
bool ready();
void markLost();
bool read(Sample& s, float tempOffset);

}
