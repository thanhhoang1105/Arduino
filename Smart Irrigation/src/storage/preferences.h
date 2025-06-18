#pragma once
#include <Arduino.h>
#include "../types.h"

// Function declarations for preference management
void initializePreferences();
void loadConfigFromPrefs();
void saveConfigToPrefs();

// Getters and setters for valve states
bool getSavedValveState(uint8_t index);
void setSavedValveState(uint8_t index, bool state);
bool getWorkingValveState(uint8_t index);
void setWorkingValveState(uint8_t index, bool state);

// Getters and setters for pump states
bool getSavedPumpState(uint8_t index);
void setSavedPumpState(uint8_t index, bool state);
bool getWorkingPumpState(uint8_t index);
void setWorkingPumpState(uint8_t index, bool state);

// Getters and setters for delay settings
uint16_t getSavedDelaySec();
uint16_t getWorkingDelaySec();
void setWorkingDelaySec(uint16_t delay);

// Power check settings
bool isPowerCheckEnabled();
void setPowerCheckEnabled(bool enabled);

// Auto mode settings
bool isAutoEnabled();
void setAutoEnabled(bool enabled);
uint8_t getAutoStartIndex();
void setAutoStartIndex(uint8_t index);

// Auto mode time settings
TimeHM getAutoFromTime();
void setAutoFromTime(TimeHM time);
TimeHM getAutoToTime();
void setAutoToTime(TimeHM time);
TimeHM getAutoDurationTime();
void setAutoDurationTime(TimeHM time);
TimeHM getAutoRestTime();
void setAutoRestTime(TimeHM time);

// External declarations of global variables
extern bool savedValveState[10];
extern bool workingValveState[10];
extern bool savedPumpState[2];
extern bool workingPumpState[2];
extern uint16_t savedDelaySec;
extern uint16_t workingDelaySec;
extern bool savedPowerCheckEnabled;
extern bool workingPowerCheckEnabled;
extern bool savedAutoEnabled;
extern bool workingAutoEnabled;
extern uint8_t savedAutoStartIndex;
extern uint8_t workingAutoStartIndex;
extern TimeHM savedAutoFrom, workingAutoFrom;
extern TimeHM savedAutoTo, workingAutoTo;
extern TimeHM savedDuration, workingDuration;
extern TimeHM savedRest, workingRest;