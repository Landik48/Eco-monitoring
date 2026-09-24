#include "tds_sensor.h"

#include <math.h>

#include "../../config.h"
#include "../diag/logger.h"
#include "i2c_bus.h"

#if USE_TDS_SENSOR
#include <Wire.h>
#include <iarduino_I2C_TDS.h>

namespace {
iarduino_I2C_TDS tds(ADDR_TDS);
bool healthy = false;
}
#endif

namespace TdsSensor {
#if USE_TDS_SENSOR

bool begin() {
  if (healthy) return true;
  if (!I2CBus::present(ADDR_TDS)) return false;
  if (!tds.begin(&Wire)) return false;
  healthy = true;
  Logger::info("tds_ok", "Датчик TDS/EC инициализирован");
  return true;
}

bool ready() { return healthy; }

void markLost() { healthy = false; }

bool read(Sample& s, float waterTemp, float scale, float offset) {
  if (!healthy) return false;

#if TDS_TEMP_COMPENSATION

  if (!isnan(waterTemp)) {
    tds.set_t(constrain(waterTemp, 0.0f, 63.75f));
  }
#endif

  const float rawTds = tds.getTDS();
  const float rawEc = tds.getEC();

  if (isnan(rawTds) || isnan(rawEc)) {
    s.sensorFault = true;
    Logger::warn("tds_nan", "Датчик TDS вернул NaN");
    return false;
  }

  s.tds = rawTds * scale + offset;
  s.ec = rawEc * scale;
  return true;
}

#else

bool begin() { return false; }
bool ready() { return false; }
void markLost() {}
bool read(Sample&, float, float, float) { return false; }

#endif

}
