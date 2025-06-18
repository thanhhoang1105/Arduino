#pragma once

// Định nghĩa các kiểu dữ liệu
enum AppState {
  STATE_MAIN,
  STATE_CONFIG,
  STATE_MANUAL,
  STATE_AUTO
};

enum ConfigMode {
  MODE_ONOFF,
  MODE_DELAY,
  MODE_POWERCHECK
};

struct TimeHM {
  uint8_t hour;
  uint8_t minute;
};

// Trạng thái toàn cục
extern AppState currentAppState;
extern uint8_t mainMenuIndex;
extern ConfigMode currentConfigMode;
extern uint8_t configMenuPage;
extern uint8_t configMenuIndex;
extern uint8_t autoMenuIndex;
extern uint8_t autoMenuPage;
extern bool inSelectValve;
extern bool inTimeSetup;
extern uint8_t autoSelectField;