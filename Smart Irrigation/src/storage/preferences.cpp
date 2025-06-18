#include "preferences.h"
#include <Preferences.h>

Preferences prefs;

// Relay states (saved in Preferences, working is for current edits)
bool savedValveState[10];
bool workingValveState[10];
bool savedPumpState[2];
bool workingPumpState[2];

// Delay (seconds) for manual irrigation cycle duration and auto mode valve step
uint16_t savedDelaySec;
uint16_t workingDelaySec;

// Power Check (ACS712)
bool savedPowerCheckEnabled;
bool workingPowerCheckEnabled;

// Automatic mode settings
bool savedAutoEnabled;
bool workingAutoEnabled;

uint8_t savedAutoStartIndex;  // 1-10 for display, internally 0-9
uint8_t workingAutoStartIndex;

TimeHM savedAutoFrom, workingAutoFrom;
TimeHM savedAutoTo, workingAutoTo;
TimeHM savedDuration, workingDuration;
TimeHM savedRest, workingRest;

void initializePreferences() {
  prefs.begin("cfg", false); // Namespace for preferences
}

void loadConfigFromPrefs() {
  // Valves
  for (uint8_t i = 0; i < 10; i++) {
    char key[3];
    sprintf(key, "v%d", i);
    savedValveState[i] = prefs.getBool(key, false);
    workingValveState[i] = savedValveState[i];
  }

  // Pumps
  savedPumpState[0] = prefs.getBool("p0", false);
  savedPumpState[1] = prefs.getBool("p1", false);
  workingPumpState[0] = savedPumpState[0];
  workingPumpState[1] = savedPumpState[1];

  // Delay
  savedDelaySec = prefs.getUInt("delay", 10);  // Default 10s
  workingDelaySec = savedDelaySec;

  // Power Check
  savedPowerCheckEnabled = prefs.getBool("power", false);
  workingPowerCheckEnabled = savedPowerCheckEnabled;

  // Auto mode
  savedAutoEnabled = prefs.getBool("ae", false);
  workingAutoEnabled = savedAutoEnabled;

  savedAutoStartIndex = prefs.getUInt("as", 1);  // 1-10 for display
  workingAutoStartIndex = savedAutoStartIndex;

  savedAutoFrom.hour = prefs.getUInt("afh", 6);    // 6:00 AM default
  savedAutoFrom.minute = prefs.getUInt("afm", 0);
  workingAutoFrom = savedAutoFrom;

  savedAutoTo.hour = prefs.getUInt("ath", 8);      // 8:00 AM default
  savedAutoTo.minute = prefs.getUInt("atm", 0);
  workingAutoTo = savedAutoTo;

  savedDuration.hour = prefs.getUInt("adh", 0);    // 5 minutes default
  savedDuration.minute = prefs.getUInt("adm", 5);
  workingDuration = savedDuration;

  savedRest.hour = prefs.getUInt("arh", 0);        // 1 minute default
  savedRest.minute = prefs.getUInt("arm", 1);
  workingRest = savedRest;

  Serial.println("[PREFS] Configuration loaded.");
}

void saveConfigToPrefs() {
  // Valves
  for (uint8_t i = 0; i < 10; i++) {
    char key[3];
    sprintf(key, "v%d", i);
    prefs.putBool(key, workingValveState[i]);
    savedValveState[i] = workingValveState[i];
  }

  // Pumps
  prefs.putBool("p0", workingPumpState[0]);
  prefs.putBool("p1", workingPumpState[1]);
  savedPumpState[0] = workingPumpState[0];
  savedPumpState[1] = workingPumpState[1];

  // Delay
  prefs.putUInt("delay", workingDelaySec);
  savedDelaySec = workingDelaySec;

  // Power Check
  prefs.putBool("power", workingPowerCheckEnabled);
  savedPowerCheckEnabled = workingPowerCheckEnabled;

  // Auto mode
  prefs.putBool("ae", workingAutoEnabled);
  savedAutoEnabled = workingAutoEnabled;

  prefs.putUInt("as", workingAutoStartIndex);
  savedAutoStartIndex = workingAutoStartIndex;

  prefs.putUInt("afh", workingAutoFrom.hour);
  prefs.putUInt("afm", workingAutoFrom.minute);
  savedAutoFrom = workingAutoFrom;

  prefs.putUInt("ath", workingAutoTo.hour);
  prefs.putUInt("atm", workingAutoTo.minute);
  savedAutoTo = workingAutoTo;

  prefs.putUInt("adh", workingDuration.hour);
  prefs.putUInt("adm", workingDuration.minute);
  savedDuration = workingDuration;

  prefs.putUInt("arh", workingRest.hour);
  prefs.putUInt("arm", workingRest.minute);
  savedRest = workingRest;

  Serial.println("[PREFS] Configuration saved.");
}

// Getter và setter cho valve states
bool getSavedValveState(uint8_t index) {
  if (index < 10) {
    return savedValveState[index];
  }
  return false;
}

void setSavedValveState(uint8_t index, bool state) {
  if (index < 10) {
    savedValveState[index] = state;
  }
}

bool getWorkingValveState(uint8_t index) {
  if (index < 10) {
    return workingValveState[index];
  }
  return false;
}

void setWorkingValveState(uint8_t index, bool state) {
  if (index < 10) {
    workingValveState[index] = state;
  }
}

// Getter và setter cho pump states
bool getSavedPumpState(uint8_t index) {
  if (index < 2) {
    return savedPumpState[index];
  }
  return false;
}

void setSavedPumpState(uint8_t index, bool state) {
  if (index < 2) {
    savedPumpState[index] = state;
  }
}

bool getWorkingPumpState(uint8_t index) {
  if (index < 2) {
    return workingPumpState[index];
  }
  return false;
}

void setWorkingPumpState(uint8_t index, bool state) {
  if (index < 2) {
    workingPumpState[index] = state;
  }
}

// Các hàm getter và setter khác
uint16_t getSavedDelaySec() {
  return savedDelaySec;
}

uint16_t getWorkingDelaySec() {
  return workingDelaySec;
}

void setWorkingDelaySec(uint16_t delay) {
  workingDelaySec = delay;
}

bool isPowerCheckEnabled() {
  return workingPowerCheckEnabled;
}

void setPowerCheckEnabled(bool enabled) {
  workingPowerCheckEnabled = enabled;
}

bool isAutoEnabled() {
  return savedAutoEnabled;
}

void setAutoEnabled(bool enabled) {
  workingAutoEnabled = enabled;
}

uint8_t getAutoStartIndex() {
  return savedAutoStartIndex;
}

void setAutoStartIndex(uint8_t index) {
  workingAutoStartIndex = index;
}

TimeHM getAutoFromTime() {
  return savedAutoFrom;
}

void setAutoFromTime(TimeHM time) {
  workingAutoFrom = time;
}

TimeHM getAutoToTime() {
  return savedAutoTo;
}

void setAutoToTime(TimeHM time) {
  workingAutoTo = time;
}

TimeHM getAutoDurationTime() {
  return savedDuration;
}

void setAutoDurationTime(TimeHM time) {
  workingDuration = time;
}

TimeHM getAutoRestTime() {
  return savedRest;
}

void setAutoRestTime(TimeHM time) {
  workingRest = time;
}