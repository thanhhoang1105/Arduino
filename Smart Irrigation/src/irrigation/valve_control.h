#pragma once
#include <Arduino.h>

void setValveState(uint8_t index, bool state);
void setPumpState(uint8_t index, bool state);
void syncValvesWithSaved();
void syncPumpsWithSaved();