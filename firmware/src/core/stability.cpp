#include "stability.h"

#include <math.h>
#include <string.h>

namespace {
float absf(float v) { return v < 0 ? -v : v; }

const uint16_t AIR_FLAGS   = SUSPECT_RATE_TEMP | SUSPECT_RATE_HUM | SUSPECT_RATE_PRES;
const uint16_t WATER_FLAGS = SUSPECT_RATE_TDS | SUSPECT_RATE_EC;
const uint16_t RATE_FLAGS  = AIR_FLAGS | WATER_FLAGS;
const uint16_t POWER_FLAGS = SUSPECT_POWER_SAG | SUSPECT_POWER_SPIKE;

}

void StabilityMonitor::reset() {
  _temp.reset();
  _hum.reset();
  _pres.reset();
  _tds.reset();
  _ec.reset();
  _hasVbat = false;
  _prevVbat = 0;
}

void StabilityMonitor::clearStats() {
  _score = 0;
  _sagEvents = 0;
  _suspectCount = 0;
  _vbatMinSet = false;
  _vbatMin = 0;
  _lastSagTs = 0;
}

const char* StabilityMonitor::state() const {
  if (_score >= 45.0f) return "unstable";
  if (_score >= 15.0f) return "watch";
  return "ok";
}

float StabilityMonitor::allowanceFor(float ratePerMin, float floorValue,
                                     uint32_t dtSec) const {
  if (dtSec == 0) dtSec = 1;
  float allowed = ratePerMin * ((float)dtSec / 60.0f);
  return allowed < floorValue ? floorValue : allowed;
}

bool StabilityMonitor::track(Channel& ch, float value, float allowed,
                             bool allowConfirm, bool& confirmed) {
  if (!ch.hasRef) {
    ch.ref = value;
    ch.hasRef = true;
    ch.hasCandidate = false;
    return false;
  }

  if (absf(value - ch.ref) <= allowed) {
    ch.ref = value;
    ch.hasCandidate = false;
    return false;
  }

  if (ch.hasCandidate && absf(value - ch.candidate) <= allowed) {
    if (allowConfirm) {
      ch.ref = value;
      ch.hasCandidate = false;
      confirmed = true;
      return false;
    }

    ch.candidate = value;
    return true;
  }

  ch.candidate = value;
  ch.hasCandidate = true;
  return true;
}

