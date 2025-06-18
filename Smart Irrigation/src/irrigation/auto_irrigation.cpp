#include "auto_irrigation.h"
#include "../storage/preferences.h"
#include "../ui/menu_system.h"
#include "../hardware/hardware.h"
#include "../hardware/display.h"
#include "valve_control.h"

// Auto mode operation
bool autoRunning = false;
unsigned long autoLastActionMillis = 0;
uint8_t autoCurrentValve = 0;
bool autoStep = false;  // false = valve on, true = rest period

bool isAutoRunning() {
  return autoRunning;
}

void setAutoRunning(bool running) {
  autoRunning = running;
}

void processAutoIrrigation() {
  if (isAutoEnabled()) {
    TimeHM autoFrom = getAutoFromTime();
    TimeHM autoTo = getAutoToTime();

    // Check if we're in the time range
    if (nowInTimeRange(autoFrom, autoTo)) {
      // Start auto mode if not running
      if (!autoRunning) {
        autoRunning = true;
        autoCurrentValve = getAutoStartIndex() - 1;  // Convert 1-based to 0-based index
        autoStep = false;
        autoLastActionMillis = millis();

        // Turn on first valve
        turnAllOff();
        setValveState(autoCurrentValve, true);
        syncPumpsWithSaved();

        Serial.println("[AUTO] Started auto irrigation");
        displayMessage("CHE DO TU DONG", "BAT DAU TUOI", "", "", 1000);
      }

      // Check for time to switch valves or start/end rest period
      unsigned long elapsedMillis = millis() - autoLastActionMillis;
      TimeHM timeToCheck = autoStep ? getAutoRestTime() : getAutoDurationTime();
      unsigned long timeToCheckMillis = (timeToCheck.hour * 3600UL + timeToCheck.minute * 60UL) * 1000UL;

      if (elapsedMillis >= timeToCheckMillis) {
        autoLastActionMillis = millis();

        if (autoStep) {
          // End of rest period, move to next valve
          autoCurrentValve = (autoCurrentValve + 1) % 10;

          // If we've gone full circle, start from configured valve again
          if (autoCurrentValve == getAutoStartIndex() - 1) {
            // Full cycle completed
            Serial.println("[AUTO] Completed full cycle, starting next round");
          }

          autoStep = false;

          // Turn on next valve
          turnAllOff();
          setValveState(autoCurrentValve, true);
          syncPumpsWithSaved();

          Serial.print("[AUTO] Activating valve ");
          Serial.println(autoCurrentValve + 1);
        } else {
          // End of valve operation, start rest period
          autoStep = true;
          turnAllOff();

          Serial.println("[AUTO] Starting rest period");
        }
      }
    } else if (autoRunning) {
      // Outside time range, stop auto mode
      autoRunning = false;
      turnAllOff();

      Serial.println("[AUTO] Stopped auto irrigation (outside time range)");
      displayMessage("CHE DO TU DONG", "KET THUC TUOI", "", "", 1000);
    }
  } else if (autoRunning) {
    // Auto mode disabled, stop if running
    autoRunning = false;
    turnAllOff();

    Serial.println("[AUTO] Stopped auto irrigation (disabled)");
  }
}