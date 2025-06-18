#include "Utils.h"
#include <Arduino.h>
#include "Hardware.h"
#include "Config.h"

namespace Utils
{
    static unsigned long lastPowerCheckMillis = 0;
    static const unsigned long POWER_CHECK_INTERVAL = 2000; // Check every 2 seconds
    static const float POWER_THRESHOLD_AMPS = 0.1;

    void checkPowerStatus()
    {
        float current = Hardware::readCurrent();
        Serial.print("[POWER] Current reading: ");
        Serial.print(current, 2);
        Serial.println(" A");

        if (current < POWER_THRESHOLD_AMPS)
        {
            Serial.println("[POWER] CAUTION: Power loss detected!");
            Hardware::turnAllOff();

            Hardware::lcd.clear();
            Hardware::lcd.setCursor(0, 0);
            Hardware::lcd.print("MAT NGUON!");
            Hardware::lcd.setCursor(0, 1);
            Hardware::lcd.print("KIEM TRA NGUON DIEN");
            delay(3000);

            // Return to main menu will be handled by Display module
        }
    }

    void periodicTasks()
    {
        // Periodic Power Check if enabled
        if (Config::getPowerCheckEnabled() && (millis() - lastPowerCheckMillis > POWER_CHECK_INTERVAL))
        {
            lastPowerCheckMillis = millis();
            checkPowerStatus();
        }
    }
}