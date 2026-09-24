#include "running_average.h"

#include <math.h>

void RunningAverage::setSize(uint8_t size) {
  if (size < 1) size = 1;
  if (size > FILTER_MAX) size = FILTER_MAX;
  if (size != _size) {
    _size = size;
    reset();
  }
}

void RunningAverage::reset() {
  _count = 0;
  _idx = 0;
  _hasLast = false;
  _last = 0;
}

void RunningAverage::push(float value) {
  if (isnan(value) || isinf(value)) return;
  _buf[_idx] = value;
  _idx = (uint8_t)((_idx + 1) % _size);
  if (_count < _size) _count++;
  _last = value;
  _hasLast = true;
}

float RunningAverage::value() const {
  if (_count == 0) return NAN;
  float sum = 0;
  for (uint8_t i = 0; i < _count; i++) sum += _buf[i];
  return sum / (float)_count;
}

float RunningAverage::last() const {
  return _hasLast ? _last : NAN;
}
