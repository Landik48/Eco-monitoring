#include <Arduino.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <esp_task_wdt.h>

#include "config.h"
#include "settings.h"

#include "src/core/sample.h"
#include "src/core/stability.h"
#include "src/core/timekeeper.h"
#include "src/diag/logger.h"
#include "src/net/command_router.h"
#include "src/net/mqtt_link.h"
#include "src/net/telemetry.h"
#include "src/net/time_sync.h"
#include "src/sensors/gps_sensor.h"
#include "src/sensors/rtc_clock.h"
#include "src/sensors/sd_storage.h"
#include "src/sensors/sensor_hub.h"

#ifndef ESP_ARDUINO_VERSION_MAJOR
#define ESP_ARDUINO_VERSION_MAJOR 2
#endif

Settings cfg;
TimeKeeper timeKeeper;
StabilityMonitor stability;

char bootId[9] = "00000000";
uint32_t seqNo = 0;
uint32_t spooledTotal = 0;

uint32_t lastSample = 0, lastStatus = 0, lastSensorRetry = 0, lastDrain = 0;
uint32_t lastSampleMs = 0;
uint32_t rebootAt = 0;
bool otaStarted = false;

static bool logPublisher(const char* json) {
  return MqttLink::publish(TOPIC_LOG, json, false);
}

static uint32_t logTimeSource() { return TimeSync::now(); }

void publishStatus() {
  if (!MqttLink::connected()) return;

  const SensorHub::Health& h = SensorHub::health();

  Telemetry::StatusInput in;
  in.name = cfg.name;
  in.bootId = bootId;
  in.rev = cfg.rev;
  in.publishIntervalMs = cfg.publishIntervalMs;
  in.statusIntervalMs = cfg.statusIntervalMs;
  in.spooledTotal = spooledTotal;
  in.bmeOk = h.bme;
  in.rtcOk = h.rtc;
  in.tdsOk = h.tds;
  in.powerOk = h.power;
  in.sdOk = h.sd;
  in.gpsOk = h.gps;
  in.stability = &stability;
  SdStorage::stats(in.spoolFiles, in.spoolBytes);

  const String payload = Telemetry::buildStatus(in);
  MqttLink::publish(TOPIC_STATUS, payload.c_str(), true);
  lastStatus = millis();
}

void applySettings(JsonObjectConst o) {
  const uint32_t incomingRev = o["rev"] | 0;
  if (incomingRev && incomingRev == cfg.rev) return;

  String err;
  if (!cfg.applyJson(o, err)) {
    Logger::error("settings_reject", err);
    return;
  }

  cfg.rev = incomingRev ? incomingRev : cfg.rev;
  cfg.save();

  SensorHub::applyFilterSize(cfg.filterSize);
  SensorHub::retryInit(cfg, false);

  StabilityLimits lim = stability.limits();
  lim.vbatMin = cfg.vbatMin;
  lim.vbatSagDelta = cfg.vbatSagDelta;
  lim.tempRatePerMin = cfg.tempRatePerMin;
  lim.presRatePerMin = cfg.presRatePerMin;
  lim.tdsRatePerMin = cfg.tdsRatePerMin;
  stability.setLimits(lim);

  publishStatus();
  Logger::info("settings_applied", "Конфигурация обновлена, rev=" + String(cfg.rev));
}

void onMqttMessage(const char* topic, const uint8_t* payload, unsigned int len) {
  if (!strcmp(topic, TOPIC_TIME)) {
    TimeSync::onServerTime(payload, len);
    return;
  }

  JsonDocument d;
  if (deserializeJson(d, payload, len) != DeserializationError::Ok) {
    Logger::warn("msg_parse", String("Невалидный JSON в топике ") + topic);
    return;
  }

  if (!strcmp(topic, TOPIC_CMD)) {
    CommandRouter::execute(d.as<JsonObjectConst>());
  } else if (!strcmp(topic, TOPIC_SETTINGS)) {
    applySettings(d.as<JsonObjectConst>());
  }
}

void onMqttConnected() {
  MqttLink::subscribe(TOPIC_CMD, 1);
  MqttLink::subscribe(TOPIC_SETTINGS, 1);
  MqttLink::subscribe(TOPIC_TIME, 1);

  publishStatus();
  Logger::flush();

  TimeSync::requestNow();
}

static void cmdPublishNow() { lastSample = millis() - cfg.publishIntervalMs; }
static void cmdClearSpool() { SdStorage::spool().clear(); }
static void cmdFactoryReset() { cfg.setDefaults(); cfg.save(); }
static void cmdScheduleReboot(uint32_t delayMs) { rebootAt = millis() + delayMs; }
static void cmdResetStability() { stability.clearStats(); }
static void cmdSyncTime() { TimeSync::requestNow(); }

static bool cmdSetTime(uint32_t epoch) { return TimeSync::applyServerEpoch(epoch, -1, true); }

