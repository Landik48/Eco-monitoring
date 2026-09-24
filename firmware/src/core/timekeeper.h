#pragma once
#include <stdint.h>

#include "limits.h"

enum TimeSource : uint8_t {
  TIME_SRC_NONE   = 0,
  TIME_SRC_RTC    = 1,
  TIME_SRC_SERVER = 2,
  TIME_SRC_GPS    = 3,
};

const char* timeSourceName(TimeSource src);

class TimeKeeper {
public:
  struct Tolerance {
    int32_t gps    = 2;
    int32_t server = 30;
    int32_t rtc    = 5;

    uint32_t staleAfterSec = 6 * 3600;
  };

  void setTolerance(const Tolerance& t) { _tol = t; }

  static bool sane(uint32_t epoch) {
    return epoch >= TIME_SANE_MIN && epoch <= TIME_SANE_MAX;
  }

  bool set(uint32_t epoch, TimeSource src, uint32_t nowMs, bool force = false);

  uint32_t now(uint32_t nowMs) const;

  bool        trusted() const { return _trusted; }
  TimeSource  source() const { return _source; }
  const char* sourceName() const { return timeSourceName(_source); }

  int32_t drift(uint32_t candidate, uint32_t nowMs) const;
  uint32_t secondsSinceSync(uint32_t nowMs) const;
  bool needsResync(uint32_t nowMs, uint32_t intervalSec) const;

  void markResyncRequested(uint32_t nowMs) {
    _lastRequestMs = nowMs;
    _requestPending = true;
    _everRequested = true;
  }
  bool resyncRequestPending() const { return _requestPending; }
  void clearResyncRequest() { _requestPending = false; }

  uint32_t secondsSinceRequest(uint32_t nowMs) const;

private:
  int32_t toleranceFor(TimeSource src) const;

  uint32_t   _base = 0;
  uint32_t   _baseMs = 0;
  bool       _trusted = false;
  TimeSource _source = TIME_SRC_NONE;

  uint32_t _lastSyncMs = 0;
  bool     _everSynced = false;
  uint32_t _lastRequestMs = 0;
  bool     _requestPending = false;
  bool     _everRequested = false;

  Tolerance _tol;
};
