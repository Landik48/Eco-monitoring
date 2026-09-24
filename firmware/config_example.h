#pragma once

#define DEVICE_ID       "station-01"
#define FW_VERSION      "4.1.0"

#define WIFI_SSID       "имя_вашей_сети"
#define WIFI_PASSWORD   "пароль_сети"

#define MQTT_SERVER     "адрес_или_IP_сервера"
#define MQTT_PORT       1883
#define MQTT_USER       "station"
#define MQTT_PASSWORD   "пароль_из_MQTT_PASSWORD"

#define MQTT_KEEPALIVE_S   30
#define MQTT_BUFFER_SIZE   1024

#define TOPIC_PREFIX     "env/" DEVICE_ID
#define TOPIC_TELEMETRY  TOPIC_PREFIX "/telemetry"
#define TOPIC_BACKLOG    TOPIC_PREFIX "/backlog"
#define TOPIC_STATUS     TOPIC_PREFIX "/status"
#define TOPIC_LOG        TOPIC_PREFIX "/log"
#define TOPIC_ACK        TOPIC_PREFIX "/ack"
#define TOPIC_CMD        TOPIC_PREFIX "/cmd"
#define TOPIC_SETTINGS   TOPIC_PREFIX "/settings"

#define TOPIC_TIME       TOPIC_PREFIX "/time"
#define TOPIC_TIMEREQ    TOPIC_PREFIX "/timereq"

#define TIME_RESYNC_INTERVAL_SEC  3600UL

#define OTA_HOSTNAME    DEVICE_ID
#define OTA_PASSWORD    "пароль_для_обновления_по_воздуху"

#define PIN_I2C_SDA   21
#define PIN_I2C_SCL   22
#define PIN_GPS_RX    16
#define PIN_GPS_TX    17
#define GPS_BAUD      9600
#define PIN_SD_CS     5

#define ADDR_BME      0x76
#define ADDR_BME_ALT  0x77
#define ADDR_RTC      0x68
#define ADDR_TDS      0x09
#define ADDR_PWR      0x35
#define PWR_SHUNT     0.0128f

#define USE_BATTERY_SHIELD 1

#define USE_TDS_SENSOR 1

#define TDS_TEMP_COMPENSATION 1

#define WDT_TIMEOUT_S        60
#define WIFI_RETRY_MS        10000UL
#define MQTT_BACKOFF_MIN_MS  2000UL
#define MQTT_BACKOFF_MAX_MS  60000UL
#define SENSOR_RETRY_MS      30000UL

#define SPOOL_DIR              "/spool"
#define SPOOL_STATE_FILE       SPOOL_DIR "/state.txt"
#define SPOOL_MAX_FILE_BYTES   (256UL * 1024UL)
#define SPOOL_MIN_FREE_BYTES   (4UL * 1024UL * 1024UL)
#define SPOOL_DRAIN_BATCH      20
#define SPOOL_DRAIN_PERIOD_MS  200UL

#define LOG_QUEUE_SIZE         8
