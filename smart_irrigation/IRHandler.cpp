#include "IRHandler.h"
#include <IRremote.h>
#include <Arduino.h>
#include "Display.h"
#include "Config.h"
#include "Irrigation.h"

namespace IRHandler
{
// IR Remote Control Codes
#define IR_CODE_1 0xBA45FF00UL
#define IR_CODE_2 0xB946FF00UL
#define IR_CODE_3 0xB847FF00UL
#define IR_CODE_4 0xBB44FF00UL
#define IR_CODE_5 0xBF40FF00UL
#define IR_CODE_6 0xBC43FF00UL
#define IR_CODE_7 0xF807FF00UL
#define IR_CODE_8 0xEA15FF00UL
#define IR_CODE_9 0xF609FF00UL
#define IR_CODE_0 0xE619FF00UL

#define IR_CODE_STAR 0xE916FF00UL // CANCEL / BACK
#define IR_CODE_HASH 0xF20DFF00UL // SAVE / CONFIRM

#define IR_CODE_UP 0xE718FF00UL
#define IR_CODE_DOWN 0xAD52FF00UL
#define IR_CODE_RIGHT 0xA55AFF00UL
#define IR_CODE_LEFT 0xF708FF00UL
#define IR_CODE_OK 0xE31CFF00UL

    // Menu state variables
    static uint8_t mainMenuIndex = 0;

    enum ConfigMode
    {
        MODE_ONOFF,
        MODE_DELAY,
        MODE_POWERCHECK
    };
    static ConfigMode currentConfigMode = MODE_ONOFF;
    static uint8_t configMenuPage = 0;
    static uint8_t configMenuIndex = 0;

    static uint8_t autoMenuIndex = 0;
    static uint8_t autoMenuPage = 0;
    static bool inSelectValve = false;
    static bool inTimeSetup = false;
    static uint8_t autoSelectField = 0;

    static uint8_t manualHour = 0;
    static uint8_t manualMin = 0;
    static bool editingManualHour = true;

    // For numeric input
    static uint8_t irNumericBuffer = 0;
    static uint8_t irNumericDigits = 0;

    // Manual irrigation scheduling
    static bool manualScheduled = false;

    void begin(uint8_t pin)
    {
        IrReceiver.begin(pin, ENABLE_LED_FEEDBACK);
    }

    void resetNumericInput()
    {
        irNumericBuffer = 0;
        irNumericDigits = 0;
    }

    void handleMainIR(uint32_t code)
    {
        if (code == IR_CODE_UP)
        {
            mainMenuIndex = (mainMenuIndex == 0) ? 2 : mainMenuIndex - 1;
            Display::drawMainMenu(mainMenuIndex, Config::getAutoEnabled(), Config::getAutoStartIndex());
        }
        else if (code == IR_CODE_DOWN)
        {
            mainMenuIndex = (mainMenuIndex + 1) % 3;
            Display::drawMainMenu(mainMenuIndex, Config::getAutoEnabled(), Config::getAutoStartIndex());
        }
        else if (code == IR_CODE_OK)
        {
            switch (mainMenuIndex)
            {
            case 0: // Config
                currentConfigMode = MODE_ONOFF;
                configMenuPage = 0;
                configMenuIndex = 0;
                Display::drawConfigMenu(currentConfigMode, configMenuPage, configMenuIndex,
                                        Config::getPowerCheckEnabled(), Config::getDelaySec());
                break;

            case 1: // Manual
                editingManualHour = true;
                manualHour = 0;
                manualMin = 0;
                Display::drawManualMenu(manualHour, manualMin, editingManualHour);
                break;

            case 2: // Auto
                autoMenuIndex = 0;
                autoMenuPage = 0;
                inSelectValve = false;
                inTimeSetup = false;
                Display::drawAutoMenu(autoMenuIndex, autoMenuPage, inSelectValve, inTimeSetup,
                                      autoSelectField, Config::getAutoEnabled(), Config::getAutoStartIndex());
                break;
            }
        }
    }

