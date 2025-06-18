#pragma once
#include <Arduino.h>

// Khai báo các hàm khởi tạo phần cứng
void initializeHardware();
void updateOutputsFromWorking();
bool isAnyValveOpen();
void turnAllOff();