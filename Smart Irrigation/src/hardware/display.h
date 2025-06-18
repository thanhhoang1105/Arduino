#pragma once
#include <Arduino.h>

void initializeDisplay();
void displayMessage(const char* line1, const char* line2 = "", const char* line3 = "", const char* line4 = "", int delayMs = 0);