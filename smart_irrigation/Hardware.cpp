#include "Hardware.h"
#include <Arduino.h>
#include <Wire.h>

namespace Hardware
{
    // Relay pins: 10 Valves, 2 Pumps (Active LOW)
    static const uint8_t relayValvePins[10] = {2, 4, 16, 17, 5, 18, 19, 21, 22, 23};
    static const uint8_t relayPumpPins[2] = {26, 25};

    // LCD (I2C PCF8574)
    static const uint8_t LCD_I2C_ADDR = 0x27;
    static const uint8_t LCD_SDA_PIN = 13;
    static const uint8_t LCD_SCL_PIN = 14;
    LiquidCrystal_PCF8574 lcd(LCD_I2C_ADDR);

    // ACS712 (Analog Current Sensor)
    static const uint8_t ACS712_PIN = 34;

    void begin()
    {
        // Initialize relay pins (Active LOW: LOW = ON, HIGH = OFF)
        for (uint8_t i = 0; i < 10; i++)
        {
            pinMode(relayValvePins[i], OUTPUT);
            digitalWrite(relayValvePins[i], HIGH); // Default OFF
        }

        for (uint8_t j = 0; j < 2; j++)
        {
            pinMode(relayPumpPins[j], OUTPUT);
            digitalWrite(relayPumpPins[j], HIGH); // Default OFF
        }

        // Initialize LCD
        Wire.begin(LCD_SDA_PIN, LCD_SCL_PIN);
        lcd.begin(20, 4);      // 20 columns, 4 rows
        lcd.setBacklight(255); // Max backlight
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("HE THONG TUOI VUON");
        lcd.setCursor(0, 1);
        lcd.print("DANG KHOI DONG...");

        Serial.println("[HW] Hardware initialized.");
    }

    float readCurrent()
    {
        // Read multiple samples for accuracy
        const int samples = 10;
        int total = 0;

        for (int i = 0; i < samples; i++)
        {
            total += analogRead(ACS712_PIN);
            delay(1);
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

    void setValve(uint8_t idx, bool on)
    {
        if (idx < 10)
        {
            // Active LOW: LOW = ON, HIGH = OFF
            digitalWrite(relayValvePins[idx], on ? LOW : HIGH);
        }
    }

    void setPump(uint8_t idx, bool on)
    {
        if (idx < 2)
        {
            // Active LOW: LOW = ON, HIGH = OFF
            digitalWrite(relayPumpPins[idx], on ? LOW : HIGH);
        }
    }

    void turnAllOff()
    {
        // Turn off all valves and pumps (Active LOW: LOW = ON, HIGH = OFF)
        for (uint8_t i = 0; i < 10; i++)
        {
            digitalWrite(relayValvePins[i], HIGH); // HIGH = OFF
        }

        for (uint8_t j = 0; j < 2; j++)
        {
            digitalWrite(relayPumpPins[j], HIGH); // HIGH = OFF
        }

        Serial.println("[SYSTEM] All relays turned off.");
    }
}