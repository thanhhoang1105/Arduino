#include "Config.h"
#include <Preferences.h>
#include <Arduino.h>

namespace Config
{
    // Các biến cấu hình
    static Preferences prefs;

    // Valve states
    static bool valveState[10];
    static bool pumpState[2];

    // Other settings
    static uint16_t delaySec;
    static bool powerCheckEnabled;

    // Auto mode settings
    static bool autoEnabled;
    static uint8_t autoStartIndex;

    // Auto time settings
    struct TimeHM
    {
        uint8_t hour;
        uint8_t minute;
    };

    static TimeHM autoFrom, autoTo;
    static TimeHM duration, rest;

    void load()
    {
        prefs.begin("cfg", false); // Namespace for preferences

        // Valves
        for (uint8_t i = 0; i < 10; i++)
        {
            String key = "v" + String(i);
            valveState[i] = prefs.getBool(key.c_str(), false);
        }

        // Pumps
        pumpState[0] = prefs.getBool("p0", false);
        pumpState[1] = prefs.getBool("p1", false);

        // Delay
        delaySec = prefs.getUInt("delay", 10); // Default 10s

        // Power Check
        powerCheckEnabled = prefs.getBool("power", false);

        // Auto mode
        autoEnabled = prefs.getBool("ae", false);
        autoStartIndex = prefs.getUInt("as", 1); // 1-10 for display

        // Time settings
        autoFrom.hour = prefs.getUInt("afh", 6); // 6:00 AM default
        autoFrom.minute = prefs.getUInt("afm", 0);

        autoTo.hour = prefs.getUInt("ath", 8); // 8:00 AM default
        autoTo.minute = prefs.getUInt("atm", 0);

        duration.hour = prefs.getUInt("adh", 0); // 5 minutes default
        duration.minute = prefs.getUInt("adm", 5);

        rest.hour = prefs.getUInt("arh", 0); // 1 minute default
        rest.minute = prefs.getUInt("arm", 1);

        Serial.println("[PREFS] Configuration loaded.");
    }

    void save()
    {
        // Valves
        for (uint8_t i = 0; i < 10; i++)
        {
            String key = "v" + String(i);
            prefs.putBool(key.c_str(), valveState[i]);
        }

        // Pumps
        prefs.putBool("p0", pumpState[0]);
        prefs.putBool("p1", pumpState[1]);

        // Delay
        prefs.putUInt("delay", delaySec);

        // Power Check
        prefs.putBool("power", powerCheckEnabled);

        // Auto mode
        prefs.putBool("ae", autoEnabled);
        prefs.putUInt("as", autoStartIndex);

        // Time settings
        prefs.putUInt("afh", autoFrom.hour);
        prefs.putUInt("afm", autoFrom.minute);

        prefs.putUInt("ath", autoTo.hour);
        prefs.putUInt("atm", autoTo.minute);

        prefs.putUInt("adh", duration.hour);
        prefs.putUInt("adm", duration.minute);

        prefs.putUInt("arh", rest.hour);
        prefs.putUInt("arm", rest.minute);

        Serial.println("[PREFS] Configuration saved.");
    }

    // Valve getters & setters
    bool getValveState(uint8_t index)
    {
        if (index < 10)
            return valveState[index];
        return false;
    }

    void setValveState(uint8_t index, bool state)
    {
        if (index < 10)
            valveState[index] = state;
    }

    // Pump getters & setters
    bool getPumpState(uint8_t index)
    {
        if (index < 2)
            return pumpState[index];
        return false;
    }

    void setPumpState(uint8_t index, bool state)
    {
        if (index < 2)
            pumpState[index] = state;
    }

    // Delay getter & setter
    uint16_t getDelaySec()
    {
        return delaySec;
    }

    void setDelaySec(uint16_t value)
    {
        delaySec = value;
    }

    // Power check getter & setter
    bool getPowerCheckEnabled()
    {
        return powerCheckEnabled;
    }

    void setPowerCheckEnabled(bool enabled)
    {
        powerCheckEnabled = enabled;
    }

    // Auto mode getter & setter
    bool getAutoEnabled()
    {
        return autoEnabled;
    }

    void setAutoEnabled(bool enabled)
    {
        autoEnabled = enabled;
    }

    // Auto start index getter & setter
    uint8_t getAutoStartIndex()
    {
        return autoStartIndex;
    }

    void setAutoStartIndex(uint8_t index)
    {
        if (index >= 1 && index <= 10)
            autoStartIndex = index;
    }

    // Auto time getters & setters
    uint8_t getAutoFromHour() { return autoFrom.hour; }
    uint8_t getAutoFromMinute() { return autoFrom.minute; }
    uint8_t getAutoToHour() { return autoTo.hour; }
    uint8_t getAutoToMinute() { return autoTo.minute; }
    uint8_t getDurationHour() { return duration.hour; }
    uint8_t getDurationMinute() { return duration.minute; }
    uint8_t getRestHour() { return rest.hour; }
    uint8_t getRestMinute() { return rest.minute; }

    void setAutoFromTime(uint8_t hour, uint8_t minute)
    {
        autoFrom.hour = hour % 24;
        autoFrom.minute = minute % 60;
    }

    void setAutoToTime(uint8_t hour, uint8_t minute)
    {
        autoTo.hour = hour % 24;
        autoTo.minute = minute % 60;
    }

    void setDurationTime(uint8_t hour, uint8_t minute)
    {
        duration.hour = hour % 24;
        duration.minute = minute % 60;
    }

    void setRestTime(uint8_t hour, uint8_t minute)
    {
        rest.hour = hour % 24;
        rest.minute = minute % 60;
    }
}