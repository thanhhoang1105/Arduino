#include "Config.h"
#include <Preferences.h>
#include <Arduino.h>

namespace Config
{
    // Đối tượng Preferences để lưu/đọc dữ liệu vào bộ nhớ flash của ESP32
    static Preferences prefs;

    // Mảng lưu trạng thái của 10 van tưới (true: bật, false: tắt)
    static bool valveState[10];

    // Mảng lưu trạng thái của 2 bơm (true: bật, false: tắt)
    static bool pumpState[2];

    // Thời gian delay giữa các van (đơn vị: giây)
    static uint16_t delaySec;

    // Cờ bật/tắt chức năng kiểm tra nguồn điện
    static bool powerCheckEnabled;

    // =========================
    // Hàm load(): Đọc cấu hình từ flash
    // =========================
    void load()
    {
        prefs.begin("cfg", false); // Mở vùng nhớ "cfg" để thao tác

        // Đọc trạng thái từng van từ flash, mặc định là OFF (false)
        for (uint8_t i = 0; i < 10; i++)
        {
            String key = "v" + String(i);
            valveState[i] = prefs.getBool(key.c_str(), false);
        }

        // Đọc trạng thái từng bơm, mặc định là OFF (false)
        pumpState[0] = prefs.getBool("p0", false);
        pumpState[1] = prefs.getBool("p1", false);

        // Đọc thời gian delay giữa các van, mặc định là 10 giây
        delaySec = prefs.getUInt("delay", 10);

        // Đọc trạng thái kiểm tra nguồn, mặc định là OFF (false)
        powerCheckEnabled = prefs.getBool("power", false);

        Serial.println("[PREFS] Configuration loaded.");
    }

    // =========================
    // Hàm save(): Lưu cấu hình hiện tại vào flash
    // =========================
    void save()
    {
        // Lưu trạng thái từng van vào flash
        for (uint8_t i = 0; i < 10; i++)
        {
            String key = "v" + String(i);
            prefs.putBool(key.c_str(), valveState[i]);
        }

        // Lưu trạng thái từng bơm vào flash
        prefs.putBool("p0", pumpState[0]);
        prefs.putBool("p1", pumpState[1]);

        // Lưu thời gian delay giữa các van
        prefs.putUInt("delay", delaySec);

        // Lưu trạng thái kiểm tra nguồn
        prefs.putBool("power", powerCheckEnabled);

        Serial.println("[PREFS] Configuration saved.");
    }

    // =========================
    // Getter/Setter cho trạng thái van tưới
    // =========================
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

    // =========================
    // Getter/Setter cho trạng thái bơm
    // =========================
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

    // =========================
    // Getter/Setter cho thời gian delay giữa các van
    // =========================
    uint16_t getDelaySec()
    {
        return delaySec;
    }

    void setDelaySec(uint16_t value)
    {
        delaySec = value;
    }

    // =========================
    // Getter/Setter cho kiểm tra nguồn điện
    // =========================
    bool getPowerCheckEnabled()
    {
        return powerCheckEnabled;
    }

    void setPowerCheckEnabled(bool enabled)
    {
        powerCheckEnabled = enabled;
    }

    uint8_t getManualHour() {
        prefs.begin("cfg", false);
        uint8_t h = prefs.getUChar("manualHour", 0);
        prefs.end();
        return h;
    }
    void setManualHour(uint8_t hour) {
        prefs.begin("cfg", false);
        prefs.putUChar("manualHour", hour);
        prefs.end();
    }
    uint8_t getManualMinute() {
        prefs.begin("cfg", false);
        uint8_t m = prefs.getUChar("manualMinute", 0);
        prefs.end();
        return m;
    }
    void setManualMinute(uint8_t minute) {
        prefs.begin("cfg", false);
        prefs.putUChar("manualMinute", minute);
        prefs.end();
    }
}