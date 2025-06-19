#pragma once

namespace TimeBlynk
{
  // Hàm khởi tạo toàn bộ hệ thống mạng, Blynk, đồng bộ thời gian
  void begin();

  // Hàm update: dự phòng cho xử lý định kỳ nếu cần (có thể để trống nếu không dùng)
  void update();

  // Hàm duy trì kết nối và xử lý sự kiện Blynk, nên gọi thường xuyên trong loop()
  void runBlynk();
}
