#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <math.h>
#include <esp_task_wdt.h>
#include <ArduinoJson.h>
#include <Adafruit_BME280.h>
#include "RTClib.h"
#include <TinyGPS++.h>
#include <HardwareSerial.h>

#include "config.h"
#if USE_TDS_SENSOR
#include <iarduino_I2C_TDS.h>
#endif
#if USE_BATTERY_SHIELD
#include "Battery_Shield.h"
#endif
#include "settings.h"
#include "spool.h"

#ifndef ESP_ARDUINO_VERSION_MAJOR
#define ESP_ARDUINO_VERSION_MAJOR 2
#endif

Adafruit_BME280   bme;
RTC_DS3231        rtc;
#if USE_TDS_SENSOR
iarduino_I2C_TDS  tds(ADDR_TDS);
#endif
#if USE_BATTERY_SHIELD
Battery_Shield    pwrBank(ADDR_PWR);
#endif
TinyGPSPlus       gps;
HardwareSerial    gpsSerial(2);

WiFiClient   espClient;
PubSubClient mqtt(espClient);

Settings cfg;
Spool    spool;

struct Health { bool bme = false, rtc = false, tds = false, pwr = false, sd = false; };
Health health;

uint32_t seqNo = 0;

char bootId[9] = "00000000";

uint32_t timeBase    = 0;
uint32_t timeBaseMs  = 0;
bool     timeTrusted = false;
const char* timeSource = "none";

uint32_t lastGpsTimeCheck = 0;
uint32_t lastSample = 0, lastStatus = 0, lastWifiTry = 0, lastMqttTry = 0;
uint32_t lastSensorRetry = 0, lastDrain = 0;
uint32_t mqttBackoff = MQTT_BACKOFF_MIN_MS;
uint32_t sentTotal = 0, spooledTotal = 0, failTotal = 0;
uint32_t rebootAt = 0;

bool otaStarted = false, wasConnected = false;

uint32_t nowUnix();
void logEvent(const char* level, const char* code, const String& msg);

class Avg {
public:
  void setSize(uint8_t s) {
    if (s < 1) s = 1;
    if (s > FILTER_MAX) s = FILTER_MAX;
    if (s != _size) { _size = s; reset(); }
  }
  void reset() { _count = 0; _idx = 0; }
  void push(float v) {
    if (isnan(v) || isinf(v)) return;
    _buf[_idx] = v;
    _idx = (_idx + 1) % _size;
    if (_count < _size) _count++;
  }
  bool  valid() const { return _count > 0; }
  float value() const {
    float s = 0;
    for (uint8_t i = 0; i < _count; i++) s += _buf[i];
    return s / _count;
  }
private:
  float   _buf[FILTER_MAX] = {0};
  uint8_t _size = 5, _idx = 0, _count = 0;
};

Avg avgTemp, avgHum, avgPres, avgTds, avgEc, avgVbat, avgIbat;

void applyFilterSize() {
  Avg* all[] = {&avgTemp, &avgHum, &avgPres, &avgTds, &avgEc, &avgVbat, &avgIbat};
  for (Avg* a : all) a->setSize(cfg.filterSize);
}

static float round2(float v) { return roundf(v * 100.0f) / 100.0f; }
static float round3(float v) { return roundf(v * 1000.0f) / 1000.0f; }

static void putAvg(JsonDocument& d, const char* key, const Avg& a) {
  if (a.valid()) d[key] = round2(a.value());
  else           d[key] = nullptr;
}

