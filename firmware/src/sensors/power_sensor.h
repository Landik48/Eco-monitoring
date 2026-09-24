#pragma once
#include <Arduino.h>

#include "../core/sample.h"

namespace PowerSensor {
bool begin();
bool ready();
void markLost();

bool read(Sample& s);

}
