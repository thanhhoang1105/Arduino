#include "BlynkCredentials.h"
#include "TimeBlynk.h"
#include <Arduino.h>
#include <BlynkSimpleEsp32.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <time.h>
#include "Hardware.h"
#include "Config.h"

namespace TimeBlynk
{
    // ===========================
    // Cấu hình NTP cho múi giờ Việt Nam (GMT+7)
    // ===========================
    static const char *ntpServer = "pool.ntp.org";
    static const long gmtOffset_sec = 7 * 3600; // GMT+7
    static const int daylightOffset_sec = 0;

    // ===========================
    // Xử lý các sự kiện từ Blynk (điều khiển van và bơm)
    // ===========================
    // Khi nhận tín hiệu từ app Blynk, cập nhật trạng thái van/bơm cả phần cứng lẫn cấu hình
    BLYNK_WRITE(V1)  { Hardware::setValve(0, param.asInt());  Config::setValveState(0, param.asInt()); }
    BLYNK_WRITE(V2)  { Hardware::setValve(1, param.asInt());  Config::setValveState(1, param.asInt()); }
    BLYNK_WRITE(V3)  { Hardware::setValve(2, param.asInt());  Config::setValveState(2, param.asInt()); }
    BLYNK_WRITE(V4)  { Hardware::setValve(3, param.asInt());  Config::setValveState(3, param.asInt()); }
    BLYNK_WRITE(V5)  { Hardware::setValve(4, param.asInt());  Config::setValveState(4, param.asInt()); }
    BLYNK_WRITE(V6)  { Hardware::setValve(5, param.asInt());  Config::setValveState(5, param.asInt()); }
    BLYNK_WRITE(V7)  { Hardware::setValve(6, param.asInt());  Config::setValveState(6, param.asInt()); }
    BLYNK_WRITE(V8)  { Hardware::setValve(7, param.asInt());  Config::setValveState(7, param.asInt()); }
    BLYNK_WRITE(V9)  { Hardware::setValve(8, param.asInt());  Config::setValveState(8, param.asInt()); }
    BLYNK_WRITE(V10) { Hardware::setValve(9, param.asInt());  Config::setValveState(9, param.asInt()); }
    BLYNK_WRITE(V11) { Hardware::setPump(0, param.asInt());   Config::setPumpState(0, param.asInt()); }
    BLYNK_WRITE(V12) { Hardware::setPump(1, param.asInt());   Config::setPumpState(1, param.asInt()); }

    // ===========================
    // Khởi tạo WiFi sử dụng WiFiManager
    // ===========================
    void initializeWiFi()
    {
        Serial.println("[WIFI] Connecting to WiFi...");
        Hardware::lcd.clear();
        Hardware::lcd.setCursor(0, 0);
        Hardware::lcd.print("KET NOI WIFI...");

        WiFiManager wm;
        wm.setConfigPortalTimeout(180); // Portal hoạt động 3 phút

        if (!wm.autoConnect("IrrigationSystem"))
        {
            Serial.println("[WIFI] Failed to connect WiFi, retrying...");
            Hardware::lcd.setCursor(0, 1);
            Hardware::lcd.print("KHONG THE KET NOI!");
            delay(2000);
            ESP.restart();
        }

        Serial.print("[WIFI] Connected! IP: ");
        Serial.println(WiFi.localIP());

        Hardware::lcd.setCursor(0, 1);
        Hardware::lcd.print("WIFI OK: ");
        Hardware::lcd.print(WiFi.SSID());
    }

    // ===========================
    // Khởi tạo kết nối Blynk
    // ===========================
    void initializeBlynk()
    {
        Hardware::lcd.setCursor(0, 2);
        Hardware::lcd.print("KET NOI BLYNK...");

        // Kết nối Blynk sử dụng token trong BlynkCredentials.h
        Blynk.config(BLYNK_AUTH_TOKEN);

        // Thử kết nối trong 30 giây
        bool connected = Blynk.connect(30000);

        if (connected)
        {
            Serial.println("[BLYNK] Connected.");
            Hardware::lcd.print(" OK");
        }
        else
        {
            Serial.println("[BLYNK] Connection failed.");
            Hardware::lcd.print(" LOI");
        }
        delay(1000);
    }

    // ===========================
    // Khởi tạo đồng bộ thời gian NTP
    // ===========================
    void initializeTime()
    {
        configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

        struct tm timeinfo;
        if (getLocalTime(&timeinfo))
        {
            char timeStr[9];
            strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);
            Serial.printf("[TIME] Múi giờ Việt Nam (GMT+7): %s\n", timeStr);
        }
        else
        {
            Serial.println("[TIME] Đang sử dụng múi giờ mặc định (GMT+7)");
        }
    }

    // ===========================
    // Hàm khởi tạo toàn bộ hệ thống mạng, Blynk, thời gian
    // ===========================
    void begin()
    {
        initializeWiFi();
        initializeBlynk();
        initializeTime();

        // Đồng bộ trạng thái van và bơm lên Blynk
        for (uint8_t i = 0; i < 10; i++)
            Blynk.virtualWrite(i, Config::getValveState(i) ? 1 : 0);

        for (uint8_t j = 0; j < 2; j++)
            Blynk.virtualWrite(10 + j, Config::getPumpState(j) ? 1 : 0);
    }

    // ===========================
    // Hàm update (dự phòng cho xử lý định kỳ nếu cần)
    // ===========================
    void update()
    {
        // Hiện tại không xử lý gì thêm ở đây
    }

    // ===========================
    // Hàm chạy Blynk, tự động reconnect nếu mất kết nối
    // ===========================
    void runBlynk()
    {
        if (!Blynk.connected())
        {
            Serial.println("[BLYNK] Connection lost, reconnecting...");
            if (Blynk.connect(5000))
                Serial.println("[BLYNK] Reconnected!");
            else
                Serial.println("[BLYNK] Reconnection failed");
        }
        Blynk.run();
    }
}