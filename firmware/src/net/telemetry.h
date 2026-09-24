#pragma once
#include <Arduino.h>

#include "../core/sample.h"
#include "../core/stability.h"
#include "../sensors/gps_sensor.h"

namespace Telemetry {
struct StatusInput {
  const char* name;
  const char* bootId;
  uint32_t rev;
  uint32_t publishIntervalMs;
  uint32_t statusIntervalMs;
  uint32_t spooledTotal;

  bool bmeOk;
  bool rtcOk;
  bool tdsOk;
  bool powerOk;
  bool sdOk;
  bool gpsOk;

  uint16_t spoolFiles;
  uint32_t spoolBytes;

  const StabilityMonitor* stability;
};

String buildMeasurement(const Sample& s, const GpsSensor::Fix& fix,
                        const StabilityVerdict& verdict, const char* bootId);

String buildStatus(const StatusInput& in);

}
