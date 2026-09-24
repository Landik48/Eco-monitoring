#include "i2c_bus.h"

#include <Wire.h>

namespace I2CBus {
void begin(uint8_t sda, uint8_t scl) {
  Wire.begin(sda, scl);
}

bool present(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

}
