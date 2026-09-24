#include "sensor_hub.h"

#include <math.h>

#include "../../config.h"
#include "../diag/logger.h"
#include "bme280_sensor.h"
#include "i2c_bus.h"
#include "power_sensor.h"
#include "rtc_clock.h"
#include "sd_storage.h"
#include "tds_sensor.h"

namespace {
SensorHub::Health healthState;

RunningAverage avgTemp, avgHum, avgPres, avgTds, avgEc, avgVbat, avgIbat;

RunningAverage* const ALL[] = {&avgTemp, &avgHum, &avgPres,
                               &avgTds,  &avgEc,  &avgVbat, &avgIbat};

void refreshHealth() {
  healthState.bme = Bme280Sensor::ready();
  healthState.rtc = RtcClock::ready();
  healthState.tds = TdsSensor::ready();
  healthState.power = PowerSensor::ready();
  healthState.sd = SdStorage::ready();
  healthState.gps = GpsSensor::alive();
}

}

namespace SensorHub {
void applyFilterSize(uint8_t size) {
  for (RunningAverage* a : ALL) a->setSize(size);
}

void begin(const Settings& cfg) {
  I2CBus::begin(PIN_I2C_SDA, PIN_I2C_SCL);
  GpsSensor::begin(GPS_BAUD, PIN_GPS_RX, PIN_GPS_TX);
  applyFilterSize(cfg.filterSize);
  retryInit(cfg, true);
}

void retryInit(const Settings& cfg, bool verbose) {
  if (cfg.enableBme && !Bme280Sensor::ready()) {
    if (!Bme280Sensor::begin() && verbose) {
      Logger::error("bme_fail", "BME280 не отвечает");
    }
  }

  if (!RtcClock::ready()) {
    if (!RtcClock::begin() && verbose) {
      Logger::error("rtc_fail", "DS3231 не отвечает");
    }
  }

  if (cfg.enableTds && !TdsSensor::ready()) {
    if (!TdsSensor::begin() && verbose) {
      Logger::error("tds_fail", "TDS-модуль не отвечает");
    }
  }

  if (cfg.enableBattery && !PowerSensor::ready()) {
    if (!PowerSensor::begin() && verbose) {
      Logger::error("pwr_fail", "Модуль питания не отвечает");
    }
  }

  if (cfg.enableSd && !SdStorage::ready()) {
    if (!SdStorage::begin(PIN_SD_CS) && verbose) {
      Logger::error("sd_fail", "SD-карта недоступна");
    }
  }

  refreshHealth();
}

Sample readRaw(const Settings& cfg) {
  Sample s;
  s.upMs = millis();

  if (cfg.enableBattery && PowerSensor::ready()) {
    PowerSensor::read(s);
  }

  if (cfg.enableBme && Bme280Sensor::ready()) {
    Bme280Sensor::read(s, cfg.tempOffset);
  }

  if (cfg.enableTds && TdsSensor::ready()) {
    float t = Sample::has(s.temp) ? s.temp : avgTemp.value();
    TdsSensor::read(s, t, cfg.tdsScale, cfg.tdsOffset);
  }

  refreshHealth();
  return s;
}

void accept(const Sample& raw) {
  if (Sample::has(raw.temp)) avgTemp.push(raw.temp);
  if (Sample::has(raw.hum)) avgHum.push(raw.hum);
  if (Sample::has(raw.pres)) avgPres.push(raw.pres);
  if (Sample::has(raw.tds)) avgTds.push(raw.tds);
  if (Sample::has(raw.ec)) avgEc.push(raw.ec);
  if (Sample::has(raw.vbat)) avgVbat.push(raw.vbat);
  if (Sample::has(raw.ibat)) avgIbat.push(raw.ibat);
}

Sample averaged() {
  Sample s;
  s.temp = avgTemp.value();
  s.hum = avgHum.value();
  s.pres = avgPres.value();
  s.tds = avgTds.value();
  s.ec = avgEc.value();
  s.vbat = avgVbat.value();
  s.ibat = avgIbat.value();
  return s;
}

bool hasData() {
  for (const RunningAverage* a : ALL) {
    if (a->valid()) return true;
  }
  return false;
}

const Health& health() { return healthState; }

}
