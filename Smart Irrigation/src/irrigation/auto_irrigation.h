#pragma once
#include <Arduino.h>
#include "../types.h"

void processAutoIrrigation();
bool isAutoRunning();
void setAutoRunning(bool running);