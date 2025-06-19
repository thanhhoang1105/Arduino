#include "IRHandler.h"
#include <IRremote.h>
#include <Arduino.h>
#include "Display.h"
#include "Config.h"
#include "Utils.h"

namespace IRHandler
{
    // ===========================
    // Định nghĩa mã phím điều khiển từ remote IR
    // ===========================
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

    // ===========================
    // Biến trạng thái menu
    // ===========================
    static uint8_t mainMenuIndex = 0;

    // Biến lưu trạng thái tạm thời cho van
    static bool tempValveState[10];
    // Biến lưu trạng thái tạm thời cho delay
    static uint16_t tempDelaySec = 0;
    // Biến lưu trạng thái tạm thời cho kiểm tra nguồn điện
    static bool tempPowerCheck = false;

    // Tạo biến nhập số cho giờ và phút (static để giữ trạng thái)
    static Utils::IRNumberInput hourInput = {0, 23};
    static Utils::IRNumberInput minInput = {0, 59};

    // Enum cho các chế độ cấu hình
    enum ConfigMode
    {
        MODE_ONOFF,     // Bật/tắt từng van
        MODE_DELAY,     // Cài đặt thời gian delay giữa các van
        MODE_POWERCHECK // Bật/tắt kiểm tra nguồn điện
    };
    static ConfigMode currentConfigMode = MODE_ONOFF;
    static uint8_t configMenuPage = 0;  // Trang hiện tại trong menu cấu hình (mỗi trang tối đa 3 van)
    static uint8_t configMenuIndex = 0; // Vị trí chọn hiện tại trong trang

    // ===========================
    // Khởi tạo IR receiver
    // ===========================
    void begin(uint8_t pin)
    {
        IrReceiver.begin(pin, ENABLE_LED_FEEDBACK);
    }

    // ===========================
    // Xử lý phím trong menu chính
    // ===========================
    void handleMainIR(uint32_t code)
    {
        // Menu chính có 2 mục: 0 = CAU HINH, 1 = THU CONG
        if (code == IR_CODE_UP)
        {
            if (mainMenuIndex > 0) mainMenuIndex--;
            Display::drawMainMenu(mainMenuIndex, false, 1);
        }
        else if (code == IR_CODE_DOWN)
        {
            if (mainMenuIndex < 1) mainMenuIndex++;
            Display::drawMainMenu(mainMenuIndex, false, 1);
        }
        else if (code == IR_CODE_OK)
        {
            if (mainMenuIndex == 0)
            {
                // Copy trạng thái van, delay, powerCheck từ Preferences vào vùng tạm
                for (uint8_t i = 0; i < 10; i++)
                    tempValveState[i] = Config::getValveState(i);
                tempDelaySec = Config::getDelaySec();
                tempPowerCheck = Config::getPowerCheckEnabled();

                currentConfigMode = MODE_ONOFF;
                configMenuPage = 0;
                configMenuIndex = 0;
                Display::drawConfigMenu(currentConfigMode, configMenuPage, configMenuIndex,
                                        tempPowerCheck, tempDelaySec, tempValveState);
            }
            else if (mainMenuIndex == 1)
            {
                // Lấy giá trị từ Preferences
                Display::manualHour = Config::getManualHour();
                Display::manualMinute = Config::getManualMinute();
                Display::editingManualHour = true;

                // Đồng bộ lại giá trị cho input struct và reset trạng thái nhập số
                hourInput.value = Display::manualHour;
                hourInput.inputCount = 0;
                hourInput.lastInputMillis = 0;
                minInput.value = Display::manualMinute;
                minInput.inputCount = 0;
                minInput.lastInputMillis = 0;

                Display::drawManualMenu(Display::manualHour, Display::manualMinute, Display::editingManualHour);
            }
        }
    }

