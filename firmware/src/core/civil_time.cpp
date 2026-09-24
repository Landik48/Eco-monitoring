#include "civil_time.h"

uint32_t civilToUnix(uint16_t year, uint8_t month, uint8_t day,
                     uint8_t hour, uint8_t minute, uint8_t second) {
  int32_t y = (int32_t)year;
  y -= (month <= 2);

  const int32_t era = (y >= 0 ? y : y - 399) / 400;
  const uint32_t yoe = (uint32_t)(y - era * 400);
  const uint32_t doy =
      (uint32_t)((153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1);
  const uint32_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  const int32_t days = era * 146097 + (int32_t)doe - 719468;

  return (uint32_t)days * 86400UL + hour * 3600UL + minute * 60UL + second;
}
