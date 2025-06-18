#include "Display.h"
#include "Hardware.h"
#include "Config.h"
#include <Arduino.h>

namespace Display
{
    AppState currentState = STATE_MAIN;
    bool blinkState = false;

    static unsigned long lastBlinkMillis = 0;

    // Biến để lưu trữ trạng thái menu
    static uint8_t mainMenuIndex = 0; // 0=Config, 1=Manual, 2=Auto

    // Config menu
    enum ConfigMode
    {
        MODE_ONOFF,
        MODE_DELAY,
        MODE_POWERCHECK
    };
    static ConfigMode currentConfigMode = MODE_ONOFF;
    static uint8_t configMenuPage = 0;
    static uint8_t configMenuIndex = 0;

    // Auto menu
    static uint8_t autoMenuIndex = 0;
    static uint8_t autoMenuPage = 0;
    static bool inSelectValve = false;
    static bool inTimeSetup = false;
    static uint8_t autoSelectField = 0;

    // Manual menu
    static uint8_t manualHour = 0;
    static uint8_t manualMin = 0;
    static bool editingManualHour = true;

    void drawMainMenu(uint8_t mainIndex, bool autoOn, uint8_t startValve)
    {
        mainMenuIndex = mainIndex;
        Hardware::lcd.clear();
        Hardware::lcd.setCursor(0, 0);
        Hardware::lcd.print("MENU CHINH");

        const char *options[3] = {"CAU HINH", "THU CONG", "TU DONG"};
        for (uint8_t i = 0; i < 3; i++)
        {
            Hardware::lcd.setCursor(0, i + 1);
            Hardware::lcd.print(i == mainMenuIndex ? ">" : " ");
            Hardware::lcd.print(options[i]);

            // Show auto mode status if on the auto menu option
            if (i == 2)
            {
                Hardware::lcd.print(" (");
                Hardware::lcd.print(autoOn ? "ON-" : "OFF-");
                Hardware::lcd.print(startValve);
                Hardware::lcd.print(")");
            }
        }

        currentState = STATE_MAIN;
    }

    void drawConfigMenu(uint8_t mode, uint8_t page, uint8_t index, bool powerCheck, uint16_t delay)
    {
        currentConfigMode = (ConfigMode)mode;
        configMenuPage = page;
        configMenuIndex = index;

        Hardware::lcd.clear();

        uint8_t startIndex;

        switch (currentConfigMode)
        {
        case MODE_ONOFF:
            Hardware::lcd.setCursor(0, 0);
            Hardware::lcd.print("Mode 0: ON/OFF VAN");

            // Show up to 3 valves per page
            startIndex = configMenuPage * 3;
            for (uint8_t row = 0; row < 3; row++)
            {
                uint8_t valveIndex = startIndex + row;
                if (valveIndex >= 10)
                    break; // Không hiển thị quá 10 van

                Hardware::lcd.setCursor(0, row + 1);
                Hardware::lcd.print(row == configMenuIndex ? ">" : " ");
                Hardware::lcd.print("BEC ");
                Hardware::lcd.print(valveIndex + 1); // Hiển thị từ 1-10
                Hardware::lcd.print(": [");
                Hardware::lcd.print(Config::getValveState(valveIndex) ? "ON " : "OFF");
                Hardware::lcd.print("]");
            }

            // Hiển thị mũi tên điều hướng nếu cần
            if (configMenuPage > 0)
            {
                Hardware::lcd.setCursor(18, 1);
                Hardware::lcd.print("^");
            }
            if ((configMenuPage + 1) * 3 < 10)
            {
                Hardware::lcd.setCursor(18, 3);
                Hardware::lcd.print("v");
            }
            break;

        case MODE_DELAY:
            Hardware::lcd.setCursor(0, 0);
            Hardware::lcd.print("Mode 1: DELAY VAN");
            Hardware::lcd.setCursor(0, 1);
            Hardware::lcd.print("> DELAY: ");
            Hardware::lcd.print(delay);
            Hardware::lcd.print(" giay");
            break;

        case MODE_POWERCHECK:
            Hardware::lcd.setCursor(0, 0);
            Hardware::lcd.print("Mode 2: POWER CHECK");
            Hardware::lcd.setCursor(0, 1);
            Hardware::lcd.print("> POWER CHECK: ");
            Hardware::lcd.print(powerCheck ? "ON" : "OFF");
            break;
        }

        currentState = STATE_CONFIG;
    }

