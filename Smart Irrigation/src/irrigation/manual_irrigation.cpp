#include "manual_irrigation.h"
#include "../storage/preferences.h"
#include "../ui/menu_system.h"
#include "../hardware/hardware.h"
#include "../hardware/display.h"
#include "valve_control.h"

// Manual irrigation state
bool editingManualHour = true;
uint8_t manualHour = 6;  // Default to 6:00
uint8_t manualMin = 0;
bool manualScheduled = false;
bool manualRunning = false;
unsigned long manualStartTime = 0;

bool isEditingManualHour() {
  return editingManualHour;
}

void setEditingManualHour(bool editing) {
  editingManualHour = editing;
}

uint8_t getManualHour() {
  return manualHour;
}

void setManualHour(uint8_t hour) {
  if (hour <= 23) {
    manualHour = hour;
  }
}

uint8_t getManualMin() {
  return manualMin;
}

void setManualMin(uint8_t min) {
  if (min <= 59) {
    manualMin = min;
  }
}

bool isManualScheduled() {
  return manualScheduled;
}

void setManualScheduled(bool scheduled) {
  manualScheduled = scheduled;
  if (!scheduled) {
    manualRunning = false;
  }
}

void processManualIrrigation() {
  if (!manualScheduled) return;

  // Check if it's time to start manual irrigation
  if (nowMatchesManualTime() && !manualRunning) {
    manualRunning = true;
    manualStartTime = millis();

    // Activate valves based on saved configuration
    syncValvesWithSaved();
    syncPumpsWithSaved();

    Serial.println("[MANUAL] Started manual irrigation");
    displayMessage("TU DONG TUOI", "BAT DAU TUOI", "", "", 1000);
  }

  // Check if manual irrigation is running and needs to be stopped
  if (manualRunning) {
    unsigned long elapsedSec = (millis() - manualStartTime) / 1000;

    if (elapsedSec >= getSavedDelaySec()) {
      // Turn off all valves and pumps when time is up
      turnAllOff();
      manualRunning = false;
      manualScheduled = false;  // One-time run

      Serial.println("[MANUAL] Completed manual irrigation cycle");
      displayMessage("TU DONG TUOI", "DA HOAN THANH", "", "", 1000);
    }
  }
}