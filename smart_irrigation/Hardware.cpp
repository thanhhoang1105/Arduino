#include "Hardware.h"
#include <Arduino.h>
#include <Wire.h>

namespace Hardware
{
    // ===========================
    // Khai báo chân điều khiển relay cho 10 van tưới và 2 bơm (Active LOW)
    // ===========================
    static const uint8_t relayValvePins[10] = {2, 4, 16, 17, 5, 18, 19, 21, 22, 23};
    static const uint8_t relayPumpPins[2]   = {26, 25};

    // ===========================
    // Khai báo LCD sử dụng I2C (PCF8574)
    // ===========================
    static const uint8_t LCD_I2C_ADDR = 0x27; // Địa chỉ I2C của LCD
    static const uint8_t LCD_SDA_PIN  = 13;   // Chân SDA
    static const uint8_t LCD_SCL_PIN  = 14;   // Chân SCL
    LiquidCrystal_PCF8574 lcd(LCD_I2C_ADDR);  // Đối tượng LCD toàn cục

    // ===========================
    // Khai báo chân cảm biến dòng ACS712
    // ===========================
    static const uint8_t ACS712_PIN = 34;

    // ===========================
    // Hàm khởi tạo phần cứng hệ thống
    // ===========================
    void begin()
    {
        // Khởi tạo các chân relay cho van tưới (Active LOW: LOW = ON, HIGH = OFF)
        for (uint8_t i = 0; i < 10; i++)
        {
            pinMode(relayValvePins[i], OUTPUT);
            digitalWrite(relayValvePins[i], HIGH); // Mặc định tắt (OFF)
        }

        // Khởi tạo các chân relay cho bơm (Active LOW)
        for (uint8_t j = 0; j < 2; j++)
        {
            pinMode(relayPumpPins[j], OUTPUT);
            digitalWrite(relayPumpPins[j], HIGH); // Mặc định tắt (OFF)
        }

        // Khởi tạo LCD I2C
        Wire.begin(LCD_SDA_PIN, LCD_SCL_PIN);
        lcd.begin(20, 4);           // LCD 20 cột, 4 dòng
        lcd.setBacklight(255);      // Đèn nền sáng tối đa
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("HE THONG TUOI VUON");
        lcd.setCursor(0, 1);
        lcd.print("DANG KHOI DONG...");

        Serial.println("[HW] Hardware initialized.");
    }

    // ===========================
    // Đọc dòng điện từ cảm biến ACS712 (trả về đơn vị Ampe)
    // ===========================
    float readCurrent()
    {
        // Đọc nhiều mẫu để tăng độ chính xác
        const int samples = 10;
        int total = 0;

        for (int i = 0; i < samples; i++)
        {
            total += analogRead(ACS712_PIN);
            delay(1);
        }

        int average = total / samples;

        // Chuyển đổi giá trị ADC sang điện áp (giả sử 3.3V, 12 bit ADC)
        float voltage = (average / 4095.0) * 3.3;

        // Chuyển đổi điện áp sang dòng điện (cho ACS712 5A)
        // Độ nhạy: 185 mV/A, Offset: 2.5V (khi không có dòng)
        float offset = 2.5;
        float sensitivity = 0.185; // 185 mV/A cho loại 5A
        float current = abs((voltage - offset) / sensitivity);

        return current;
    }

    // ===========================
    // Bật/tắt van tưới theo chỉ số (0-9)
    // ===========================
    void setValve(uint8_t idx, bool on)
    {
        if (idx < 10)
        {
            // Active LOW: LOW = ON, HIGH = OFF
            digitalWrite(relayValvePins[idx], on ? LOW : HIGH);
        }
    }

    // ===========================
    // Bật/tắt bơm theo chỉ số (0-1)
    // ===========================
    void setPump(uint8_t idx, bool on)
    {
        if (idx < 2)
        {
            // Active LOW: LOW = ON, HIGH = OFF
            digitalWrite(relayPumpPins[idx], on ? LOW : HIGH);
        }
    }

    // ===========================
    // Tắt toàn bộ van và bơm (an toàn khi khởi động hoặc dừng hệ thống)
    // ===========================
    void turnAllOff()
    {
        // Tắt tất cả van
        for (uint8_t i = 0; i < 10; i++)
        {
            digitalWrite(relayValvePins[i], HIGH); // HIGH = OFF
        }

        // Tắt tất cả bơm
        for (uint8_t j = 0; j < 2; j++)
        {
            digitalWrite(relayPumpPins[j], HIGH); // HIGH = OFF
        }

        Serial.println("[SYSTEM] All relays turned off.");
    }
}