    void handleConfigIR(uint32_t code)
    {
        if (currentConfigMode == MODE_ONOFF)
        {
            uint8_t valvesPerPage = 3;
            uint8_t totalValves = 10;
            uint8_t totalPages = (totalValves + valvesPerPage - 1) / valvesPerPage;
            uint8_t currentValveIndex = configMenuPage * valvesPerPage + configMenuIndex;

            if (code == IR_CODE_UP)
            {
                if (configMenuIndex > 0)
                {
                    configMenuIndex--;
                }
                else if (configMenuPage > 0)
                {
                    configMenuPage--;
                    configMenuIndex = min(valvesPerPage - 1, totalValves - configMenuPage * valvesPerPage - 1);
                }
                Display::drawConfigMenu(currentConfigMode, configMenuPage, configMenuIndex,
                                        Config::getPowerCheckEnabled(), Config::getDelaySec());
            }
            else if (code == IR_CODE_DOWN)
            {
                if (configMenuIndex < min(valvesPerPage - 1, totalValves - configMenuPage * valvesPerPage - 1))
                {
                    configMenuIndex++;
                }
                else if (configMenuPage < totalPages - 1)
                {
                    configMenuPage++;
                    configMenuIndex = 0;
                }
                Display::drawConfigMenu(currentConfigMode, configMenuPage, configMenuIndex,
                                        Config::getPowerCheckEnabled(), Config::getDelaySec());
            }
            else if (code == IR_CODE_LEFT)
            {
                currentConfigMode = MODE_POWERCHECK;
                Display::drawConfigMenu(currentConfigMode, configMenuPage, configMenuIndex,
                                        Config::getPowerCheckEnabled(), Config::getDelaySec());
            }
            else if (code == IR_CODE_RIGHT)
            {
                currentConfigMode = MODE_DELAY;
                Display::drawConfigMenu(currentConfigMode, configMenuPage, configMenuIndex,
                                        Config::getPowerCheckEnabled(), Config::getDelaySec());
            }
            else if (code == IR_CODE_OK)
            {
                if (currentValveIndex < totalValves)
                {
                    bool currentState = Config::getValveState(currentValveIndex);
                    Config::setValveState(currentValveIndex, !currentState);
                    Display::drawConfigMenu(currentConfigMode, configMenuPage, configMenuIndex,
                                            Config::getPowerCheckEnabled(), Config::getDelaySec());
                }
            }
        }
        else if (currentConfigMode == MODE_DELAY)
        {
            uint16_t delaySec = Config::getDelaySec();

            if (code == IR_CODE_UP)
            {
                if (delaySec < 10)
                {
                    delaySec++;
                }
                else if (delaySec < 60)
                {
                    delaySec += 5;
                }
                else if (delaySec < 300)
                {
                    delaySec += 30;
                }
                else
                {
                    delaySec += 60;
                }
                if (delaySec > 3600)
                    delaySec = 3600; // Max 1 hour
                Config::setDelaySec(delaySec);
                Display::drawConfigMenu(currentConfigMode, configMenuPage, configMenuIndex,
                                        Config::getPowerCheckEnabled(), delaySec);
            }
            else if (code == IR_CODE_DOWN)
            {
                if (delaySec > 300)
                {
                    delaySec -= 60;
                }
                else if (delaySec > 60)
                {
                    delaySec -= 30;
                }
                else if (delaySec > 10)
                {
                    delaySec -= 5;
                }
                else if (delaySec > 1)
                {
                    delaySec--;
                }
                Config::setDelaySec(delaySec);
                Display::drawConfigMenu(currentConfigMode, configMenuPage, configMenuIndex,
                                        Config::getPowerCheckEnabled(), delaySec);
            }
            else if (code == IR_CODE_LEFT)
            {
                currentConfigMode = MODE_ONOFF;
                configMenuPage = 0;
                configMenuIndex = 0;
                Display::drawConfigMenu(currentConfigMode, configMenuPage, configMenuIndex,
                                        Config::getPowerCheckEnabled(), Config::getDelaySec());
            }
            else if (code == IR_CODE_RIGHT)
            {
                currentConfigMode = MODE_POWERCHECK;
                Display::drawConfigMenu(currentConfigMode, configMenuPage, configMenuIndex,
                                        Config::getPowerCheckEnabled(), Config::getDelaySec());
            }
        }
        else if (currentConfigMode == MODE_POWERCHECK)
        {
            if (code == IR_CODE_OK)
            {
                Config::setPowerCheckEnabled(!Config::getPowerCheckEnabled());
                Display::drawConfigMenu(currentConfigMode, configMenuPage, configMenuIndex,
                                        Config::getPowerCheckEnabled(), Config::getDelaySec());
            }
            else if (code == IR_CODE_LEFT)
            {
                currentConfigMode = MODE_DELAY;
                Display::drawConfigMenu(currentConfigMode, configMenuPage, configMenuIndex,
                                        Config::getPowerCheckEnabled(), Config::getDelaySec());
            }
            else if (code == IR_CODE_RIGHT)
            {
                currentConfigMode = MODE_ONOFF;
                configMenuPage = 0;
                configMenuIndex = 0;
                Display::drawConfigMenu(currentConfigMode, configMenuPage, configMenuIndex,
                                        Config::getPowerCheckEnabled(), Config::getDelaySec());
            }
        }

        // Common actions for all config modes
        if (code == IR_CODE_HASH)
        {
            // Save configuration
            Config::save();

            // Return to main menu
            Display::drawMainMenu(mainMenuIndex, Config::getAutoEnabled(), Config::getAutoStartIndex());
        }
        else if (code == IR_CODE_STAR)
        {
            // Cancel changes
            Config::load();

            // Return to main menu
            Display::drawMainMenu(mainMenuIndex, Config::getAutoEnabled(), Config::getAutoStartIndex());
        }
    }

