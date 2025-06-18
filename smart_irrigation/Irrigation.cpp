#include "Irrigation.h"
#include "Config.h"
#include "Hardware.h"
#include "Display.h"
#include <Arduino.h>
#include <time.h>

namespace Irrigation
{
    struct TimeHM
    {
        uint8_t hour;
        uint8_t minute;
    };

    // Manual mode variables
    static uint8_t manualHour;
    static uint8_t manualMin;
    static bool manualScheduled = false;
    static bool isManualRunning = false;
    static unsigned long manualStartMillis;

    // Auto mode variables
    static bool autoRunning = false;
    static unsigned long autoLastActionMillis = 0;
    static uint8_t autoCurrentValve = 0;
    static bool autoStep = false; // false = valve on, true = rest period

    // Thêm các biến mới
    static uint8_t currentActiveValve = 0xFF; // Không có béc nào đang mở
    static uint8_t nextValve = 0xFF;          // Không có béc tiếp theo
    static uint16_t valveRemainingTime = 0;   // Thời gian còn lại của béc hiện tại (giây)
    static uint16_t delayRemainingTime = 0;   // Thời gian delay còn lại (giây)
    static uint32_t lastUpdateMillis = 0;     // Thời điểm cập nhật cuối cùng

    TimeHM getCurrentTimeHM()
    {
        TimeHM currentTime = {0, 0};
        struct tm timeinfo;

        if (getLocalTime(&timeinfo))
        {
            currentTime.hour = timeinfo.tm_hour;
            currentTime.minute = timeinfo.tm_min;
        }

        return currentTime;
    }

    bool nowInTimeRange()
    {
        TimeHM now = getCurrentTimeHM();

        // Convert all to minutes for easier comparison
        int nowMinutes = now.hour * 60 + now.minute;
        int fromMinutes = Config::getAutoFromHour() * 60 + Config::getAutoFromMinute();
        int toMinutes = Config::getAutoToHour() * 60 + Config::getAutoToMinute();

        if (fromMinutes <= toMinutes)
        {
            // Normal time range (e.g., 08:00 to 17:00)
            return (nowMinutes >= fromMinutes && nowMinutes <= toMinutes);
        }
        else
        {
            // Time range spans midnight (e.g., 22:00 to 06:00)
            return (nowMinutes >= fromMinutes || nowMinutes <= toMinutes);
        }
    }

    bool nowMatchesManual()
    {
        TimeHM now = getCurrentTimeHM();
        return (now.hour == manualHour && now.minute == manualMin);
    }

    bool anySavedValveOpen()
    {
        for (uint8_t i = 0; i < 10; i++)
        {
            if (Config::getValveState(i))
                return true;
        }
        return false;
    }

    void scheduleManual(uint8_t hour, uint8_t minute)
    {
        manualHour = hour;
        manualMin = minute;

        if (anySavedValveOpen())
        {
            manualScheduled = true;
            isManualRunning = false;

            // Display confirmation
            Hardware::lcd.clear();
            Hardware::lcd.setCursor(0, 0);
            Hardware::lcd.print("DA HEN GIO TU DONG");
            Hardware::lcd.setCursor(0, 1);
            Hardware::lcd.print("VAO LUC: ");
            if (manualHour < 10)
                Hardware::lcd.print("0");
            Hardware::lcd.print(manualHour);
            Hardware::lcd.print(":");
            if (manualMin < 10)
                Hardware::lcd.print("0");
            Hardware::lcd.print(manualMin);

            // Show which valves will be activated
            Hardware::lcd.setCursor(0, 2);
            Hardware::lcd.print("Van tuoi: ");
            String valveList = "";
            for (uint8_t i = 0; i < 10; i++)
            {
                if (Config::getValveState(i))
                {
                    valveList += String(i + 1) + " ";
                }
            }
            Hardware::lcd.setCursor(0, 3);
            Hardware::lcd.print(valveList);

            delay(3000);
            Display::drawMainMenu(0, Config::getAutoEnabled(), Config::getAutoStartIndex());
        }
        else
        {
            // Error: No valves configured to ON
            Hardware::lcd.clear();
            Hardware::lcd.setCursor(0, 0);
            Hardware::lcd.print("LOI: KHONG CO VAN");
            Hardware::lcd.setCursor(0, 1);
            Hardware::lcd.print("NAO DUOC BAT");
            Hardware::lcd.setCursor(0, 3);
            Hardware::lcd.print("Vao CAU HINH...");
            delay(3000);

            // Set up config for valve selection
            Display::drawConfigMenu(0, 0, 0, Config::getPowerCheckEnabled(), Config::getDelaySec());
        }
    }

