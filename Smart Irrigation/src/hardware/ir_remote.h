#pragma once
#include <Arduino.h>

void initializeIR();
void handleIRInput();
uint8_t getIrNumericBuffer();
uint8_t getIrNumericDigits();
void resetIRNumericInput();