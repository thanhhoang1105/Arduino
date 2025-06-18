#pragma once
#include <stdint.h>

namespace Config
{
  void load();
  void save();

  // Valve getters/setters
  bool getValveState(uint8_t index);
  void setValveState(uint8_t index, bool state);

  // Pump getters/setters
  bool getPumpState(uint8_t index);
  void setPumpState(uint8_t index, bool state);

  // Delay getter/setter
  uint16_t getDelaySec();
  void setDelaySec(uint16_t seconds);

  // Power check getter/setter
  bool getPowerCheckEnabled();
  void setPowerCheckEnabled(bool enabled);

  // Auto mode getters/setters
  bool getAutoEnabled();
  void setAutoEnabled(bool enabled);
  uint8_t getAutoStartIndex();
  void setAutoStartIndex(uint8_t index);

  // Auto time range getters/setters
  uint8_t getAutoFromHour();
  void setAutoFromHour(uint8_t hour);
  uint8_t getAutoFromMinute();
  void setAutoFromMinute(uint8_t min);
  uint8_t getAutoToHour();
  void setAutoToHour(uint8_t hour);
  uint8_t getAutoToMinute();
  void setAutoToMinute(uint8_t min);

  // Duration getters/setters
  uint8_t getDurationHour();
  void setDurationHour(uint8_t hour);
  uint8_t getDurationMinute();
  void setDurationMinute(uint8_t min);

  // Rest time getters/setters
  uint8_t getRestHour();
  void setRestHour(uint8_t hour);
  uint8_t getRestMinute();
  void setRestMinute(uint8_t min);
}