    void handleManualIR(uint32_t code)
    {
        int numKey = -1;
        if (code == IR_CODE_0) numKey = 0;
        if (code == IR_CODE_1) numKey = 1;
        if (code == IR_CODE_2) numKey = 2;
        if (code == IR_CODE_3) numKey = 3;
        if (code == IR_CODE_4) numKey = 4;
        if (code == IR_CODE_5) numKey = 5;
        if (code == IR_CODE_6) numKey = 6;
        if (code == IR_CODE_7) numKey = 7;
        if (code == IR_CODE_8) numKey = 8;
        if (code == IR_CODE_9) numKey = 9;

        unsigned long now = millis();

        if (Display::editingManualHour)
        {
            if (numKey != -1) {
                hourInput.maxValue = 23;
                hourInput.processNumKey(numKey, now);
                Display::manualHour = hourInput.value;
                Display::drawManualMenu(Display::manualHour, Display::manualMinute, true);
                return;
            }
            if (code == IR_CODE_UP) {
                hourInput.maxValue = 23;
                hourInput.processUp();
                Display::manualHour = hourInput.value;
                Display::drawManualMenu(Display::manualHour, Display::manualMinute, true);
            }
            else if (code == IR_CODE_DOWN) {
                hourInput.maxValue = 23;
                hourInput.processDown();
                Display::manualHour = hourInput.value;
                Display::drawManualMenu(Display::manualHour, Display::manualMinute, true);
            }
            else if (code == IR_CODE_LEFT || code == IR_CODE_RIGHT) {
                // Chuyển sang chỉnh phút
                Display::editingManualHour = false;
                Display::drawManualMenu(Display::manualHour, Display::manualMinute, false);
            }
        }
        else
        {
            if (numKey != -1) {
                minInput.maxValue = 59;
                minInput.processNumKey(numKey, now);
                Display::manualMinute = minInput.value;
                Display::drawManualMenu(Display::manualHour, Display::manualMinute, false);
                return;
            }
            if (code == IR_CODE_UP) {
                minInput.maxValue = 59;
                minInput.processMinuteUp();
                Display::manualMinute = minInput.value;
                Display::drawManualMenu(Display::manualHour, Display::manualMinute, false);
            }
            else if (code == IR_CODE_DOWN) {
                minInput.maxValue = 59;
                minInput.processMinuteDown();
                Display::manualMinute = minInput.value;
                Display::drawManualMenu(Display::manualHour, Display::manualMinute, false);
            }
            else if (code == IR_CODE_LEFT || code == IR_CODE_RIGHT) {
                // Chuyển sang chỉnh giờ
                Display::editingManualHour = true;
                Display::drawManualMenu(Display::manualHour, Display::manualMinute, true);
            }
        }
        if (code == IR_CODE_HASH) {
            // Lưu giá trị giờ/phút vào Preferences
            Config::setManualHour(Display::manualHour);
            Config::setManualMinute(Display::manualMinute);
            Display::drawMainMenu(1, false, 1);
        }
        else if (code == IR_CODE_STAR) {
            // Thoát không lưu, không cần trả lại giá trị cũ
            Display::drawMainMenu(1, false, 1);
        }
    }

