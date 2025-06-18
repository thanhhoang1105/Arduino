#pragma once
#include <Arduino.h>
#include "../types.h"

// Menu system functions
void handleIR(uint32_t code);
TimeHM getCurrentTimeHM();
bool nowInTimeRange(const TimeHM &from, const TimeHM &to);
bool nowMatchesManualTime();
bool anySavedValveOpen();