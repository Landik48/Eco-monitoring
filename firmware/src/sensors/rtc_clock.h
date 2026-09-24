#pragma once
#include <Arduino.h>

namespace RtcClock {
bool begin();
bool ready();
bool lostPower();
uint32_t read();
bool write(uint32_t epoch);
void clearLostPower();

}
