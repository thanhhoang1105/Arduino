#include "Irrigation.h"
#include "Config.h"
#include "Hardware.h"
#include "Display.h"
#include <Arduino.h>

namespace Irrigation
{
    // ===========================
    // Hàm update(): Không còn xử lý gì vì chỉ giữ lại màn hình chính và cấu hình
    // ===========================
    void update()
    {
        // Không còn logic tưới tự động/thủ công ở đây.
        // Nếu muốn có thể thêm các tác vụ kiểm tra trạng thái hệ thống hoặc cập nhật hiển thị.
    }
}