    void runManual()
    {
        Serial.println("[MANUAL] Starting manual irrigation cycle.");

        // Tìm béc đầu tiên được cài đặt ON
        currentActiveValve = 0xFF;
        for (uint8_t i = 0; i < 10; i++)
        {
            if (Config::getValveState(i))
            {
                currentActiveValve = i;
                break;
            }
        }

        // Nếu không có béc nào được cài đặt ON, hiển thị lỗi
        if (currentActiveValve == 0xFF)
        {
            Hardware::lcd.clear();
            Hardware::lcd.setCursor(0, 0);
            Hardware::lcd.print("LOI: KHONG CO VAN");
            Hardware::lcd.setCursor(0, 1);
            Hardware::lcd.print("NAO DUOC BAT");
            delay(2000);
            Display::drawMainMenu(0, Config::getAutoEnabled(), Config::getAutoStartIndex());
            return;
        }

        // Tìm béc tiếp theo (nếu có)
        nextValve = 0xFF;
        for (uint8_t i = currentActiveValve + 1; i < 10; i++)
        {
            if (Config::getValveState(i))
            {
                nextValve = i;
                break;
            }
        }

        // Set status flags
        isManualRunning = true;
        manualStartMillis = millis();
        lastUpdateMillis = millis();

        // Set thời gian chạy
        valveRemainingTime = Config::getDelaySec();
        delayRemainingTime = 0;

        // Bật béc hiện tại
        for (uint8_t i = 0; i < 10; i++)
        {
            Hardware::setValve(i, i == currentActiveValve);
        }

        // Hiển thị thông tin
        Display::drawManualRunning(currentActiveValve, valveRemainingTime, nextValve, delayRemainingTime);
    }

    void runAuto()
    {
        // Check if within the configured time range
        if (nowInTimeRange())
        {
            if (!autoRunning)
            {
                // Start the auto cycle
                autoRunning = true;
                autoCurrentValve = Config::getAutoStartIndex() - 1; // Convert to 0-9 index
                autoStep = false;                                   // Start with valve step
                autoLastActionMillis = millis();

                // Display status
                Hardware::lcd.clear();
                Hardware::lcd.setCursor(0, 0);
                Hardware::lcd.print("TU DONG BAT DAU");
                delay(1500);
                Display::drawMainMenu(0, Config::getAutoEnabled(), Config::getAutoStartIndex());
            }

            // Process the automatic irrigation sequence
            if (autoRunning)
            {
                // If in valve step (irrigating)
                if (!autoStep)
                {
                    unsigned long durationMs = ((unsigned long)Config::getDurationHour() * 3600UL +
                                                Config::getDurationMinute() * 60UL) *
                                               1000UL;

                    if (millis() - autoLastActionMillis >= durationMs)
                    {
                        // Valve duration complete, move to rest step
                        for (uint8_t i = 0; i < 10; i++)
                        {
                            Hardware::setValve(i, false);
                        }

                        autoStep = true; // Switch to rest step
                        autoLastActionMillis = millis();

                        // Display status
                        Hardware::lcd.clear();
                        Hardware::lcd.setCursor(0, 0);
                        Hardware::lcd.print("TU DONG: NGHI");
                        Hardware::lcd.setCursor(0, 1);
                        Hardware::lcd.print("Thoi gian: ");
                        Hardware::lcd.print(Config::getRestHour() > 0 ? String(Config::getRestHour()) + "h " : "");
                        Hardware::lcd.print(Config::getRestMinute());
                        Hardware::lcd.print("m");
                    }
                }
                // If in rest step
                else
                {
                    unsigned long restMs = ((unsigned long)Config::getRestHour() * 3600UL +
                                            Config::getRestMinute() * 60UL) *
                                           1000UL;

                    if (millis() - autoLastActionMillis >= restMs)
                    {
                        // Rest complete, move to next valve
                        autoCurrentValve = (autoCurrentValve + 1) % 10; // Cycle through valves 0-9

                        // If we've gone full circle, restart from the configured start valve
                        if (autoCurrentValve == Config::getAutoStartIndex() - 1)
                        {
                            // We've completed a full cycle - check if we're still in the auto time range
                            if (!nowInTimeRange())
                            {
                                autoRunning = false;
                                Hardware::turnAllOff();

                                Hardware::lcd.clear();
                                Hardware::lcd.setCursor(0, 0);
                                Hardware::lcd.print("TU DONG KET THUC");
                                delay(2000);
                                Display::drawMainMenu(0, Config::getAutoEnabled(), Config::getAutoStartIndex());
                                return;
                            }
                        }

                        // Turn on the next valve
                        for (uint8_t i = 0; i < 10; i++)
                        {
                            Hardware::setValve(i, i == autoCurrentValve);
                        }

                        autoStep = false; // Switch to valve step
                        autoLastActionMillis = millis();

                        // Display status
                        Hardware::lcd.clear();
                        Hardware::lcd.setCursor(0, 0);
                        Hardware::lcd.print("TU DONG: VAN ");
                        Hardware::lcd.print(autoCurrentValve + 1); // Display 1-10
                        Hardware::lcd.setCursor(0, 1);
                        Hardware::lcd.print("DANG CHAY...");
                        Hardware::lcd.setCursor(0, 2);
                        Hardware::lcd.print("Thoi gian: ");
                        Hardware::lcd.print(Config::getDurationHour() > 0 ? String(Config::getDurationHour()) + "h " : "");
                        Hardware::lcd.print(Config::getDurationMinute());
                        Hardware::lcd.print("m");
                    }
                }
            }
        }
        // Outside auto time range
        else if (autoRunning)
        {
            autoRunning = false;
            Hardware::turnAllOff();

            Hardware::lcd.clear();
            Hardware::lcd.setCursor(0, 0);
            Hardware::lcd.print("TU DONG NGOAI GIO");
            Hardware::lcd.setCursor(0, 1);
            Hardware::lcd.print("TAT TAT CA");
            delay(2000);
            Display::drawMainMenu(0, Config::getAutoEnabled(), Config::getAutoStartIndex());
        }
    }

