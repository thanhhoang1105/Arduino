#include "sensors.h"
#include "../config.h"
#include "../storage/preferences.h"
#include "hardware.h"
#include "display.h"

unsigned long lastPowerCheckMillis = 0;

float readACS712Current() {
  // Read multiple samples for accuracy
  const int samples = 10;
  int total = 0;

  for (int i = 0; i < samples; i++) {
    total += analogRead(ACS712_PIN);
    delay(2);
  }

  int average = total / samples;

  // Convert ADC value to voltage
  float voltage = (average / 4095.0) * 3.3;

  // Convert voltage to current (ACS712 30A model)
  // Sensitivity: 66 mV/A, Offset: 2.5V (when 0A)
  float offset = 2.5;
  float sensitivity = 0.066;
  float current = abs((voltage - offset) / sensitivity);

  return current;
}

void checkPower() {
  if (isPowerCheckEnabled() && (millis() - lastPowerCheckMillis > POWER_CHECK_INTERVAL)) {
    lastPowerCheckMillis = millis();

    float current = readACS712Current();
    Serial.print("[POWER] Current reading: ");
    Serial.print(current, 2);
    Serial.println(" A");

    if (isAnyValveOpen() && current < POWER_THRESHOLD_AMPS) {
      Serial.println("[POWER] Power failure detected! Turning all off.");
      displayMessage("LOI DIEN!", "Dang tat he thong...", "", "", 2000);
      turnAllOff();
    }
  }
}