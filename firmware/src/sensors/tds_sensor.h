#pragma once
#include <Arduino.h>

#include "../core/sample.h"

namespace TdsSensor {
bool begin();
bool ready();
void markLost();

bool read(Sample& s, float waterTemp, float scale, float offset);

}
