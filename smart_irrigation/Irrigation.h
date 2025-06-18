#pragma once
#include <stdint.h>

namespace Irrigation
{
  void update(); // gọi trong loop()
  void runManual();
  void runAuto();
  bool nowMatchesManual();
  bool nowInRange();
  void scheduleManual(uint8_t hour, uint8_t minute);
}
