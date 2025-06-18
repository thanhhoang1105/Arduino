#pragma once
#include <Arduino.h>
#include "../types.h"

// Manual irrigation functions
bool isEditingManualHour();
void setEditingManualHour(bool editing);
uint8_t getManualHour();
void setManualHour(uint8_t hour);
uint8_t getManualMin();
void setManualMin(uint8_t min);
bool isManualScheduled();
void setManualScheduled(bool scheduled);
void processManualIrrigation();