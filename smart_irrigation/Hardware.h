#pragma once
#include <LiquidCrystal_PCF8574.h>
#include <stdint.h>

namespace Hardware
{
  // Đối tượng LCD toàn cục, dùng để hiển thị thông tin lên màn hình LCD I2C
  extern LiquidCrystal_PCF8574 lcd;

  // Hàm khởi tạo phần cứng: relay, LCD, cảm biến, v.v.
  void begin();

  // Đọc dòng điện từ cảm biến ACS712 (trả về đơn vị Ampe)
  float readCurrent();

  // Bật/tắt van tưới theo chỉ số (0-9)
  void setValve(uint8_t idx, bool on);

  // Bật/tắt bơm theo chỉ số (0-1)
  void setPump(uint8_t idx, bool on);

  // Tắt toàn bộ van và bơm (an toàn khi khởi động hoặc dừng hệ thống)
  void turnAllOff();
}
