#pragma once
#include <Preferences.h>
#include <LiquidCrystal_PCF8574.h>

namespace Hardware {
  extern LiquidCrystal_PCF8574 lcd;
  void begin();
  float readCurrent();
  void setValve(uint8_t idx, bool on);
  void setPump(uint8_t idx, bool on);
  void turnAllOff();
}
