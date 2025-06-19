#include "Utils.h"
#include <Arduino.h>
#include "Hardware.h"
#include "Config.h"

namespace Utils
{
    // Biến lưu thời điểm lần kiểm tra nguồn gần nhất (ms)
    static unsigned long lastPowerCheckMillis = 0;
    // Khoảng thời gian giữa 2 lần kiểm tra nguồn (ms)
    static const unsigned long POWER_CHECK_INTERVAL = 2000; // 2 giây
    // Ngưỡng dòng điện tối thiểu để coi là có nguồn (A)
    static const float POWER_THRESHOLD_AMPS = 0.1;

    // ===========================
    // Hàm kiểm tra trạng thái nguồn điện
    // ===========================
    void checkPowerStatus()
    {
        float current = Hardware::readCurrent();
        Serial.print("[POWER] Current reading: ");
        Serial.print(current, 2);
        Serial.println(" A");

        // Nếu dòng điện nhỏ hơn ngưỡng, coi như mất nguồn
        if (current < POWER_THRESHOLD_AMPS)
        {
            Serial.println("[POWER] CAUTION: Power loss detected!");
            Hardware::turnAllOff(); // Tắt toàn bộ van và bơm để bảo vệ

            // Hiển thị cảnh báo lên LCD
            Hardware::lcd.clear();
            Hardware::lcd.setCursor(0, 0);
            Hardware::lcd.print("MAT NGUON!");
            Hardware::lcd.setCursor(0, 1);
            Hardware::lcd.print("KIEM TRA NGUON DIEN");
            delay(3000);

            // Quay về menu chính sẽ do module Display xử lý
        }
    }

    // ===========================
    // Hàm thực hiện các tác vụ định kỳ
    // ===========================
    void periodicTasks()
    {
        // Nếu bật chức năng kiểm tra nguồn và đủ thời gian, thì kiểm tra nguồn
        if (Config::getPowerCheckEnabled() && (millis() - lastPowerCheckMillis > POWER_CHECK_INTERVAL))
        {
            lastPowerCheckMillis = millis();
            checkPowerStatus();
        }
    }
}