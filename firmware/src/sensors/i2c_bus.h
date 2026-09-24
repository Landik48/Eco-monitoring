#pragma once
#include <Arduino.h>

namespace I2CBus {
void begin(uint8_t sda, uint8_t scl);

bool present(uint8_t addr);

}
