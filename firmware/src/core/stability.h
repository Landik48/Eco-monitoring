#pragma once
#include <stdint.h>

#include "sample.h"

enum SuspectFlag : uint16_t {
  SUSPECT_NONE          = 0,
  SUSPECT_POWER_SAG     = 1 << 0,
  SUSPECT_POWER_SPIKE   = 1 << 1,
  SUSPECT_RATE_TEMP     = 1 << 2,
  SUSPECT_RATE_HUM      = 1 << 3,
  SUSPECT_RATE_PRES     = 1 << 4,
  SUSPECT_RATE_TDS      = 1 << 5,
  SUSPECT_RATE_EC       = 1 << 6,
  SUSPECT_SENSOR_FAULT  = 1 << 7,
  SUSPECT_MULTI         = 1 << 8,
  SUSPECT_CONFIRMED     = 1 << 9,
};

struct StabilityLimits {
  float vbatMin        = 3.40f;
  float vbatSagDelta   = 0.25f;
  float ibatSpike      = 1.50f;

  float tempRatePerMin = 6.0f;
  float humRatePerMin  = 30.0f;
  float presRatePerMin = 2.0f;
  float tdsRatePerMin  = 400.0f;
  float ecRatePerMin   = 800.0f;

  float tempFloor = 0.5f;
  float humFloor  = 2.0f;
  float presFloor = 0.3f;
  float tdsFloor  = 25.0f;
  float ecFloor   = 50.0f;

  uint32_t staleAfterSec = 900;
};

struct StabilityVerdict {
  uint16_t flags = SUSPECT_NONE;
  bool suspect = false;
  bool confirmsPrevious = false;
  uint8_t score = 0;
  const char* state = "ok";
};

class StabilityMonitor {
public:
  void setLimits(const StabilityLimits& limits) { _lim = limits; }
  const StabilityLimits& limits() const { return _lim; }

  void reset();
  StabilityVerdict evaluate(const Sample& s, uint32_t dtSec);

  uint8_t     score() const { return (uint8_t)(_score + 0.5f); }
  const char* state() const;
  uint32_t    sagEvents() const { return _sagEvents; }
  uint32_t    suspectCount() const { return _suspectCount; }
  float       vbatMinSeen() const { return _vbatMin; }
  uint32_t    lastSagTs() const { return _lastSagTs; }

  void clearStats();

private:
  struct Channel {
    bool  hasRef = false;
    float ref = 0;
    bool  hasCandidate = false;
    float candidate = 0;

    void reset() { hasRef = false; hasCandidate = false; }
  };

  bool track(Channel& ch, float value, float allowed, bool allowConfirm, bool& confirmed);
  float allowanceFor(float ratePerMin, float floorValue, uint32_t dtSec) const;

  StabilityLimits _lim;
  Channel _temp, _hum, _pres, _tds, _ec;

  bool  _hasVbat = false;
  float _prevVbat = 0;

  float    _score = 0;
  uint32_t _sagEvents = 0;
  uint32_t _suspectCount = 0;
  float    _vbatMin = 0;
  bool     _vbatMinSet = false;
  uint32_t _lastSagTs = 0;
};

void describeSuspectFlags(uint16_t flags, char* buf, uint32_t bufLen);
