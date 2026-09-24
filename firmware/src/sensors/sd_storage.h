#pragma once
#include <Arduino.h>

#include "../../spool.h"

namespace SdStorage {
bool begin(uint8_t csPin);
bool ready();
Spool& spool();
void stats(uint16_t& files, uint32_t& bytes);

}
