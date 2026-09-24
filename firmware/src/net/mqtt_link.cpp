#include "mqtt_link.h"

#include <PubSubClient.h>
#include <WiFi.h>

#include "../../config.h"
#include "../diag/logger.h"

namespace {
WiFiClient espClient;
PubSubClient mqtt(espClient);

MqttLink::ConnectHandler connectHandler = nullptr;
MqttLink::MessageHandler messageHandler = nullptr;

void trampoline(char* topic, byte* payload, unsigned int len) {
  if (messageHandler) messageHandler(topic, (const uint8_t*)payload, len);
}

uint32_t lastWifiTry = 0;
uint32_t lastMqttTry = 0;
uint32_t backoff = MQTT_BACKOFF_MIN_MS;

uint32_t sent = 0;
uint32_t failed = 0;
bool wasConnected = false;
const char* WILL_PAYLOAD = "{\"device_id\":\"" DEVICE_ID "\",\"online\":false}";

void tryConnect() {
  if (mqtt.connect(DEVICE_ID, MQTT_USER, MQTT_PASSWORD,
                   TOPIC_STATUS, 0, true, WILL_PAYLOAD)) {
    backoff = MQTT_BACKOFF_MIN_MS;
    if (connectHandler) connectHandler();
    Logger::info("mqtt_connected", "Соединение с брокером установлено");
    return;
  }

  failed++;
  Serial.printf("MQTT ошибка, state=%d, повтор через %lu мс\n",
                mqtt.state(), (unsigned long)backoff);
  backoff = backoff * 2;
  if (backoff > MQTT_BACKOFF_MAX_MS) backoff = MQTT_BACKOFF_MAX_MS;
}
}

namespace MqttLink {
void begin(MessageHandler onMessage, ConnectHandler onConnect) {
  connectHandler = onConnect;
  messageHandler = onMessage;

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  mqtt.setServer(MQTT_SERVER, MQTT_PORT);
  mqtt.setBufferSize(MQTT_BUFFER_SIZE);
  mqtt.setKeepAlive(MQTT_KEEPALIVE_S);
  mqtt.setCallback(trampoline);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    if (wasConnected) {
      wasConnected = false;
      Serial.println("WiFi потерян");
    }
    if (millis() - lastWifiTry >= WIFI_RETRY_MS) {
      lastWifiTry = millis();
      WiFi.disconnect();
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }
    return;
  }

  if (!wasConnected) {
    wasConnected = true;
    backoff = MQTT_BACKOFF_MIN_MS;
    Serial.print("WiFi OK, IP: ");
    Serial.println(WiFi.localIP());
  }

  if (!mqtt.connected()) {
    if (millis() - lastMqttTry >= backoff) {
      lastMqttTry = millis();
      tryConnect();
    }
    return;
  }

  mqtt.loop();
}

bool wifiUp() { return WiFi.status() == WL_CONNECTED; }

bool connected() { return mqtt.connected(); }

void subscribe(const char* topic, uint8_t qos) { mqtt.subscribe(topic, qos); }

bool publish(const char* topic, const char* payload, bool retain) {
  if (!mqtt.connected()) return false;
  return mqtt.publish(topic, payload, retain);
}

int32_t rssi() { return wifiUp() ? WiFi.RSSI() : 0; }

String ip() { return WiFi.localIP().toString(); }

uint32_t sentCount() { return sent; }
uint32_t failedCount() { return failed; }
void addSent(uint32_t n) { sent += n; }
void addFailed(uint32_t n) { failed += n; }

}
