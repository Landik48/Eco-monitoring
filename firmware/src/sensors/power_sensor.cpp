#include "power_sensor.h"

#include <math.h>

#include "../../config.h"
#include "../diag/logger.h"
#include "i2c_bus.h"

#if USE_BATTERY_SHIELD
#include <Wire.h>

#include "Battery_Shield.h"

namespace {
Battery_Shield pwrBank(ADDR_PWR);
bool healthy = false;

const float VBAT_MIN_PLAUSIBLE = 0.5f;
const float VBAT_MAX_PLAUSIBLE = 15.0f;
const float IBAT_MAX_PLAUSIBLE = 10.0f;
}
#endif

namespace PowerSensor {
#if USE_BATTERY_SHIELD

bool begin() {
  if (healthy) return true;
  if (!I2CBus::present(ADDR_PWR)) return false;
  pwrBank.begin(&Wire, PWR_SHUNT);
  healthy = true;
  Logger::info("pwr_ok", "Модуль питания инициализирован");
  return true;
}

bool ready() { return healthy; }

void markLost() { healthy = false; }

bool read(Sample& s) {
  if (!healthy) return false;

  const float v = pwrBank.voltmeter(BATTERY);
  const float i = pwrBank.amperemeter(BATTERY);

  bool any = false;

  if (!isnan(v) && v > VBAT_MIN_PLAUSIBLE && v < VBAT_MAX_PLAUSIBLE) {
    s.vbat = v;
    any = true;
  } else if (!isnan(v)) {
    s.sensorFault = true;
  }

  if (!isnan(i) && fabsf(i) < IBAT_MAX_PLAUSIBLE) {
    s.ibat = i;
    any = true;
  }

  return any;
}

#else

bool begin() { return false; }
bool ready() { return false; }
void markLost() {}
bool read(Sample&) { return false; }

#endif

}
