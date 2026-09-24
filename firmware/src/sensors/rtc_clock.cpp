#include "rtc_clock.h"

#include "RTClib.h"

#include "../../config.h"
#include "../core/limits.h"
#include "../diag/logger.h"
#include "i2c_bus.h"

namespace {
RTC_DS3231 rtc;
bool healthy = false;
bool powerWasLost = false;
}

namespace RtcClock {
bool begin() {
  if (healthy) return true;
  if (!I2CBus::present(ADDR_RTC)) return false;
  if (!rtc.begin()) return false;

  healthy = true;
  powerWasLost = rtc.lostPower();

  if (powerWasLost) {
    Logger::warn("rtc_lost_power",
                 "RTC потерял питание: время будет взято у GPS или сервера");
  } else {
    Logger::info("rtc_ok", "DS3231 инициализирован");
  }
  return true;
}

bool ready() { return healthy; }

bool lostPower() { return powerWasLost; }

uint32_t read() {
  if (!healthy) return 0;

  if (powerWasLost) return 0;

  const DateTime now = rtc.now();
  if (!now.isValid()) return 0;

  const uint32_t epoch = now.unixtime();
  if (epoch < TIME_SANE_MIN || epoch > TIME_SANE_MAX) return 0;
  return epoch;
}

bool write(uint32_t epoch) {
  if (!healthy) return false;
  if (epoch < TIME_SANE_MIN || epoch > TIME_SANE_MAX) return false;

  rtc.adjust(DateTime(epoch));

  const DateTime back = rtc.now();
  if (!back.isValid()) return false;
  const int32_t diff = (int32_t)((int64_t)back.unixtime() - (int64_t)epoch);
  if (diff < -3 || diff > 3) return false;

  clearLostPower();
  return true;
}

void clearLostPower() { powerWasLost = false; }

}