StabilityVerdict StabilityMonitor::evaluate(const Sample& s, uint32_t dtSec) {
  StabilityVerdict v;

  if (dtSec > _lim.staleAfterSec) {
    reset();
  }

  uint16_t flags = SUSPECT_NONE;
  bool confirmed = false;

  if (Sample::has(s.vbat)) {
    if (!_vbatMinSet || s.vbat < _vbatMin) {
      _vbatMin = s.vbat;
      _vbatMinSet = true;
    }
    if (s.vbat < _lim.vbatMin) flags |= SUSPECT_POWER_SAG;
    if (_hasVbat && (_prevVbat - s.vbat) >= _lim.vbatSagDelta) {
      flags |= SUSPECT_POWER_SAG;
    }
    _prevVbat = s.vbat;
    _hasVbat = true;
  }

  if (Sample::has(s.ibat) && absf(s.ibat) >= _lim.ibatSpike) {
    flags |= SUSPECT_POWER_SPIKE;
  }

  if (s.sensorFault) flags |= SUSPECT_SENSOR_FAULT;

  const bool powerHealthy = (flags & (SUSPECT_POWER_SAG | SUSPECT_POWER_SPIKE)) == 0 &&
                            !s.sensorFault;

  if (Sample::has(s.temp) &&
      track(_temp, s.temp, allowanceFor(_lim.tempRatePerMin, _lim.tempFloor, dtSec),
            powerHealthy, confirmed)) {
    flags |= SUSPECT_RATE_TEMP;
  }
  if (Sample::has(s.hum) &&
      track(_hum, s.hum, allowanceFor(_lim.humRatePerMin, _lim.humFloor, dtSec),
            powerHealthy, confirmed)) {
    flags |= SUSPECT_RATE_HUM;
  }
  if (Sample::has(s.pres) &&
      track(_pres, s.pres, allowanceFor(_lim.presRatePerMin, _lim.presFloor, dtSec),
            powerHealthy, confirmed)) {
    flags |= SUSPECT_RATE_PRES;
  }
  if (Sample::has(s.tds) &&
      track(_tds, s.tds, allowanceFor(_lim.tdsRatePerMin, _lim.tdsFloor, dtSec),
            powerHealthy, confirmed)) {
    flags |= SUSPECT_RATE_TDS;
  }
  if (Sample::has(s.ec) &&
      track(_ec, s.ec, allowanceFor(_lim.ecRatePerMin, _lim.ecFloor, dtSec),
            powerHealthy, confirmed)) {
    flags |= SUSPECT_RATE_EC;
  }

  if ((flags & AIR_FLAGS) && (flags & WATER_FLAGS)) {
    flags |= SUSPECT_MULTI;
  }

  if (confirmed) {
    flags |= SUSPECT_CONFIRMED;
    v.confirmsPrevious = true;
  }

  const bool hasRate  = (flags & RATE_FLAGS) != 0;
  const bool hasPower = (flags & POWER_FLAGS) != 0;

  v.suspect = (hasPower && hasRate) ||
              (flags & SUSPECT_MULTI) != 0 ||
              (flags & SUSPECT_SENSOR_FAULT) != 0;

  float weight = 0;
  if (flags & SUSPECT_POWER_SAG)    { weight += 18; _sagEvents++; _lastSagTs = s.ts; }
  if (flags & SUSPECT_POWER_SPIKE)  weight += 10;
  if (flags & SUSPECT_SENSOR_FAULT) weight += 14;
  if (flags & SUSPECT_MULTI)        weight += 22;
  if (hasRate && hasPower)          weight += 8;

  _score = _score * 0.88f + weight;
  if (_score > 100.0f) _score = 100.0f;
  if (_score < 0.05f) _score = 0;

  if (v.suspect) _suspectCount++;

  v.flags = flags;
  v.score = score();
  v.state = state();
  return v;
}

void describeSuspectFlags(uint16_t flags, char* buf, uint32_t bufLen) {
  if (!buf || bufLen == 0) return;
  buf[0] = '\0';
  if (flags == SUSPECT_NONE) {
    strncpy(buf, "норма", bufLen - 1);
    buf[bufLen - 1] = '\0';
    return;
  }

  struct Entry { uint16_t bit; const char* text; };
  static const Entry table[] = {
    {SUSPECT_POWER_SAG,    "просадка питания"},
    {SUSPECT_POWER_SPIKE,  "бросок тока"},
    {SUSPECT_RATE_TEMP,    "скачок температуры"},
    {SUSPECT_RATE_HUM,     "скачок влажности"},
    {SUSPECT_RATE_PRES,    "скачок давления"},
    {SUSPECT_RATE_TDS,     "скачок TDS"},
    {SUSPECT_RATE_EC,      "скачок EC"},
    {SUSPECT_SENSOR_FAULT, "отказ датчика"},
    {SUSPECT_MULTI,        "несвязанные каналы разом"},
    {SUSPECT_CONFIRMED,    "изменение подтверждено"},
  };

  uint32_t used = 0;
  for (const Entry& e : table) {
    if (!(flags & e.bit)) continue;
    uint32_t need = strlen(e.text) + (used ? 2 : 0);
    if (used + need >= bufLen) break;
    if (used) {
      buf[used++] = ',';
      buf[used++] = ' ';
    }
    strcpy(buf + used, e.text);
    used += strlen(e.text);
  }
  buf[used] = '\0';
}
