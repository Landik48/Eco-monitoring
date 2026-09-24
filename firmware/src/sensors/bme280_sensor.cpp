#include "bme280_sensor.h"

#include <Adafruit_BME280.h>
#include <Wire.h>
#include <math.h>

#include "../../config.h"
#include "../diag/logger.h"
#include "i2c_bus.h"

namespace {
Adafruit_BME280 bme;
bool healthy = false;
}

namespace Bme280Sensor {
bool begin() {
  if (healthy) return true;

  uint8_t addr = 0;
  if (I2CBus::present(ADDR_BME)) addr = ADDR_BME;
  else if (I2CBus::present(ADDR_BME_ALT)) addr = ADDR_BME_ALT;
  if (!addr) return false;

  if (!bme.begin(addr)) return false;

  bme.setSampling(Adafruit_BME280::MODE_FORCED,
                  Adafruit_BME280::SAMPLING_X1,
                  Adafruit_BME280::SAMPLING_X1,
                  Adafruit_BME280::SAMPLING_X1,
                  Adafruit_BME280::FILTER_OFF);
  healthy = true;
  Logger::info("bme_ok", "BME280 инициализирован");
  return true;
}

bool ready() { return healthy; }

void markLost() { healthy = false; }

bool read(Sample& s, float tempOffset) {
  if (!healthy) return false;

  if (!bme.takeForcedMeasurement()) {
    healthy = false;
    s.sensorFault = true;
    Logger::error("bme_lost", "BME280 не ответил на запрос замера");
    return false;
  }

  const float t = bme.readTemperature();
  const float h = bme.readHumidity();
  const float p = bme.readPressure() / 100.0f;

  if (isnan(t) || isnan(h) || isnan(p)) {
    s.sensorFault = true;
    Logger::warn("bme_nan", "BME280 вернул NaN");
    return false;
  }

  s.temp = t + tempOffset;
  s.hum = h;
  s.pres = p;
  return true;
}

}
