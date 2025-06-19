#include "Display.h"
#include "Hardware.h"
#include "Config.h"
#include <Arduino.h>

namespace Display
{
    // Biến lưu trạng thái giao diện hiện tại (menu chính hoặc cấu hình)
    AppState currentState = STATE_MAIN;

    // Chỉ số mục đang chọn trong menu chính (ở đây chỉ còn 1 mục là "CAU HINH")
    static uint8_t mainMenuIndex = 0;

    // Biến lưu trạng thái thủ công
    uint8_t manualHour = 0;
    uint8_t manualMinute = 0;
    bool editingManualHour = true;

    // Enum xác định chế độ cấu hình hiện tại
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
    // Hàm vẽ màn hình menu chính
    // ===========================
    // Chỉ còn 1 lựa chọn là "CAU HINH"
    void drawMainMenu(uint8_t mainIndex, bool /*autoOn*/, uint8_t /*startValve*/)
    {
        mainMenuIndex = mainIndex;
        Hardware::lcd.clear();
        Hardware::lcd.setCursor(0, 0);
        Hardware::lcd.print("MENU CHINH");

        // Hiển thị duy nhất 1 tùy chọn: CAU HINH
        const char *options[2] = {"CAU HINH", "THU CONG"};
        for (uint8_t i = 0; i < 2; i++)
        {
            Hardware::lcd.setCursor(0, i + 1);
            // Nếu đang chọn mục này thì hiển thị dấu '>', ngược lại là khoảng trắng
            Hardware::lcd.print(i == mainMenuIndex ? ">" : " ");
            Hardware::lcd.print(options[i]);
        }

        // Cập nhật trạng thái giao diện
        currentState = STATE_MAIN;
    }

    // ==========================================
    // Hàm vẽ màn hình cấu hình (Config Menu)
    // ==========================================
    // Hiển thị 3 chế độ: Bật/tắt van, delay giữa các van, kiểm tra nguồn
    void drawConfigMenu(uint8_t mode, uint8_t page, uint8_t index, bool powerCheck, uint16_t delay, const bool* valveStates)
    {
        currentConfigMode = (ConfigMode)mode;
        configMenuPage = page;
        configMenuIndex = index;

        Hardware::lcd.clear();

        uint8_t startIndex;

        switch (currentConfigMode)
        {
        case MODE_ONOFF:
            // Hiển thị chế độ bật/tắt từng van
            Hardware::lcd.setCursor(0, 0);
            Hardware::lcd.print("Mode 0: ON/OFF VAN");

            // Hiển thị tối đa 3 van trên 1 trang
            startIndex = configMenuPage * 3;
            for (uint8_t row = 0; row < 3; row++)
            {
                uint8_t valveIndex = startIndex + row;
                if (valveIndex >= 10)
                    break; // Không hiển thị quá 10 van

                Hardware::lcd.setCursor(0, row + 1);
                // Hiển thị dấu '>' ở dòng đang chọn
                Hardware::lcd.print(row == configMenuIndex ? ">" : " ");
                Hardware::lcd.print("BEC ");
                Hardware::lcd.print(valveIndex + 1); // Hiển thị số van (1-10)
                Hardware::lcd.print(": [");
                // Sử dụng trạng thái tạm nếu có, nếu không thì lấy từ Config
                bool state = valveStates ? valveStates[valveIndex] : Config::getValveState(valveIndex);
                Hardware::lcd.print(state ? "ON " : "OFF");
                Hardware::lcd.print("]");
            }

            // Hiển thị mũi tên điều hướng nếu có nhiều trang
            if (configMenuPage > 0)
            {
                Hardware::lcd.setCursor(18, 1);
                Hardware::lcd.print("^"); // Mũi tên lên
            }
            if ((configMenuPage + 1) * 3 < 10)
            {
                Hardware::lcd.setCursor(18, 3);
                Hardware::lcd.print("v"); // Mũi tên xuống
            }
            break;

        case MODE_DELAY:
            // Hiển thị chế độ cài đặt thời gian delay giữa các van
            Hardware::lcd.setCursor(0, 0);
            Hardware::lcd.print("Mode 1: DELAY VAN");
            Hardware::lcd.setCursor(0, 1);
            Hardware::lcd.print("> DELAY: ");
            Hardware::lcd.print(delay);
            Hardware::lcd.print(" giay"); // Hiển thị thời gian delay (giây)
            break;

        case MODE_POWERCHECK:
            // Hiển thị chế độ kiểm tra nguồn điện
            Hardware::lcd.setCursor(0, 0);
            Hardware::lcd.print("Mode 2: POWER CHECK");
            Hardware::lcd.setCursor(0, 1);
            Hardware::lcd.print("> POWER CHECK: ");
            Hardware::lcd.print(powerCheck ? "ON" : "OFF"); // Hiển thị trạng thái kiểm tra nguồn
            break;
        }

        // Cập nhật trạng thái giao diện
        currentState = STATE_CONFIG;
    }

    // Vẽ màn hình chỉnh giờ/phút thủ công
    void drawManualMenu(uint8_t hour, uint8_t minute, bool editingHour)
    {
        Hardware::lcd.clear();
        Hardware::lcd.setCursor(0, 0);
        Hardware::lcd.print("THU CONG");

        Hardware::lcd.setCursor(0, 1);
        Hardware::lcd.print("Gio: ");
        if (editingHour)
            Hardware::lcd.print("[");

        if (hour < 10) Hardware::lcd.print("0");
        Hardware::lcd.print(hour);

        if (editingHour)
            Hardware::lcd.print("]");
        else
            Hardware::lcd.print(" ");

        Hardware::lcd.print("  Phut: ");
        if (!editingHour)
            Hardware::lcd.print("[");

        if (minute < 10) Hardware::lcd.print("0");
        Hardware::lcd.print(minute);

        if (!editingHour)
            Hardware::lcd.print("]");
        else
            Hardware::lcd.print(" ");

        Hardware::lcd.setCursor(0, 3);
        Hardware::lcd.print("#: Luu   *: Thoat");

        currentState = STATE_MANUAL;
    }
}