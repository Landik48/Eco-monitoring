#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <ArduinoJson.h>

#define FILTER_MAX 15

struct Settings {
  char     name[32];
  uint32_t publishIntervalMs;
  uint32_t statusIntervalMs;
  uint8_t  filterSize;

  bool     enableBme;
  bool     enableTds;
  bool     enableGps;
  bool     enableBattery;
  bool     enableSd;
  bool     sdBuffering;
  bool     otaEnabled;

  uint8_t  gpsMinSats;
  float    tempOffset;
  float    tdsOffset;
  float    tdsScale;

  uint32_t rev;

  void setDefaults() {
    strlcpy(name, "Станция 1", sizeof(name));
    publishIntervalMs = 60000;
    statusIntervalMs = 30000;
    filterSize = 5;
    gpsMinSats = 4;
    enableBme = enableTds = enableGps = enableBattery = enableSd = true;
    sdBuffering = true;
    otaEnabled = true;
    tempOffset = 0.0f;
    tdsOffset = 0.0f;
    tdsScale = 1.0f;
    rev = 0;
  }

  void toJson(JsonObject o) const {
    o["name"] = name;
    o["publish_interval_ms"] = publishIntervalMs;
    o["status_interval_ms"] = statusIntervalMs;
    o["filter_size"] = filterSize;
    o["enable_bme"] = enableBme;
    o["enable_tds"] = enableTds;
    o["enable_gps"] = enableGps;
    o["enable_battery"] = enableBattery;
    o["enable_sd"] = enableSd;
    o["sd_buffering"] = sdBuffering;
    o["ota_enabled"] = otaEnabled;
    o["gps_min_sats"] = gpsMinSats;
    o["temp_offset"] = tempOffset;
    o["tds_offset"] = tdsOffset;
    o["tds_scale"] = tdsScale;
    o["rev"] = rev;
  }

  void save() const {
    JsonDocument d;
    toJson(d.to<JsonObject>());
    String s;
    serializeJson(d, s);
    Preferences p;
    p.begin("envcfg", false);
    p.putString("json", s);
    p.end();
  }

  void load() {
    setDefaults();
    Preferences p;
    p.begin("envcfg", true);
    String s = p.getString("json", "");
    p.end();
    if (s.isEmpty()) return;

    JsonDocument d;
    if (deserializeJson(d, s) != DeserializationError::Ok) return;
    String err;
    applyJson(d.as<JsonObjectConst>(), err, false);
    rev = d["rev"] | 0;
  }

  bool applyJson(JsonObjectConst o, String& err, bool bumpRev = true) {
    Settings draft = *this;

    bool ok =
      num(o, "publish_interval_ms", draft.publishIntervalMs, (uint32_t)5000, (uint32_t)3600000, err) &&
      num(o, "status_interval_ms", draft.statusIntervalMs, (uint32_t)5000, (uint32_t)600000, err) &&
      num(o, "filter_size", draft.filterSize, (uint8_t)1, (uint8_t)FILTER_MAX, err) &&
      num(o, "gps_min_sats", draft.gpsMinSats, (uint8_t)0, (uint8_t)12, err) &&
      num(o, "temp_offset", draft.tempOffset, -20.0f, 20.0f, err) &&
      num(o, "tds_offset", draft.tdsOffset, -500.0f, 500.0f, err) &&
      num(o, "tds_scale", draft.tdsScale, 0.1f, 10.0f, err);
    if (!ok) return false;

    ok = flag(o, "enable_bme", draft.enableBme, err) &&
         flag(o, "enable_tds", draft.enableTds, err) &&
         flag(o, "enable_gps", draft.enableGps, err) &&
         flag(o, "enable_battery", draft.enableBattery, err) &&
         flag(o, "enable_sd", draft.enableSd, err) &&
         flag(o, "sd_buffering", draft.sdBuffering, err) &&
         flag(o, "ota_enabled", draft.otaEnabled, err);
    if (!ok) return false;

    JsonVariantConst v = o["name"];
    if (!v.isNull()) {
      if (!v.is<const char*>()) {
        err = "name: ожидается строка";
        return false;
      }
      strlcpy(draft.name, v.as<const char*>(), sizeof(draft.name));
    }

    uint32_t keepRev = rev;
    *this = draft;
    rev = bumpRev ? keepRev + 1 : keepRev;
    return true;
  }

private:
  template <typename T>
  static bool num(JsonObjectConst o, const char* key, T& dst, T lo, T hi, String& err) {
    JsonVariantConst v = o[key];
    if (v.isNull()) return true;
    if (!v.is<float>() && !v.is<int>()) { err = String(key) + ": ожидается число"; return false; }
    double x = v.as<double>();
    if (x < (double)lo || x > (double)hi) {
      err = String(key) + ": допустим диапазон " + String((double)lo) + ".." + String((double)hi);
      return false;
    }
    dst = (T)x;
    return true;
  }

  static bool flag(JsonObjectConst o, const char* key, bool& dst, String& err) {
    JsonVariantConst v = o[key];
    if (v.isNull()) return true;
    if (!v.is<bool>()) { err = String(key) + ": ожидается true/false"; return false; }
    dst = v.as<bool>();
    return true;
  }
};