static bool i2cPresent(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

void initSensors(bool verbose) {
  if (cfg.enableBme && !health.bme) {
    uint8_t a = i2cPresent(ADDR_BME) ? ADDR_BME : (i2cPresent(ADDR_BME_ALT) ? ADDR_BME_ALT : 0);
    if (a && bme.begin(a)) {
      bme.setSampling(Adafruit_BME280::MODE_FORCED,
                      Adafruit_BME280::SAMPLING_X1,
                      Adafruit_BME280::SAMPLING_X1,
                      Adafruit_BME280::SAMPLING_X1,
                      Adafruit_BME280::FILTER_OFF);
      health.bme = true;
      logEvent("info", "bme_ok", "BME280 инициализирован");
    } else if (verbose) {
      logEvent("error", "bme_fail", "BME280 не отвечает");
    }
  }

  if (!health.rtc) {
    if (i2cPresent(ADDR_RTC) && rtc.begin()) {
      health.rtc = true;
      DateTime n = rtc.now();
      if (rtc.lostPower()) {
        logEvent("warn", "rtc_lost_power", "RTC потерял питание, время считается недостоверным");
      } else if (n.year() >= 2023) {
        timeBase = n.unixtime(); timeBaseMs = millis();
        timeSource = "rtc"; timeTrusted = true;
      } else if (verbose) {
        logEvent("warn", "rtc_unset", "RTC не выставлен, ждём GPS или сервер");
      }
    } else if (verbose) {
      logEvent("error", "rtc_fail", "DS3231 не отвечает");
    }
  }

#if USE_TDS_SENSOR
  if (cfg.enableTds && !health.tds) {
    if (i2cPresent(ADDR_TDS) && tds.begin(&Wire)) health.tds = true;
    else if (verbose) logEvent("error", "tds_fail", "TDS-модуль не отвечает");
  }
#endif

#if USE_BATTERY_SHIELD
  if (cfg.enableBattery && !health.pwr) {
    if (i2cPresent(ADDR_PWR)) { pwrBank.begin(&Wire, PWR_SHUNT); health.pwr = true; }
    else if (verbose) logEvent("error", "pwr_fail", "Battery Shield не отвечает");
  }
#endif

  if (cfg.enableSd && !health.sd) {
    if (SD.begin(PIN_SD_CS)) health.sd = spool.begin();
    if (!health.sd && verbose) logEvent("error", "sd_fail", "SD-карта недоступна");
  }
}

uint32_t nowUnix() {
  if (!timeTrusted) return 0;
  if (health.rtc) {
    DateTime n = rtc.now();
    if (n.year() >= 2023) return n.unixtime();
  }

  return timeBase + (millis() - timeBaseMs) / 1000;
}

void setTime(uint32_t epoch, const char* source) {
  if (epoch < 1700000000UL) return;
  timeBase   = epoch;
  timeBaseMs = millis();
  timeSource = source;
  bool first = !timeTrusted;
  timeTrusted = true;
  if (health.rtc) rtc.adjust(DateTime(epoch));
  logEvent("info", "time_set",
           String(first ? "Время установлено, источник " : "Время уточнено, источник ") + source);
}

static int32_t timeDrift(uint32_t candidate) {
  uint32_t local = nowUnix();
  if (!local) return INT32_MAX;
  return (int32_t)((int64_t)candidate - (int64_t)local);
}

void syncTimeFromGps() {
  if (!cfg.enableGps) return;
  if (!gps.date.isValid() || !gps.time.isValid()) return;
  if (gps.date.year() < 2023) return;
  if (gps.time.age() > 2000) return;

  DateTime g(gps.date.year(), gps.date.month(), gps.date.day(),
             gps.time.hour(), gps.time.minute(), gps.time.second());
  int32_t drift = timeDrift(g.unixtime());
  if (drift != INT32_MAX && abs(drift) < 5) return;
  setTime(g.unixtime(), "gps");
}

void syncTimeFromServer(uint32_t serverTs) {
  if (serverTs < 1700000000UL) return;

  if (timeTrusted && !strcmp(timeSource, "gps")) return;
  int32_t drift = timeDrift(serverTs);
  if (drift != INT32_MAX && abs(drift) < 120) return;
  setTime(serverTs, "server");
}

String   logQueue[LOG_QUEUE_SIZE];
uint8_t  logCount = 0;

static String makeLogJson(const char* level, const char* code, const String& msg) {
  JsonDocument d;
  d["ts"]    = nowUnix();
  d["level"] = level;
  d["code"]  = code;
  d["msg"]   = msg;
  String out;
  serializeJson(d, out);
  return out;
}

void logEvent(const char* level, const char* code, const String& msg) {
  Serial.printf("[%s] %s: %s\n", level, code, msg.c_str());
  String json = makeLogJson(level, code, msg);

  if (mqtt.connected() && mqtt.publish(TOPIC_LOG, json.c_str(), false)) return;

  if (logCount < LOG_QUEUE_SIZE) {
    logQueue[logCount++] = json;
  } else {
    for (uint8_t i = 1; i < LOG_QUEUE_SIZE; i++) logQueue[i - 1] = logQueue[i];
    logQueue[LOG_QUEUE_SIZE - 1] = json;
  }
}

void flushLogQueue() {
  while (logCount && mqtt.connected()) {
    if (!mqtt.publish(TOPIC_LOG, logQueue[0].c_str(), false)) return;
    for (uint8_t i = 1; i < logCount; i++) logQueue[i - 1] = logQueue[i];
    logCount--;
  }
}

void sendAck(const String& id, bool ok, const String& error) {
  if (id.isEmpty()) return;
  JsonDocument d;
  d["id"]   = id;
  d["ok"]   = ok;
  d["ts"]   = nowUnix();
  d["rev"]  = cfg.rev;
  if (!ok) d["error"] = error;
  String out;
  serializeJson(d, out);
  mqtt.publish(TOPIC_ACK, out.c_str(), false);
}

void buildStatus(JsonObject s) {
  s["device_id"]   = DEVICE_ID;
  s["online"]      = true;
  s["ts"]          = nowUnix();
  s["name"]        = cfg.name;
  s["fw"]          = FW_VERSION;
  s["boot"]        = bootId;
  s["uptime_s"]    = millis() / 1000;
  s["ip"]          = WiFi.localIP().toString();
  s["rssi"]        = (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : 0;
  s["free_heap"]   = ESP.getFreeHeap();
  s["rev"]         = cfg.rev;
  s["sent"]        = sentTotal;
  s["spooled"]     = spooledTotal;
  s["failed"]      = failTotal;
  s["time_valid"]  = timeTrusted;
  s["time_source"] = timeSource;
  s["publish_interval_ms"] = cfg.publishIntervalMs;
  s["status_interval_ms"]  = cfg.statusIntervalMs;

  JsonObject h = s["sensors"].to<JsonObject>();
  h["bme"] = health.bme; h["rtc"] = health.rtc; h["tds"] = health.tds;
  h["battery"] = health.pwr; h["sd"] = health.sd;
  h["gps"] = gps.charsProcessed() > 10;

  uint16_t files = 0; uint32_t bytes = 0;
  if (health.sd) spool.stats(files, bytes);
  s["spool_files"] = files;
  s["spool_bytes"] = bytes;
}

void publishStatus() {
  if (!mqtt.connected()) return;
  JsonDocument d;
  buildStatus(d.to<JsonObject>());
  String out;
  serializeJson(d, out);
  mqtt.publish(TOPIC_STATUS, out.c_str(), true);
  lastStatus = millis();
}

static bool isDangerous(const char* cmd) {
  return !strcmp(cmd, "reboot") || !strcmp(cmd, "factory_reset") ||
         !strcmp(cmd, "clear_spool");
}

void execCommand(JsonObjectConst c) {
  String id = c["id"] | "";
  const char* cmd = c["cmd"] | "";
  if (!*cmd) { sendAck(id, false, "не указано поле cmd"); return; }

  if (isDangerous(cmd) && !(c["confirm"] | false)) {
    sendAck(id, false, "требуется confirm: true");
    return;
  }

  if (!strcmp(cmd, "ping")) {
    sendAck(id, true, "");
  } else if (!strcmp(cmd, "publish_now")) {
    lastSample = millis() - cfg.publishIntervalMs;
    sendAck(id, true, "");
  } else if (!strcmp(cmd, "get_status")) {
    publishStatus();
    sendAck(id, true, "");
  } else if (!strcmp(cmd, "clear_spool")) {
    spool.clear();
    sendAck(id, true, "");
    logEvent("warn", "spool_cleared", "Буфер SD очищен по команде");
  } else if (!strcmp(cmd, "factory_reset")) {
    cfg.setDefaults();
    cfg.save();
    sendAck(id, true, "");
    rebootAt = millis() + 3000;
  } else if (!strcmp(cmd, "reboot")) {
    sendAck(id, true, "");
    rebootAt = millis() + 3000;
  } else {
    sendAck(id, false, "неизвестная команда");
  }
}

void applySettings(JsonObjectConst o) {
  uint32_t incomingRev = o["rev"] | 0;
  if (incomingRev && incomingRev == cfg.rev) return;

  String err;
  if (!cfg.applyJson(o, err)) {
    logEvent("error", "settings_reject", err);
    return;
  }
  cfg.rev = incomingRev ? incomingRev : cfg.rev;
  cfg.save();
  applyFilterSize();
  initSensors(false);
  publishStatus();
  logEvent("info", "settings_applied", "Конфигурация обновлена, rev=" + String(cfg.rev));
}

void onMqttMessage(char* topic, byte* payload, unsigned int len) {
  JsonDocument d;
  if (deserializeJson(d, payload, len) != DeserializationError::Ok) {
    logEvent("warn", "msg_parse", String("Невалидный JSON в топике ") + topic);
    return;
  }
  if (!strcmp(topic, TOPIC_CMD))      execCommand(d.as<JsonObjectConst>());
  else if (!strcmp(topic, TOPIC_SETTINGS)) applySettings(d.as<JsonObjectConst>());
}

String buildMeasurement() {
  JsonDocument d;

  uint32_t ts = nowUnix();
  d["seq"]      = ++seqNo;
  d["ts"]       = ts;
  d["ts_valid"] = ts > 0;
  d["up_ms"]    = millis();
  d["boot"]     = bootId;
  d["rssi"]     = (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : 0;

  if (health.bme) {
    if (bme.takeForcedMeasurement()) {
      float t = bme.readTemperature();
      float h = bme.readHumidity();
      float p = bme.readPressure() / 100.0f;
      if (isnan(t) || isnan(h) || isnan(p)) logEvent("warn", "bme_nan", "BME280 вернул NaN");
      avgTemp.push(t + cfg.tempOffset);
      avgHum.push(h);
      avgPres.push(p);
    } else {
      health.bme = false;
    }
  }
  putAvg(d, "temp", avgTemp);
  putAvg(d, "hum", avgHum);
  putAvg(d, "pres", avgPres);

#if USE_TDS_SENSOR
  if (health.tds) {
#if TDS_TEMP_COMPENSATION

    if (avgTemp.valid()) {
      tds.set_t(constrain(avgTemp.value(), 0.0f, 63.75f));
    }
#endif
    avgTds.push(tds.getTDS() * cfg.tdsScale + cfg.tdsOffset);
    avgEc.push(tds.getEC() * cfg.tdsScale);
  }
#endif
  putAvg(d, "tds", avgTds);
  putAvg(d, "ec", avgEc);

#if USE_BATTERY_SHIELD
  if (health.pwr) {
    float v = pwrBank.voltmeter(BATTERY);
    float i = pwrBank.amperemeter(BATTERY);
    if (v > 0.5f && v < 15.0f) avgVbat.push(v);
    avgIbat.push(i);
  }
#endif
  putAvg(d, "vbat", avgVbat);
  if (avgIbat.valid()) d["ibat"] = round3(avgIbat.value());
  else                 d["ibat"] = nullptr;

  bool fix = cfg.enableGps && gps.location.isValid() &&
             gps.location.age() < 10000 &&
             gps.satellites.value() >= cfg.gpsMinSats;
  d["gps_valid"] = fix;
  if (fix) {
    d["lat"]   = serialized(String(gps.location.lat(), 6));
    d["lon"]   = serialized(String(gps.location.lng(), 6));
    d["alt"]   = gps.altitude.isValid() ? round2(gps.altitude.meters()) : 0.0f;
    d["speed"] = gps.speed.isValid() ? round2(gps.speed.kmph()) : 0.0f;
    d["sats"]  = gps.satellites.value();
    d["hdop"]  = gps.hdop.isValid() ? round2(gps.hdop.hdop()) : 0.0f;
  } else {
    d["lat"] = nullptr; d["lon"] = nullptr; d["alt"] = nullptr;
    d["speed"] = nullptr; d["sats"] = 0; d["hdop"] = nullptr;
  }

  String s;
  serializeJson(d, s);
  return s;
}

bool publishMeasurement(const String& record) {
  if (!mqtt.connected()) return false;
  if (!mqtt.publish(TOPIC_TELEMETRY, record.c_str(), false)) return false;
  sentTotal++;
  return true;
}

void drainSpool() {
  if (!mqtt.connected() || !health.sd) return;
  if (millis() - lastDrain < SPOOL_DRAIN_PERIOD_MS) return;
  lastDrain = millis();
  if (!spool.pending()) return;

  uint16_t n = spool.drainLines([](const char* line) {
    return mqtt.publish(TOPIC_BACKLOG, line);
  }, SPOOL_DRAIN_BATCH);

  if (n) { sentTotal += n; mqtt.loop(); }
}

void connectMqtt() {
  static const char* WILL =
    "{\"device_id\":\"" DEVICE_ID "\",\"online\":false}";

  if (mqtt.connect(DEVICE_ID, MQTT_USER, MQTT_PASSWORD,
                   TOPIC_STATUS, 0, true, WILL)) {
    mqttBackoff = MQTT_BACKOFF_MIN_MS;
    mqtt.subscribe(TOPIC_CMD, 1);
    mqtt.subscribe(TOPIC_SETTINGS, 1);
    publishStatus();
    flushLogQueue();
    logEvent("info", "mqtt_connected", "Соединение с брокером установлено");
  } else {
    failTotal++;
    Serial.printf("MQTT ошибка, state=%d, повтор через %lu мс\n",
                  mqtt.state(), (unsigned long)mqttBackoff);
    mqttBackoff = min(mqttBackoff * 2, (uint32_t)MQTT_BACKOFF_MAX_MS);
  }
}

void netLoop() {
  if (WiFi.status() != WL_CONNECTED) {
    if (wasConnected) { wasConnected = false; Serial.println("WiFi потерян"); }
    if (millis() - lastWifiTry >= WIFI_RETRY_MS) {
      lastWifiTry = millis();
      WiFi.disconnect();
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }
    return;
  }

  if (!wasConnected) {
    wasConnected = true;
    mqttBackoff = MQTT_BACKOFF_MIN_MS;
    Serial.print("WiFi OK, IP: "); Serial.println(WiFi.localIP());
    if (cfg.otaEnabled && !otaStarted) {
      ArduinoOTA.setHostname(OTA_HOSTNAME);
      ArduinoOTA.setPassword(OTA_PASSWORD);
      ArduinoOTA.begin();
      otaStarted = true;
    }
  }

  if (!mqtt.connected()) {
    if (millis() - lastMqttTry >= mqttBackoff) { lastMqttTry = millis(); connectMqtt(); }
    return;
  }
  mqtt.loop();
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n=== Станция мониторинга, fw " FW_VERSION " ===");

#if ESP_ARDUINO_VERSION_MAJOR >= 3
  esp_task_wdt_config_t wdtCfg = {
    .timeout_ms = WDT_TIMEOUT_S * 1000, .idle_core_mask = 0, .trigger_panic = true
  };
  esp_task_wdt_init(&wdtCfg);
#else
  esp_task_wdt_init(WDT_TIMEOUT_S, true);
#endif
  esp_task_wdt_add(NULL);

  snprintf(bootId, sizeof(bootId), "%08x", (unsigned int)esp_random());

  cfg.load();
  applyFilterSize();

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  gpsSerial.begin(GPS_BAUD, SERIAL_8N1, PIN_GPS_RX, PIN_GPS_TX);

  initSensors(true);

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  mqtt.setServer(MQTT_SERVER, MQTT_PORT);
  mqtt.setBufferSize(MQTT_BUFFER_SIZE);
  mqtt.setKeepAlive(MQTT_KEEPALIVE_S);
  mqtt.setCallback(onMqttMessage);

  lastSample = millis() - cfg.publishIntervalMs;
  Serial.println("Инициализация завершена");
}

void loop() {
  esp_task_wdt_reset();

  if (rebootAt && millis() >= rebootAt) { delay(100); ESP.restart(); }

  if (otaStarted && cfg.otaEnabled) ArduinoOTA.handle();
  netLoop();

  uint32_t gpsStart = millis();
  while (gpsSerial.available() && millis() - gpsStart < 20) gps.encode(gpsSerial.read());

  if (millis() - lastGpsTimeCheck >= 5000) {
    lastGpsTimeCheck = millis();
    syncTimeFromGps();
  }

  if (millis() - lastSample >= cfg.publishIntervalMs) {
    lastSample = millis();
    String record = buildMeasurement();
    Serial.println(record);

    if (!publishMeasurement(record)) {
      if (cfg.sdBuffering && health.sd) {
        if (spool.append(record.c_str())) spooledTotal++;
        else logEvent("error", "spool_write", "Не удалось записать измерение на SD");
      } else {
        failTotal++;
      }
    }
  }

  drainSpool();
  flushLogQueue();

  if (millis() - lastStatus >= cfg.statusIntervalMs) publishStatus();

  if (millis() - lastSensorRetry >= SENSOR_RETRY_MS) {
    lastSensorRetry = millis();
    initSensors(false);
  }
}
