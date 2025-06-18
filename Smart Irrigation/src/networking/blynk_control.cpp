#include "blynk_control.h"
#include <BlynkSimpleEsp32.h>
#include "../config.h"
#include "../hardware/display.h"
#include "../storage/preferences.h"

char blynkAuthToken[] = BLYNK_AUTH_TOKEN;

void initializeBlynk() {
  Blynk.config(blynkAuthToken);
  if (Blynk.connect()) {
    Serial.println("[BLYNK] Connected to Blynk server");
    displayMessage("KET NOI WIFI OK", WiFi.SSID().c_str(), "BLYNK OK", "", 1000);
  } else {
    Serial.println("[BLYNK] Failed to connect to Blynk server");
    displayMessage("KET NOI WIFI OK", WiFi.SSID().c_str(), "LOI KET NOI BLYNK", "Dang tien hanh...", 2000);
  }
}

void runBlynk() {
  Blynk.run();
}

void updateBlynkValve(uint8_t index, bool state) {
  if (index < 10) {
    Blynk.virtualWrite(index, state ? 1 : 0);
  }
}

void updateBlynkPump(uint8_t index, bool state) {
  if (index < 2) {
    Blynk.virtualWrite(10 + index, state ? 1 : 0);
  }
}

// Các handler Blynk virtual pin được viết ở đây
BLYNK_WRITE(V0) { setWorkingValveState(0, param.asInt()); updateOutputsFromWorking(); }
BLYNK_WRITE(V1) { setWorkingValveState(1, param.asInt()); updateOutputsFromWorking(); }
BLYNK_WRITE(V2) { setWorkingValveState(2, param.asInt()); updateOutputsFromWorking(); }
BLYNK_WRITE(V3) { setWorkingValveState(3, param.asInt()); updateOutputsFromWorking(); }
BLYNK_WRITE(V4) { setWorkingValveState(4, param.asInt()); updateOutputsFromWorking(); }
BLYNK_WRITE(V5) { setWorkingValveState(5, param.asInt()); updateOutputsFromWorking(); }
BLYNK_WRITE(V6) { setWorkingValveState(6, param.asInt()); updateOutputsFromWorking(); }
BLYNK_WRITE(V7) { setWorkingValveState(7, param.asInt()); updateOutputsFromWorking(); }
BLYNK_WRITE(V8) { setWorkingValveState(8, param.asInt()); updateOutputsFromWorking(); }
BLYNK_WRITE(V9) { setWorkingValveState(9, param.asInt()); updateOutputsFromWorking(); }
BLYNK_WRITE(V10) { setWorkingPumpState(0, param.asInt()); updateOutputsFromWorking(); }
BLYNK_WRITE(V11) { setWorkingPumpState(1, param.asInt()); updateOutputsFromWorking(); }