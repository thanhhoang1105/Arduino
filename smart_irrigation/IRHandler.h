#pragma once
#include <stdint.h>

namespace IRHandler
{
  // Hàm khởi tạo bộ thu IR, truyền vào số chân kết nối IR receiver
  void begin(uint8_t pin);

  // Hàm xử lý tín hiệu IR, gọi liên tục trong loop() để nhận và xử lý phím bấm từ remote
  void process();
}
