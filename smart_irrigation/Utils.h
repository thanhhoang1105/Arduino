#pragma once
#include <stdint.h>

namespace Utils {
  void periodicTasks();

  // Struct dùng chung cho nhập số IR với timeout
  struct IRNumberInput {
      uint8_t value = 0;
      uint8_t maxValue = 59;
      uint8_t inputCount = 0;
      unsigned long lastInputMillis = 0;
      static const unsigned long INPUT_TIMEOUT = 1000; // 1 giây

      void reset() {
          inputCount = 0;
          lastInputMillis = 0;
      }

      // Xử lý nhập số, trả về true nếu có thay đổi giá trị
      bool processNumKey(int numKey, unsigned long now) {
          if (inputCount == 0 || (now - lastInputMillis > INPUT_TIMEOUT)) {
              value = numKey;
              inputCount = 1;
          } else {
              value = value * 10 + numKey;
              if (value > maxValue) value = maxValue;
              inputCount = 0;
          }
          lastInputMillis = now;
          return true;
      }

      // Xử lý up cho phút với quy tắc đặc biệt
      bool processMinuteUp() {
          if (value > 45) {
              value = 0;
          } else {
              uint8_t next = value + 15;
              value = (next > maxValue) ? maxValue : next;
          }
          return true;
      }
      // Xử lý down cho phút với quy tắc đặc biệt
      bool processMinuteDown() {
          if (value < 15) {
              value = 0;
          } else {
              value -= 15;
          }
          return true;
      }

      // Xử lý up/down cho giờ (hoặc giá trị thông thường)
      bool processUp() {
          if (value < maxValue) value++;
          return true;
      }
      bool processDown() {
          if (value > 0) value--;
          return true;
      }
  };
}
