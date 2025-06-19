#pragma once
#include <stdint.h>

namespace Config
{
    // ===========================
    // Hàm khởi tạo và lưu cấu hình vào bộ nhớ flash
    // ===========================
    void load();   // Đọc cấu hình từ flash khi khởi động
    void save();   // Lưu cấu hình hiện tại vào flash

    // ===========================
    // Trạng thái van tưới (10 van)
    // ===========================
    // Lấy trạng thái của van tại vị trí index (true: bật, false: tắt)
    bool getValveState(uint8_t index);
    // Đặt trạng thái cho van tại vị trí index
    void setValveState(uint8_t index, bool state);

    // ===========================
    // Trạng thái bơm (2 bơm)
    // ===========================
    // Lấy trạng thái của bơm tại vị trí index (true: bật, false: tắt)
    bool getPumpState(uint8_t index);
    // Đặt trạng thái cho bơm tại vị trí index
    void setPumpState(uint8_t index, bool state);

    // ===========================
    // Thời gian delay giữa các van (giây)
    // ===========================
    // Lấy giá trị delay hiện tại (giây)
    uint16_t getDelaySec();
    // Đặt giá trị delay mới (giây)
    void setDelaySec(uint16_t seconds);

    // ===========================
    // Kiểm tra nguồn điện
    // ===========================
    // Lấy trạng thái kiểm tra nguồn điện (true: bật, false: tắt)
    bool getPowerCheckEnabled();
    // Đặt trạng thái kiểm tra nguồn điện
    void setPowerCheckEnabled(bool enabled);

    // ===========================
    // Thời gian thủ công (giờ, phút)
    // ===========================
    uint8_t getManualHour();
    void setManualHour(uint8_t hour);
    uint8_t getManualMinute();
    void setManualMinute(uint8_t minute);
}
