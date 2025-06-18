#pragma once

namespace TimeBlynk
{
  void begin();
  void update();   // nếu cần xử lý thêm
  void runBlynk(); // Thêm hàm này để gọi Blynk.run()
}
