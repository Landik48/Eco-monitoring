#include "gps_sensor.h"

#include <HardwareSerial.h>
#include <TinyGPS++.h>

#include "../core/civil_time.h"
#include "../core/limits.h"

namespace {
TinyGPSPlus gps;
HardwareSerial gpsSerial(2);
}

namespace GpsSensor {
void begin(uint32_t baud, int8_t rxPin, int8_t txPin) {
  gpsSerial.begin(baud, SERIAL_8N1, rxPin, txPin);
}

void pump(uint32_t budgetMs) {
  const uint32_t start = millis();
  while (gpsSerial.available() && (millis() - start) < budgetMs) {
    gps.encode(gpsSerial.read());
  }
}

bool alive() { return gps.charsProcessed() > 10; }

uint8_t satellites() {
  return gps.satellites.isValid() ? (uint8_t)gps.satellites.value() : 0;
}

Fix fix(uint8_t minSats, uint32_t maxAgeMs) {
  Fix f;
  if (!gps.location.isValid()) return f;
  if (gps.location.age() > maxAgeMs) return f;
  if (satellites() < minSats) return f;

  f.valid = true;
  f.lat = gps.location.lat();
  f.lon = gps.location.lng();
  f.alt = gps.altitude.isValid() ? (float)gps.altitude.meters() : 0.0f;
  f.speed = gps.speed.isValid() ? (float)gps.speed.kmph() : 0.0f;
  f.sats = satellites();
  f.hdop = gps.hdop.isValid() ? (float)gps.hdop.hdop() : 0.0f;
  return f;
}

uint32_t utc(uint32_t maxAgeMs) {
  if (!gps.date.isValid() || !gps.time.isValid()) return 0;
  if (gps.date.year() < 2024) return 0;
  if (gps.time.age() > maxAgeMs) return 0;

  const uint32_t epoch = civilToUnix(gps.date.year(), gps.date.month(), gps.date.day(),
                                     gps.time.hour(), gps.time.minute(), gps.time.second());
  if (epoch < TIME_SANE_MIN || epoch > TIME_SANE_MAX) return 0;
  return epoch;
}

}
