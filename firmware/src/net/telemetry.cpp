#include "telemetry.h"

#include <ArduinoJson.h>
#include <math.h>

#include "../../config.h"
#include "mqtt_link.h"
#include "time_sync.h"

namespace {
float round2(float v) { return roundf(v * 100.0f) / 100.0f; }
float round3(float v) { return roundf(v * 1000.0f) / 1000.0f; }

void put(JsonDocument& d, const char* key, float value, bool three = false) {
  if (Sample::has(value)) d[key] = three ? round3(value) : round2(value);
  else                    d[key] = nullptr;
}

}

namespace Telemetry {
String buildMeasurement(const Sample& s, const GpsSensor::Fix& fix,
                        const StabilityVerdict& verdict, const char* bootId) {
  JsonDocument d;

  d["seq"] = s.seq;
  d["ts"] = s.ts;
  d["ts_valid"] = s.ts > 0;
  d["time_source"] = TimeSync::sourceName();
  d["up_ms"] = s.upMs;
  d["boot"] = bootId;
  d["rssi"] = MqttLink::rssi();

  put(d, "temp", s.temp);
  put(d, "hum", s.hum);
  put(d, "pres", s.pres);
  put(d, "tds", s.tds);
  put(d, "ec", s.ec);
  put(d, "vbat", s.vbat);
  put(d, "ibat", s.ibat, true);

  d["flags"] = verdict.flags;
  d["suspect"] = verdict.suspect;
  d["confirms_previous"] = verdict.confirmsPrevious;
  d["stability"] = verdict.score;

  d["gps_valid"] = fix.valid;
  if (fix.valid) {
    d["lat"] = serialized(String(fix.lat, 6));
    d["lon"] = serialized(String(fix.lon, 6));
    d["alt"] = round2(fix.alt);
    d["speed"] = round2(fix.speed);
    d["sats"] = fix.sats;
    d["hdop"] = round2(fix.hdop);
  } else {
    d["lat"] = nullptr;
    d["lon"] = nullptr;
    d["alt"] = nullptr;
    d["speed"] = nullptr;
    d["sats"] = 0;
    d["hdop"] = nullptr;
  }

  String out;
  serializeJson(d, out);
  return out;
}

String buildStatus(const StatusInput& in) {
  JsonDocument d;

  d["device_id"] = DEVICE_ID;
  d["online"] = true;
  d["ts"] = TimeSync::now();
  d["name"] = in.name;
  d["fw"] = FW_VERSION;
  d["boot"] = in.bootId;
  d["uptime_s"] = millis() / 1000;
  d["ip"] = MqttLink::ip();
  d["rssi"] = MqttLink::rssi();
  d["free_heap"] = ESP.getFreeHeap();
  d["rev"] = in.rev;
  d["sent"] = MqttLink::sentCount();
  d["spooled"] = in.spooledTotal;
  d["failed"] = MqttLink::failedCount();

  d["time_valid"] = TimeSync::trusted();
  d["time_source"] = TimeSync::sourceName();
  d["time_age_s"] = TimeSync::secondsSinceSync();

  d["publish_interval_ms"] = in.publishIntervalMs;
  d["status_interval_ms"] = in.statusIntervalMs;

  JsonObject sensors = d["sensors"].to<JsonObject>();
  sensors["bme"] = in.bmeOk;
  sensors["rtc"] = in.rtcOk;
  sensors["tds"] = in.tdsOk;
  sensors["battery"] = in.powerOk;
  sensors["sd"] = in.sdOk;
  sensors["gps"] = in.gpsOk;

  d["spool_files"] = in.spoolFiles;
  d["spool_bytes"] = in.spoolBytes;

  if (in.stability) {
    JsonObject st = d["stability"].to<JsonObject>();
    st["score"] = in.stability->score();
    st["state"] = in.stability->state();
    st["sag_events"] = in.stability->sagEvents();
    st["suspect_samples"] = in.stability->suspectCount();
    st["vbat_min"] = round2(in.stability->vbatMinSeen());
    st["last_sag_ts"] = in.stability->lastSagTs();
  }

  String out;
  serializeJson(d, out);
  return out;
}

}