    void drawManualMenu(uint8_t hour, uint8_t minute, bool editHour)
    {
        manualHour = hour;
        manualMin = minute;
        editingManualHour = editHour;

        Hardware::lcd.clear();
        Hardware::lcd.setCursor(0, 0);
        Hardware::lcd.print("THU CONG");
        Hardware::lcd.setCursor(0, 1);

        // Display [HH:MM] with blinking effect on the field being edited
        Hardware::lcd.print("[");

        // Hours
        if (editingManualHour && blinkState)
        {
            Hardware::lcd.print("  ");
        }
        else
        {
            if (manualHour < 10)
                Hardware::lcd.print("0");
            Hardware::lcd.print(manualHour);
        }

        Hardware::lcd.print(":");

        // Minutes
        if (!editingManualHour && blinkState)
        {
            Hardware::lcd.print("  ");
        }
        else
        {
            if (manualMin < 10)
                Hardware::lcd.print("0");
            Hardware::lcd.print(manualMin);
        }

        Hardware::lcd.print("]");

        // Show which field is being edited
        Hardware::lcd.setCursor(0, 2);
        Hardware::lcd.print("Dang chinh: ");
        Hardware::lcd.print(editingManualHour ? "GIO" : "PHUT");

        currentState = STATE_MANUAL;
    }

    void drawAutoMenu(uint8_t index, uint8_t page, bool selectValve, bool timeSetup,
                      uint8_t field, bool autoOn, uint8_t startValve)
    {
        autoMenuIndex = index;
        autoMenuPage = page;
        inSelectValve = selectValve;
        inTimeSetup = timeSetup;
        autoSelectField = field;

        Hardware::lcd.clear();
        Hardware::lcd.setCursor(0, 0);

        // Show status in title
        Hardware::lcd.print("TU DONG ");
        Hardware::lcd.print(autoOn ? "(ON-" : "(OFF-");
        Hardware::lcd.print(startValve);
        Hardware::lcd.print(")");

        if (inSelectValve)
        {
            // Valve selection interface
            Hardware::lcd.setCursor(0, 1);
            Hardware::lcd.print("> Chon Van Bat Dau:");
            Hardware::lcd.setCursor(0, 2);
            Hardware::lcd.print("  VAN: [");
            Hardware::lcd.print(Config::getAutoStartIndex());
            Hardware::lcd.print("]");
            return;
        }

        if (inTimeSetup)
        {
            // Time setup interface - FROM-TO on first row
            Hardware::lcd.setCursor(0, 1);
            Hardware::lcd.print("F[");

            // FROM hour
            if (autoSelectField == 0 && blinkState)
            {
                Hardware::lcd.print("  ");
            }
            else
            {
                if (Config::getAutoFromHour() < 10)
                    Hardware::lcd.print("0");
                Hardware::lcd.print(Config::getAutoFromHour());
            }

            Hardware::lcd.print(":");

            // FROM minute
            if (autoSelectField == 1 && blinkState)
            {
                Hardware::lcd.print("  ");
            }
            else
            {
                if (Config::getAutoFromMinute() < 10)
                    Hardware::lcd.print("0");
                Hardware::lcd.print(Config::getAutoFromMinute());
            }

            Hardware::lcd.print("]-T[");

            // TO hour
            if (autoSelectField == 2 && blinkState)
            {
                Hardware::lcd.print("  ");
            }
            else
            {
                if (Config::getAutoToHour() < 10)
                    Hardware::lcd.print("0");
                Hardware::lcd.print(Config::getAutoToHour());
            }

            Hardware::lcd.print(":");

            // TO minute
            if (autoSelectField == 3 && blinkState)
            {
                Hardware::lcd.print("  ");
            }
            else
            {
                if (Config::getAutoToMinute() < 10)
                    Hardware::lcd.print("0");
                Hardware::lcd.print(Config::getAutoToMinute());
            }

            Hardware::lcd.print("]");

            // Duration-Rest on second row
            Hardware::lcd.setCursor(0, 2);
            Hardware::lcd.print("D[");

            // DURATION hour
            if (autoSelectField == 4 && blinkState)
            {
                Hardware::lcd.print("  ");
            }
            else
            {
                if (Config::getDurationHour() < 10)
                    Hardware::lcd.print("0");
                Hardware::lcd.print(Config::getDurationHour());
            }

            Hardware::lcd.print(":");

            // DURATION minute
            if (autoSelectField == 5 && blinkState)
            {
                Hardware::lcd.print("  ");
            }
            else
            {
                if (Config::getDurationMinute() < 10)
                    Hardware::lcd.print("0");
                Hardware::lcd.print(Config::getDurationMinute());
            }

            Hardware::lcd.print("]-R[");

            // REST hour
            if (autoSelectField == 6 && blinkState)
            {
                Hardware::lcd.print("  ");
            }
            else
            {
                if (Config::getRestHour() < 10)
                    Hardware::lcd.print("0");
                Hardware::lcd.print(Config::getRestHour());
            }

            Hardware::lcd.print(":");

            // REST minute
            if (autoSelectField == 7 && blinkState)
            {
                Hardware::lcd.print("  ");
            }
            else
            {
                if (Config::getRestMinute() < 10)
                    Hardware::lcd.print("0");
                Hardware::lcd.print(Config::getRestMinute());
            }

            Hardware::lcd.print("]");
            return;
        }

        // Main auto menu options
        const char *options[5] = {
            "1. BAT TU DONG",
            "2. TAT TU DONG",
            "3. RESET",
            "4. CHON VAN BAT DAU",
            "5. CAI DAT THOI GIAN"};

        // Calculate which page of options to show
        uint8_t itemsPerPage = 3;
        uint8_t totalItems = 5;
        uint8_t totalPages = (totalItems + itemsPerPage - 1) / itemsPerPage;
        uint8_t startItem = autoMenuPage * itemsPerPage;

        // Calculate items to display
        uint8_t itemsToDisplay = totalItems - startItem;
        uint8_t itemsOnThisPage = (itemsPerPage < itemsToDisplay) ? itemsPerPage : itemsToDisplay;

        // Display the current page of menu items
        for (uint8_t i = 0; i < itemsOnThisPage; i++)
        {
            uint8_t itemIndex = startItem + i;
            Hardware::lcd.setCursor(0, i + 1);
            Hardware::lcd.print(itemIndex == autoMenuIndex ? ">" : " ");
            Hardware::lcd.print(options[itemIndex]);
        }

        // Show navigation arrows if needed
        if (autoMenuPage > 0)
        {
            Hardware::lcd.setCursor(18, 1);
            Hardware::lcd.print("^");
        }
        if (autoMenuPage < totalPages - 1)
        {
            Hardware::lcd.setCursor(18, 3);
            Hardware::lcd.print("v");
        }

        currentState = STATE_AUTO;
    }

