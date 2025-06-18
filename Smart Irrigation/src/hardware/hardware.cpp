#include "hardware.h"
#include "../config.h"
#include "display.h"
#include "sensors.h"
#include "../storage/preferences.h"
#include "../networking/blynk_control.h"

void initializeHardware() {
  // Initialize relay pins (Active LOW: LOW = ON, HIGH = OFF)
  for (uint8_t i = 0; i < 10; i++) {
    pinMode(relayValvePins[i], OUTPUT);
    digitalWrite(relayValvePins[i], HIGH);  // Default OFF
  }
  for (uint8_t j = 0; j < 2; j++) {
    pinMode(relayPumpPins[j], OUTPUT);
    digitalWrite(relayPumpPins[j], HIGH);  // Default OFF
  }

  initializeDisplay();
}

void updateOutputsFromWorking() {
  // Valves (Active LOW: LOW = ON, HIGH = OFF)
  for (uint8_t i = 0; i < 10; i++) {
    digitalWrite(relayValvePins[i], getWorkingValveState(i) ? LOW : HIGH);
    updateBlynkValve(i, getWorkingValveState(i));
  }

  // Check if any valve is open before allowing pump activation
  bool anyValveOpen = isAnyValveOpen();

  // Pumps (Active LOW: LOW = ON, HIGH = OFF)
  for (uint8_t j = 0; j < 2; j++) {
    // Prevent pumps from running if no valves are open
    if (!anyValveOpen) {
      setWorkingPumpState(j, false);
    }
    digitalWrite(relayPumpPins[j], getWorkingPumpState(j) ? LOW : HIGH);
    updateBlynkPump(j, getWorkingPumpState(j));
  }
}

bool isAnyValveOpen() {
  for (uint8_t i = 0; i < 10; i++) {
    if (getWorkingValveState(i)) return true;
  }
  return false;
}

void turnAllOff() {
  // Turn off all valves and pumps
  for (uint8_t i = 0; i < 10; i++) {
    setWorkingValveState(i, false);
  }
  for (uint8_t j = 0; j < 2; j++) {
    setWorkingPumpState(j, false);
  }
  updateOutputsFromWorking();
}