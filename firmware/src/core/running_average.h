#pragma once
#include <stdint.h>

#include "limits.h"

class RunningAverage {
public:
  void  setSize(uint8_t size);
  void  reset();
  void  push(float value);

  bool  valid() const { return _count > 0; }
  float value() const;

  uint8_t count() const { return _count; }
  uint8_t size() const { return _size; }

  float last() const;

private:
  float   _buf[FILTER_MAX] = {0};
  uint8_t _size = 5;
  uint8_t _idx = 0;
  uint8_t _count = 0;
  bool    _hasLast = false;
  float   _last = 0;
};
