#pragma once
#include <stdint.h>

enum AppState
{
  STATE_MAIN,
  STATE_CONFIG,
  STATE_MANUAL,
  STATE_AUTO
};

namespace Display
{
  extern AppState currentState;
  extern bool blinkState;
  void drawMainMenu(uint8_t mainIndex, bool autoOn, uint8_t startValve);
  void drawConfigMenu(uint8_t configMode, uint8_t page, uint8_t index, bool powerCheckEnabled, uint16_t delaySec);
  void drawManualMenu(uint8_t hour, uint8_t minute, bool editingHour);
  void drawAutoMenu(uint8_t page, uint8_t index, bool inSelectValve, bool inTimeSetup, uint8_t selectField, bool autoEnabled, uint8_t startValve);
  void updateBlink();
  void drawManualRunning(uint8_t activeValve, uint16_t remainingSecs, uint8_t nextValve, uint16_t delayRemaining);
}