    void update()
    {
        // Manual Irrigation Logic
        if (manualScheduled && !isManualRunning)
        {
            if (nowMatchesManual())
            {
                runManual();
            }
        }

        // Check if manual irrigation is running and needs to be stopped
        if (isManualRunning)
        {
            if (millis() - manualStartMillis >= Config::getDelaySec() * 1000UL)
            {
                // Stop irrigation
                for (uint8_t i = 0; i < 10; i++)
                {
                    Hardware::setValve(i, false);
                }
                isManualRunning = false;
                manualScheduled = false;

                Hardware::lcd.clear();
                Hardware::lcd.setCursor(0, 0);
                Hardware::lcd.print("TUOI THU CONG XONG");
                delay(2000);
                Display::drawMainMenu(0, Config::getAutoEnabled(), Config::getAutoStartIndex());
            }
        }

        // Xử lý chế độ thủ công đang chạy
        if (isManualRunning)
        {
            uint32_t now = millis();
            uint32_t elapsed = now - lastUpdateMillis;

            // Cập nhật thời gian mỗi giây
            if (elapsed >= 1000)
            {
                lastUpdateMillis = now;

                // Nếu đang trong thời gian delay
                if (delayRemainingTime > 0)
                {
                    delayRemainingTime--;

                    // Nếu hết thời gian delay, tắt béc hiện tại và bật béc tiếp theo
                    if (delayRemainingTime == 0 && nextValve != 0xFF)
                    {
                        // Tắt béc hiện tại
                        Hardware::setValve(currentActiveValve, false);

                        // Chuyển sang béc tiếp theo
                        currentActiveValve = nextValve;

                        // Tìm béc tiếp theo mới (nếu có)
                        nextValve = 0xFF;
                        for (uint8_t i = currentActiveValve + 1; i < 10; i++)
                        {
                            if (Config::getValveState(i))
                            {
                                nextValve = i;
                                break;
                            }
                        }

                        // Bật béc mới
                        Hardware::setValve(currentActiveValve, true);

                        // Reset thời gian
                        valveRemainingTime = Config::getDelaySec();
                    }
                }
                else if (valveRemainingTime > 0)
                {
                    valveRemainingTime--;

                    // Bắt đầu đếm ngược thời gian delay khi còn 10s
                    uint16_t delayStartThreshold = min(static_cast<uint16_t>(10), Config::getDelaySec());

                    if (valveRemainingTime == delayStartThreshold && nextValve != 0xFF)
                    {
                        delayRemainingTime = delayStartThreshold;
                        Hardware::setValve(nextValve, true);
                    }

                    // Nếu hết thời gian và không có béc tiếp theo, kết thúc tưới
                    if (valveRemainingTime == 0 && nextValve == 0xFF)
                    {
                        // Tắt tất cả van
                        for (uint8_t i = 0; i < 10; i++)
                        {
                            Hardware::setValve(i, false);
                        }

                        isManualRunning = false;
                        Hardware::lcd.clear();
                        Hardware::lcd.setCursor(0, 0);
                        Hardware::lcd.print("TUOI THU CONG XONG");
                        delay(2000);
                        Display::drawMainMenu(0, Config::getAutoEnabled(), Config::getAutoStartIndex());
                    }
                }

                // Cập nhật màn hình
                if (isManualRunning)
                {
                    Display::drawManualRunning(currentActiveValve, valveRemainingTime,
                                               delayRemainingTime > 0 ? nextValve : 0xFF,
                                               delayRemainingTime);
                }
            }
        }

        // Automatic Irrigation Logic
        if (Config::getAutoEnabled())
        {
            runAuto();
        }
    }
}