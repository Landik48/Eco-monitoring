#include "sd_storage.h"

#include <SD.h>
#include <SPI.h>

#include "../diag/logger.h"

namespace {
Spool spoolInstance;
bool healthy = false;
}

namespace SdStorage {
bool begin(uint8_t csPin) {
  if (healthy) return true;
  if (!SD.begin(csPin)) return false;
  if (!spoolInstance.begin()) return false;
  healthy = true;
  Logger::info("sd_ok", "SD-карта готова, буфер открыт");
  return true;
}

bool ready() { return healthy; }

Spool& spool() { return spoolInstance; }

void stats(uint16_t& files, uint32_t& bytes) {
  files = 0;
  bytes = 0;
  if (healthy) spoolInstance.stats(files, bytes);
}

}
