#pragma once
#include <stdint.h>

// Enum định nghĩa các trạng thái giao diện chính của hệ thống
enum AppState
{
  STATE_MAIN,    // Màn hình chính (menu)
  STATE_CONFIG,  // Màn hình cấu hình
  STATE_MANUAL,  // Màn hình thủ công
};

namespace Display
{
  extern AppState currentState; // Biến lưu trạng thái giao diện hiện tại
  extern bool blinkState;       // Biến dùng cho hiệu ứng nhấp nháy (nếu có)

  extern uint8_t manualHour;
  extern uint8_t manualMinute;
  extern bool editingManualHour;

  // Hàm vẽ màn hình menu chính
  // mainIndex: chỉ số mục đang chọn trong menu chính
  // autoOn, startValve: các tham số liên quan chế độ tự động (có thể bỏ qua nếu không dùng)
  void drawMainMenu(uint8_t mainIndex, bool autoOn, uint8_t startValve);

  // Hàm vẽ màn hình cấu hình
  // configMode: chế độ cấu hình (bật/tắt van, delay, kiểm tra nguồn)
  // page: trang hiện tại trong menu cấu hình (mỗi trang tối đa 3 van)
  // index: vị trí chọn hiện tại trong trang
  // powerCheckEnabled: trạng thái kiểm tra nguồn điện
  // delaySec: thời gian delay giữa các van (giây)
  // valveStates: trạng thái hiện tại của các van (để hiển thị trên giao diện)
  void drawConfigMenu(uint8_t configMode, uint8_t page, uint8_t index, bool powerCheckEnabled, uint16_t delaySec, const bool* valveStates = nullptr);

  // Hàm vẽ màn hình thủ công
  // hour: giờ hiện tại (0-23)
  // minute: phút hiện tại (0-59)
  // editingHour: true nếu đang chỉnh giờ, false nếu đang chỉnh phút
  // Hiển thị giao diện chỉnh giờ/phút cho chế độ thủ công
  void drawManualMenu(uint8_t hour, uint8_t minute, bool editingHour);
}
