#pragma once
#include <Arduino.h>

void initializeBlynk();
void runBlynk();
void updateBlynkValve(uint8_t index, bool state);
void updateBlynkPump(uint8_t index, bool state);