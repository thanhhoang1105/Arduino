#include "BlynkCredentials.h"
#include <Arduino.h>
#include "Hardware.h"
#include "Config.h"
#include "TimeBlynk.h"
#include "Display.h"
#include "IRHandler.h"
#include "Irrigation.h"
#include "Utils.h"

// ===========================
// Hàm setup(): Khởi tạo hệ thống
// ===========================
void setup()
{
    Serial.begin(115200);         // Khởi động Serial để debug
    delay(100);                   // Đợi ổn định nguồn

    Hardware::begin();            // Khởi tạo phần cứng: relay, LCD, cảm biến...
    Config::load();               // Đọc cấu hình từ bộ nhớ flash
    TimeBlynk::begin();           // Kết nối WiFi, Blynk, đồng bộ thời gian
    Display::drawMainMenu(0, false, 1); // Hiển thị menu chính (chỉ còn mục cấu hình)
    IRHandler::begin(15);         // Khởi tạo bộ thu IR trên chân số 15
}

// ===========================
// Hàm loop(): Vòng lặp chính của chương trình
// ===========================
void loop()
{
    TimeBlynk::runBlynk();        // Duy trì kết nối và xử lý sự kiện Blynk
    IRHandler::process();         // Xử lý tín hiệu từ remote IR
    Irrigation::update();         // (Hiện tại không xử lý gì, dự phòng mở rộng)
    Utils::periodicTasks();       // Thực hiện các tác vụ định kỳ (ví dụ: kiểm tra nguồn)
}