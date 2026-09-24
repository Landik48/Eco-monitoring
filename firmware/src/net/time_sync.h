#pragma once
#include <Arduino.h>

#include "../core/timekeeper.h"

namespace TimeSync {
void begin(TimeKeeper* keeper);
void primeFromRtc();
void pollGps(bool gpsEnabled);
void maintain(bool online);
void onServerTime(const uint8_t* payload, unsigned int len);
void requestNow();
bool applyServerEpoch(uint32_t epoch, int32_t rttMs, bool force = false);
uint32_t now();
bool trusted();
const char* sourceName();
uint32_t secondsSinceSync();

}