    // ===========================
    // Xử lý phím trong menu cấu hình
    // ===========================
    void handleConfigIR(uint32_t code)
    {
        if (currentConfigMode == MODE_ONOFF)
        {
            // --- Điều khiển bật/tắt van ---
            uint8_t valvesPerPage = 3;
            uint8_t totalValves = 10;
            uint8_t totalPages = (totalValves + valvesPerPage - 1) / valvesPerPage;
            uint8_t currentValveIndex = configMenuPage * valvesPerPage + configMenuIndex;

            if (code == IR_CODE_UP)
            {
                // Di chuyển lên trong trang hoặc sang trang trước
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
                                        tempPowerCheck, tempDelaySec, tempValveState);
            }
            else if (code == IR_CODE_DOWN)
            {
                // Di chuyển xuống trong trang hoặc sang trang sau
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
                                        tempPowerCheck, tempDelaySec, tempValveState);
            }
            else if (code == IR_CODE_LEFT)
            {
                // Chuyển sang chế độ kiểm tra nguồn
                currentConfigMode = MODE_POWERCHECK;
                Display::drawConfigMenu(currentConfigMode, configMenuPage, configMenuIndex,
                                        tempPowerCheck, tempDelaySec, tempValveState);
            }
            else if (code == IR_CODE_RIGHT)
            {
                // Chuyển sang chế độ delay
                currentConfigMode = MODE_DELAY;
                Display::drawConfigMenu(currentConfigMode, configMenuPage, configMenuIndex,
                                        tempPowerCheck, tempDelaySec, tempValveState);
            }
            else if (code == IR_CODE_OK)
            {
                // Đảo trạng thái ON/OFF của van đang chọn (trên vùng tạm)
                if (currentValveIndex < totalValves)
                {
                    tempValveState[currentValveIndex] = !tempValveState[currentValveIndex];
                    Display::drawConfigMenu(currentConfigMode, configMenuPage, configMenuIndex,
                                            tempPowerCheck, tempDelaySec, tempValveState);
                }
            }
        }
        else if (currentConfigMode == MODE_DELAY)
        {
            if (code == IR_CODE_UP)
            {
                if (tempDelaySec < 10)
                    tempDelaySec++;
                else if (tempDelaySec < 60)
                    tempDelaySec += 5;
                else if (tempDelaySec < 300)
                    tempDelaySec += 30;
                else
                    tempDelaySec += 60;
                if (tempDelaySec > 3600)
                    tempDelaySec = 3600;
                Display::drawConfigMenu(currentConfigMode, configMenuPage, configMenuIndex,
                                        tempPowerCheck, tempDelaySec, tempValveState);
            }
            else if (code == IR_CODE_DOWN)
            {
                if (tempDelaySec > 300)
                    tempDelaySec -= 60;
                else if (tempDelaySec > 60)
                    tempDelaySec -= 30;
                else if (tempDelaySec > 10)
                    tempDelaySec -= 5;
                else if (tempDelaySec > 1)
                    tempDelaySec--;
                Display::drawConfigMenu(currentConfigMode, configMenuPage, configMenuIndex,
                                        tempPowerCheck, tempDelaySec, tempValveState);
            }
            else if (code == IR_CODE_LEFT)
            {
                currentConfigMode = MODE_ONOFF;
                configMenuPage = 0;
                configMenuIndex = 0;
                Display::drawConfigMenu(currentConfigMode, configMenuPage, configMenuIndex,
                                        tempPowerCheck, tempDelaySec, tempValveState);
            }
            else if (code == IR_CODE_RIGHT)
            {
                currentConfigMode = MODE_POWERCHECK;
                Display::drawConfigMenu(currentConfigMode, configMenuPage, configMenuIndex,
                                        tempPowerCheck, tempDelaySec, tempValveState);
            }
        }
        else if (currentConfigMode == MODE_POWERCHECK)
        {
            // --- Bật/tắt kiểm tra nguồn điện ---
            if (code == IR_CODE_OK)
            {
                tempPowerCheck = !tempPowerCheck;
                Display::drawConfigMenu(currentConfigMode, configMenuPage, configMenuIndex,
                                        tempPowerCheck, tempDelaySec, tempValveState);
            }
            else if (code == IR_CODE_LEFT)
            {
                currentConfigMode = MODE_DELAY;
                Display::drawConfigMenu(currentConfigMode, configMenuPage, configMenuIndex,
                                        tempPowerCheck, tempDelaySec, tempValveState);
            }
            else if (code == IR_CODE_RIGHT)
            {
                currentConfigMode = MODE_ONOFF;
                configMenuPage = 0;
                configMenuIndex = 0;
                Display::drawConfigMenu(currentConfigMode, configMenuPage, configMenuIndex,
                                        tempPowerCheck, tempDelaySec, tempValveState);
            }
        }

        // --- Hành động chung cho mọi chế độ cấu hình ---
        if (code == IR_CODE_HASH)
        {
            // Lưu trạng thái tạm vào Preferences
            for (uint8_t i = 0; i < 10; i++)
                Config::setValveState(i, tempValveState[i]);
            Config::setDelaySec(tempDelaySec);
            Config::setPowerCheckEnabled(tempPowerCheck);
            Config::save();
            Display::drawMainMenu(mainMenuIndex, false, 1);
        }
        else if (code == IR_CODE_STAR)
        {
            // Thoát, không lưu, trả lại trạng thái gốc từ Preferences
            for (uint8_t i = 0; i < 10; i++)
                tempValveState[i] = Config::getValveState(i);
            tempDelaySec = Config::getDelaySec();
            tempPowerCheck = Config::getPowerCheckEnabled();
            Display::drawMainMenu(mainMenuIndex, false, 1);
        }
    }

    // ===========================
    // Hàm xử lý tín hiệu IR tổng
    // ===========================
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
                default:
                    Display::drawMainMenu(mainMenuIndex, false, 1);
                    break;
                }
            }
            IrReceiver.resume(); // Sẵn sàng nhận tín hiệu IR tiếp theo
        }
    }
}