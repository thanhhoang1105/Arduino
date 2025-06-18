#include "menu_system.h"
#include "../config.h"
#include "main_menu.h"
#include "config_menu.h"
#include "manual_menu.h"
#include "auto_menu.h"
#include "../hardware/ir_remote.h"
#include <time.h>

void handleIR(uint32_t code) {
  switch (currentAppState) {
    case STATE_MAIN:
      handleMainIR(code);
      break;
    case STATE_CONFIG:
      handleConfigIR(code);
      break;
    case STATE_MANUAL:
      handleManualIR(code);
      break;
    case STATE_AUTO:
      handleAutoIR(code);
      break;
    default:
      break;
  }
}

TimeHM getCurrentTimeHM() {
  TimeHM currentTime = {0, 0};
  struct tm timeinfo;

  if (getLocalTime(&timeinfo)) {
    currentTime.hour = timeinfo.tm_hour;
    currentTime.minute = timeinfo.tm_min;
  }

  return currentTime;
}

bool nowInTimeRange(const TimeHM &from, const TimeHM &to) {
  TimeHM now = getCurrentTimeHM();

  // Convert all to minutes for easier comparison
  int nowMinutes = now.hour * 60 + now.minute;
  int fromMinutes = from.hour * 60 + from.minute;
  int toMinutes = to.hour * 60 + to.minute;

  if (fromMinutes <= toMinutes) {
    // Simple case: within the same day
    return (nowMinutes >= fromMinutes && nowMinutes < toMinutes);
  } else {
    // Wrap around midnight case
    return (nowMinutes >= fromMinutes || nowMinutes < toMinutes);
  }
}

bool nowMatchesManualTime() {
  TimeHM now = getCurrentTimeHM();
  return (now.hour == getManualHour() && now.minute == getManualMin());
}

bool anySavedValveOpen() {
  for (uint8_t i = 0; i < 10; i++) {
    if (getSavedValveState(i)) return true;
  }
  return false;
}