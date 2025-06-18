#include "BlynkCredentials.h"
#include "TimeBlynk.h"
#include <Arduino.h>
#include <BlynkSimpleEsp32.h> // Giữ dòng này
#include <WiFi.h>
#include <WiFiManager.h>
#include <time.h>
#include "Hardware.h"
#include "Config.h"

namespace TimeBlynk
{
    // NTP settings
    static const char *ntpServer = "pool.ntp.org";
    static const long gmtOffset_sec = 7 * 3600; // GMT+7 (Vietnam)
    static const int daylightOffset_sec = 0;

    // Blynk virtual pin handlers
    BLYNK_WRITE(V1)
    {
        Hardware::setValve(0, param.asInt());
        Config::setValveState(0, param.asInt());
    }
    BLYNK_WRITE(V2)
    {
        Hardware::setValve(1, param.asInt());
        Config::setValveState(1, param.asInt());
    }
    BLYNK_WRITE(V3)
    {
        Hardware::setValve(2, param.asInt());
        Config::setValveState(2, param.asInt());
    }
    BLYNK_WRITE(V4)
    {
        Hardware::setValve(3, param.asInt());
        Config::setValveState(3, param.asInt());
    }
    BLYNK_WRITE(V5)
    {
        Hardware::setValve(4, param.asInt());
        Config::setValveState(4, param.asInt());
    }
    BLYNK_WRITE(V6)
    {
        Hardware::setValve(5, param.asInt());
        Config::setValveState(5, param.asInt());
    }
    BLYNK_WRITE(V7)
    {
        Hardware::setValve(6, param.asInt());
        Config::setValveState(6, param.asInt());
    }
    BLYNK_WRITE(V8)
    {
        Hardware::setValve(7, param.asInt());
        Config::setValveState(7, param.asInt());
    }
    BLYNK_WRITE(V9)
    {
        Hardware::setValve(8, param.asInt());
        Config::setValveState(8, param.asInt());
    }
    BLYNK_WRITE(V10)
    {
        Hardware::setValve(9, param.asInt());
        Config::setValveState(9, param.asInt());
    }
    BLYNK_WRITE(V11)
    {
        Hardware::setPump(0, param.asInt());
        Config::setPumpState(0, param.asInt());
    }
    BLYNK_WRITE(V12)
    {
        Hardware::setPump(1, param.asInt());
        Config::setPumpState(1, param.asInt());
    }

    void initializeWiFi()
    {
        Serial.println("[WIFI] Connecting to WiFi...");
        Hardware::lcd.clear();
        Hardware::lcd.setCursor(0, 0);
        Hardware::lcd.print("KET NOI WIFI...");

        // Use WiFiManager for easier WiFi setup
        WiFiManager wm;
        wm.setConfigPortalTimeout(180); // Portal stays active for 3 minutes

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

    void initializeBlynk()
    {
        Hardware::lcd.setCursor(0, 2);
        Hardware::lcd.print("KET NOI BLYNK...");

        // Initialize Blynk (sử dụng BLYNK_AUTH_TOKEN từ BlynkCredentials.h)
        Blynk.config(BLYNK_AUTH_TOKEN);

        // Thêm timeout khi kết nối
        bool connected = Blynk.connect(30000); // 30 giây timeout

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

    void initializeTime()
    {
        // Đã thiết lập sẵn múi giờ Việt Nam (GMT+7)
        static const long gmtOffset_sec = 7 * 3600; // GMT+7 (Vietnam)
        static const int daylightOffset_sec = 0;

        // Thiết lập đồng bộ NTP trong nền
        configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

        // Không hiển thị "CAI DAT GIO..." nữa

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

        // Không hiển thị "OK" hoặc "LOI" và không delay
    }

    void begin()
    {
        initializeWiFi();
        initializeBlynk();
        initializeTime();

        // Sync Blynk with current valve and pump states
        for (uint8_t i = 0; i < 10; i++)
        {
            Blynk.virtualWrite(i, Config::getValveState(i) ? 1 : 0);
        }

        for (uint8_t j = 0; j < 2; j++)
        {
            Blynk.virtualWrite(10 + j, Config::getPumpState(j) ? 1 : 0);
        }
    }

    void update()
    {
        // This function can be used for additional periodic processing
        // Currently, Blynk.run() is called in the main loop
    }

    void runBlynk()
    {
        // Kiểm tra và kết nối lại nếu mất kết nối
        if (!Blynk.connected())
        {
            Serial.println("[BLYNK] Connection lost, reconnecting...");
            if (Blynk.connect(5000))
            {
                Serial.println("[BLYNK] Reconnected!");
            }
            else
            {
                Serial.println("[BLYNK] Reconnection failed");
            }
        }

        Blynk.run();
    }
}