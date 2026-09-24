#include "time_sync.h"

#include <ArduinoJson.h>

#include "../../config.h"
#include "../diag/logger.h"
#include "../sensors/gps_sensor.h"
#include "../sensors/rtc_clock.h"
#include "mqtt_link.h"

namespace {
TimeKeeper* tk = nullptr;

uint32_t lastGpsCheck = 0;
uint32_t lastRequestSent = 0;
const uint32_t RESYNC_INTERVAL_SEC = TIME_RESYNC_INTERVAL_SEC;
const uint32_t RETRY_WHEN_UNSET_MS = 15000UL;
const uint32_t RETRY_WHEN_SET_MS = 60000UL;

const uint32_t RTC_WRITE_PERIOD_SEC = 3600;
uint32_t lastRtcWrite = 0;
bool rtcEverWritten = false;

void persistToRtc(uint32_t epoch) {
  if (!RtcClock::ready()) return;
  if (rtcEverWritten && (epoch - lastRtcWrite) < RTC_WRITE_PERIOD_SEC) return;

  if (RtcClock::write(epoch)) {
    lastRtcWrite = epoch;
    rtcEverWritten = true;
  } else {
    Logger::warn("rtc_write_fail", "Не удалось записать время в RTC");
  }
}

void sendRequest() {
  if (!MqttLink::connected()) return;

  JsonDocument d;
  d["device_id"] = DEVICE_ID;
  d["nonce"] = millis();
  d["have_time"] = tk && tk->trusted();
  d["source"] = tk ? tk->sourceName() : "none";

  String out;
  serializeJson(d, out);

  if (MqttLink::publish(TOPIC_TIMEREQ, out.c_str(), false)) {
    lastRequestSent = millis();
    if (tk) tk->markResyncRequested(millis());
  }
}
}

namespace TimeSync {
void begin(TimeKeeper* keeper) {
  tk = keeper;
  lastGpsCheck = 0;
  lastRequestSent = 0;
}

void primeFromRtc() {
  if (!tk) return;
  const uint32_t epoch = RtcClock::read();
  if (!epoch) return;

  if (tk->set(epoch, TIME_SRC_RTC, millis())) {
    Logger::info("time_set", "Часы подняты из RTC");
  }
}

void pollGps(bool gpsEnabled) {
  if (!tk || !gpsEnabled) return;
  if (millis() - lastGpsCheck < 5000UL) return;
  lastGpsCheck = millis();

  const uint32_t epoch = GpsSensor::utc(2000);
  if (!epoch) return;

  if (tk->set(epoch, TIME_SRC_GPS, millis())) {
    Logger::info("time_set", "Часы синхронизированы по GPS");
    persistToRtc(epoch);
  }
}

void maintain(bool online) {
  if (!tk || !online) return;

  const bool haveTime = tk->trusted();
  const uint32_t retryMs = haveTime ? RETRY_WHEN_SET_MS : RETRY_WHEN_UNSET_MS;

  if (lastRequestSent && (millis() - lastRequestSent) < retryMs) return;

  if (!haveTime || tk->needsResync(millis(), RESYNC_INTERVAL_SEC)) {
    sendRequest();
  }
}

void requestNow() {
  lastRequestSent = 0;
  sendRequest();
}

bool applyServerEpoch(uint32_t epoch, int32_t rttMs, bool force) {
  if (!tk) return false;

  uint32_t corrected = epoch;
  if (rttMs > 0 && rttMs < 20000) {
    corrected += (uint32_t)((rttMs / 2) / 1000);
  }

  const bool changed = tk->set(corrected, TIME_SRC_SERVER, millis(), force);
  if (changed) {
    Logger::info("time_set", String("Часы синхронизированы с сервером, задержка ") +
                                 String(rttMs) + " мс");
    persistToRtc(corrected);
  }
  return changed;
}

void onServerTime(const uint8_t* payload, unsigned int len) {
  JsonDocument d;
  if (deserializeJson(d, payload, len) != DeserializationError::Ok) {
    Logger::warn("time_parse", "Невалидный ответ сервера о времени");
    return;
  }

  const uint32_t epoch = d["ts"] | 0UL;
  if (!epoch) {
    Logger::warn("time_empty", "Сервер прислал пустое время");
    return;
  }

  int32_t rtt = -1;
  const uint32_t nonce = d["nonce"] | 0UL;
  if (nonce) {
    const uint32_t back = millis() - nonce;
    if (back < 20000UL) rtt = (int32_t)back;
  }

  applyServerEpoch(epoch, rtt);
}

uint32_t now() { return tk ? tk->now(millis()) : 0; }

bool trusted() { return tk && tk->trusted(); }

const char* sourceName() { return tk ? tk->sourceName() : "none"; }

uint32_t secondsSinceSync() {
  return tk ? tk->secondsSinceSync(millis()) : UINT32_MAX;
}

}