    void drawManualRunning(uint8_t activeValve, uint16_t remainingSecs, uint8_t nextValve, uint16_t delayRemaining)
    {
        Hardware::lcd.clear();
        Hardware::lcd.setCursor(0, 0);
        Hardware::lcd.print("THU CONG (RUN)");

        // Hiển thị béc hiện tại đang mở
        if (activeValve < 10)
        {
            Hardware::lcd.setCursor(0, 1);
            Hardware::lcd.print("BEC ");
            Hardware::lcd.print(activeValve + 1); // Hiển thị từ 1-10
            Hardware::lcd.print(" (");

            // Hiển thị thời gian còn lại
            uint8_t mins = remainingSecs / 60;
            uint8_t secs = remainingSecs % 60;

            if (mins < 10)
                Hardware::lcd.print("0");
            Hardware::lcd.print(mins);
            Hardware::lcd.print(":");
            if (secs < 10)
                Hardware::lcd.print("0");
            Hardware::lcd.print(secs);
            Hardware::lcd.print(")");
        }

        // Nếu có béc tiếp theo và đang trong thời gian delay
        if (nextValve < 10 && delayRemaining > 0)
        {
            Hardware::lcd.setCursor(0, 2);
            Hardware::lcd.print("BEC ");
            Hardware::lcd.print(nextValve + 1); // Hiển thị từ 1-10
            Hardware::lcd.print(" (");
            Hardware::lcd.print(delayRemaining);
            Hardware::lcd.print("s)");
        }

        currentState = STATE_MANUAL;
    }

    void updateBlink()
    {
        if (millis() - lastBlinkMillis > 500)
        {
            lastBlinkMillis = millis();
            blinkState = !blinkState;

            // Redraw UI if it has blinking elements
            if (currentState == STATE_MANUAL ||
                (currentState == STATE_AUTO && inTimeSetup))
            {
                if (currentState == STATE_MANUAL)
                {
                    drawManualMenu(manualHour, manualMin, editingManualHour);
                }
                else if (currentState == STATE_AUTO)
                {
                    drawAutoMenu(autoMenuIndex, autoMenuPage, inSelectValve,
                                 inTimeSetup, autoSelectField, Config::getAutoEnabled(),
                                 Config::getAutoStartIndex());
                }
            }
        }
    }
}