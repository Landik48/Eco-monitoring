#include "timekeeper.h"

const char* timeSourceName(TimeSource src) {
  switch (src) {
    case TIME_SRC_GPS:    return "gps";
    case TIME_SRC_SERVER: return "server";
    case TIME_SRC_RTC:    return "rtc";
    default:              return "none";
  }
}

int32_t TimeKeeper::toleranceFor(TimeSource src) const {
  switch (src) {
    case TIME_SRC_GPS:    return _tol.gps;
    case TIME_SRC_SERVER: return _tol.server;
    case TIME_SRC_RTC:    return _tol.rtc;
    default:              return 0;
  }
}

uint32_t TimeKeeper::now(uint32_t nowMs) const {
  if (!_trusted) return 0;

  return _base + (uint32_t)((nowMs - _baseMs) / 1000U);
}

int32_t TimeKeeper::drift(uint32_t candidate, uint32_t nowMs) const {
  uint32_t local = now(nowMs);
  if (local == 0) return INT32_MAX;
  return (int32_t)((int64_t)candidate - (int64_t)local);
}

bool TimeKeeper::set(uint32_t epoch, TimeSource src, uint32_t nowMs, bool force) {
  if (!sane(epoch)) return false;
  if (src == TIME_SRC_NONE) return false;

  if (_trusted && !force) {
    if (src < _source) {
      if (secondsSinceSync(nowMs) < _tol.staleAfterSec) return false;

      int32_t d = drift(epoch, nowMs);
      if (d == INT32_MAX) return false;
      int32_t big = toleranceFor(_source) * 20;
      if (big < 600) big = 600;
      if (d > -big && d < big) return false;
    } else {
      int32_t d = drift(epoch, nowMs);
      int32_t tol = toleranceFor(src);
      if (d != INT32_MAX && d > -tol && d < tol) {
        _lastSyncMs = nowMs;
        _everSynced = true;
        _requestPending = false;
        if (src > _source) _source = src;
        return false;
      }
    }
  }

  _base = epoch;
  _baseMs = nowMs;
  _source = src;
  _trusted = true;
  _lastSyncMs = nowMs;
  _everSynced = true;
  _requestPending = false;
  return true;
}

uint32_t TimeKeeper::secondsSinceSync(uint32_t nowMs) const {
  if (!_everSynced) return UINT32_MAX;
  return (uint32_t)((nowMs - _lastSyncMs) / 1000U);
}

uint32_t TimeKeeper::secondsSinceRequest(uint32_t nowMs) const {
  if (!_everRequested && !_requestPending) return UINT32_MAX;
  return (uint32_t)((nowMs - _lastRequestMs) / 1000U);
}

bool TimeKeeper::needsResync(uint32_t nowMs, uint32_t intervalSec) const {
  if (!_trusted) return true;
  if (_source == TIME_SRC_GPS) return false;
  return secondsSinceSync(nowMs) >= intervalSec;
}
