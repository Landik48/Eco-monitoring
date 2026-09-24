#pragma once
#include <Arduino.h>

namespace MqttLink {
typedef void (*MessageHandler)(const char* topic, const uint8_t* payload, unsigned int len);
typedef void (*ConnectHandler)();
void begin(MessageHandler onMessage, ConnectHandler onConnect);
void loop();
bool wifiUp();
bool connected();

void subscribe(const char* topic, uint8_t qos = 1);
bool publish(const char* topic, const char* payload, bool retain = false);

int32_t rssi();
String  ip();

uint32_t sentCount();
uint32_t failedCount();
void     addSent(uint32_t n);
void     addFailed(uint32_t n);

}