    void handleManualIR(uint32_t code)
    {
        // Handle direct numeric input
        if (code >= IR_CODE_0 && code <= IR_CODE_9)
        {
            uint8_t num = 0;
            if (code == IR_CODE_0)
                num = 0;
            else if (code == IR_CODE_1)
                num = 1;
            else if (code == IR_CODE_2)
                num = 2;
            else if (code == IR_CODE_3)
                num = 3;
            else if (code == IR_CODE_4)
                num = 4;
            else if (code == IR_CODE_5)
                num = 5;
            else if (code == IR_CODE_6)
                num = 6;
            else if (code == IR_CODE_7)
                num = 7;
            else if (code == IR_CODE_8)
                num = 8;
            else if (code == IR_CODE_9)
                num = 9;

            if (irNumericDigits == 0)
            {
                irNumericBuffer = num;
                irNumericDigits = 1;
            }
            else
            {
                irNumericBuffer = irNumericBuffer * 10 + num;
                irNumericDigits = 2;
            }

            if (editingManualHour)
            {
                if (irNumericBuffer > 23)
                {
                    irNumericBuffer = (irNumericDigits == 1) ? num : 23;
                }
                manualHour = irNumericBuffer;
            }
            else
            {
                if (irNumericBuffer > 59)
                {
                    irNumericBuffer = (irNumericDigits == 1) ? num : 59;
                }
                manualMin = irNumericBuffer;
            }

            if (irNumericDigits == 2)
            {
                resetNumericInput();
            }

            Display::drawManualMenu(manualHour, manualMin, editingManualHour);
            return;
        }

        // Reset numeric input on other keys
        resetNumericInput();

        // Handle other navigation and control keys
        if (code == IR_CODE_UP)
        {
            if (editingManualHour)
            {
                manualHour = (manualHour + 1) % 24;
            }
            else
            {
                // Tăng phút theo bội số của 15
                manualMin = (manualMin + 15) % 60;
                if (manualMin > 45)
                {
                    manualMin = 0;
                }
            }
            Display::drawManualMenu(manualHour, manualMin, editingManualHour);
        }
        else if (code == IR_CODE_DOWN)
        {
            if (editingManualHour)
            {
                manualHour = (manualHour > 0) ? manualHour - 1 : 23;
            }
            else
            {
                // Giảm phút theo bội số của 15
                if (manualMin < 15)
                {
                    manualMin = 0;
                }
                else
                {
                    manualMin = manualMin - 15;
                }
            }
            Display::drawManualMenu(manualHour, manualMin, editingManualHour);
        }
        else if (code == IR_CODE_OK)
        {
            if (editingManualHour)
            {
                // Chuyển từ chỉnh giờ sang chỉnh phút
                editingManualHour = false;
                Display::drawManualMenu(manualHour, manualMin, editingManualHour);
            }
            else
            {
                // Nếu đang chỉnh phút và bấm OK thì khởi chạy ngay
                Irrigation::runManual();
            }
        }
        else if (code == IR_CODE_HASH)
        {
            // Chạy chế độ thủ công ngay lập tức khi ấn #
            Irrigation::runManual();
        }
        else if (code == IR_CODE_STAR)
        {
            // Quay lại
            Display::drawMainMenu(mainMenuIndex, Config::getAutoEnabled(), Config::getAutoStartIndex());
        }
    }

    void handleAutoIR(uint32_t code)
    {
        // TODO: Implement Auto IR handling
        // This is complex and would require significant code
        // Consider implementing key parts as needed
    }

    void process()
    {
        if (IrReceiver.decode())
        {
            uint32_t irCode = IrReceiver.decodedIRData.decodedRawData;
            if (irCode != 0)
            {
                Serial.print("[IR] Code received: 0x");
                Serial.println(irCode, HEX);

                switch (Display::currentState)
                {
                case STATE_MAIN:
                    handleMainIR(irCode);
                    break;
                case STATE_CONFIG:
                    handleConfigIR(irCode);
                    break;
                case STATE_MANUAL:
                    handleManualIR(irCode);
                    break;
                case STATE_AUTO:
                    handleAutoIR(irCode);
                    break;
                default:
                    Display::drawMainMenu(mainMenuIndex, Config::getAutoEnabled(), Config::getAutoStartIndex());
                    break;
                }
            }
            IrReceiver.resume(); // Ready for next IR signal
        }
    }
}