void drainSpool() {
  if (!MqttLink::connected() || !SdStorage::ready()) return;
  if (millis() - lastDrain < SPOOL_DRAIN_PERIOD_MS) return;
  lastDrain = millis();

  Spool& spool = SdStorage::spool();
  if (!spool.pending()) return;

  const uint16_t n = spool.drainLines([](const char* line) {
    return MqttLink::publish(TOPIC_BACKLOG, line, false);
  }, SPOOL_DRAIN_BATCH);

  if (n) MqttLink::addSent(n);
}

void doSample() {
  const uint32_t nowMs = millis();
  uint32_t dtSec = lastSampleMs ? (nowMs - lastSampleMs) / 1000U : 0;
  if (dtSec == 0) dtSec = cfg.publishIntervalMs / 1000U;
  lastSampleMs = nowMs;

  Sample raw = SensorHub::readRaw(cfg);
  raw.ts = TimeSync::now();
  raw.upMs = nowMs;
  raw.seq = ++seqNo;

  const StabilityVerdict verdict = stability.evaluate(raw, dtSec);

  if (!verdict.suspect) {
    SensorHub::accept(raw);
  }

  Sample out = SensorHub::hasData() ? SensorHub::averaged() : raw;
  out.ts = raw.ts;
  out.upMs = raw.upMs;
  out.seq = raw.seq;

  const GpsSensor::Fix fix = GpsSensor::fix(cfg.gpsMinSats, 10000);
  String record = Telemetry::buildMeasurement(out, fix, verdict, bootId);

  if (verdict.suspect) {
    JsonDocument d;
    if (deserializeJson(d, record) == DeserializationError::Ok) {
      JsonObject r = d["rejected"].to<JsonObject>();
      if (Sample::has(raw.temp)) r["temp"] = raw.temp;
      if (Sample::has(raw.hum)) r["hum"] = raw.hum;
      if (Sample::has(raw.pres)) r["pres"] = raw.pres;
      if (Sample::has(raw.tds)) r["tds"] = raw.tds;
      if (Sample::has(raw.ec)) r["ec"] = raw.ec;
      if (Sample::has(raw.vbat)) r["vbat"] = raw.vbat;
      if (Sample::has(raw.ibat)) r["ibat"] = raw.ibat;
      record = "";
      serializeJson(d, record);
    }

    char why[160];
    describeSuspectFlags(verdict.flags, why, sizeof(why));
    Logger::warn("unstable", String("Замер помечен недостоверным: ") + why);
  }

  Serial.println(record);

  if (MqttLink::publish(TOPIC_TELEMETRY, record.c_str(), false)) {
    MqttLink::addSent(1);
  } else if (cfg.sdBuffering && SdStorage::ready()) {
    if (SdStorage::spool().append(record.c_str())) {
      spooledTotal++;
    } else {
      MqttLink::addFailed(1);
      Logger::error("spool_write", "Не удалось записать измерение на SD");
    }
  } else {
    MqttLink::addFailed(1);
  }
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
  Logger::begin(logPublisher, logTimeSource);
  SensorHub::begin(cfg);
  TimeSync::begin(&timeKeeper);
  TimeSync::primeFromRtc();

  StabilityLimits lim;
  lim.vbatMin = cfg.vbatMin;
  lim.vbatSagDelta = cfg.vbatSagDelta;
  lim.tempRatePerMin = cfg.tempRatePerMin;
  lim.presRatePerMin = cfg.presRatePerMin;
  lim.tdsRatePerMin = cfg.tdsRatePerMin;
  stability.setLimits(lim);

  CommandRouter::Handlers handlers;
  handlers.publishStatus = publishStatus;
  handlers.publishNow = cmdPublishNow;
  handlers.clearSpool = cmdClearSpool;
  handlers.factoryReset = cmdFactoryReset;
  handlers.scheduleReboot = cmdScheduleReboot;
  handlers.resetStability = cmdResetStability;
  handlers.syncTime = cmdSyncTime;
  handlers.setTime = cmdSetTime;
  CommandRouter::begin(handlers);
  MqttLink::begin(onMqttMessage, onMqttConnected);
  lastSample = millis() - cfg.publishIntervalMs;
  Serial.println("Инициализация завершена");
}

void loop() {
  esp_task_wdt_reset();

  if (rebootAt && millis() >= rebootAt) {
    delay(100);
    ESP.restart();
  }

  if (cfg.otaEnabled) {
    if (!otaStarted && MqttLink::wifiUp()) {
      ArduinoOTA.setHostname(OTA_HOSTNAME);
      ArduinoOTA.setPassword(OTA_PASSWORD);
      ArduinoOTA.begin();
      otaStarted = true;
    }
    if (otaStarted) ArduinoOTA.handle();
  }

  MqttLink::loop();

  GpsSensor::pump(20);
  TimeSync::pollGps(cfg.enableGps);
  TimeSync::maintain(MqttLink::connected());

  if (millis() - lastSample >= cfg.publishIntervalMs) {
    lastSample = millis();
    doSample();
  }

  drainSpool();
  Logger::flush();

  if (millis() - lastStatus >= cfg.statusIntervalMs) publishStatus();

  if (millis() - lastSensorRetry >= SENSOR_RETRY_MS) {
    lastSensorRetry = millis();
    SensorHub::retryInit(cfg, false);
  }
}
