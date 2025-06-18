# HỆ THỐNG TƯỚI THÔNG MINH

## Giới thiệu

Hệ thống tưới thông minh là giải pháp tự động hóa quá trình tưới vườn, sử dụng ESP32 để điều khiển nhiều van tưới khác nhau. Hệ thống hỗ trợ cả chế độ tưới thủ công và tự động theo lịch trình, có thể điều khiển từ xa thông qua ứng dụng Blynk hoặc điều khiển trực tiếp bằng remote hồng ngoại.

## Tính năng

-   **Điều khiển đa van**: Hỗ trợ điều khiển lên đến 10 van tưới và 2 bơm
-   **Giao diện LCD**: Hiển thị trạng thái và menu cài đặt trên màn hình LCD 20x4
-   **Điều khiển từ xa**: Sử dụng remote hồng ngoại để điều chỉnh cài đặt
-   **Kết nối IoT**: Giám sát và điều khiển qua ứng dụng Blynk
-   **Chế độ tưới thủ công**: Cài đặt thời gian tưới theo nhu cầu
-   **Chế độ tưới tự động**: Lập lịch tưới tự động theo khung giờ
-   **Kiểm tra nguồn điện**: Tự động tắt hệ thống khi phát hiện mất nguồn
-   **Lưu cài đặt**: Lưu trữ cấu hình ngay cả khi mất điện

## Yêu cầu phần cứng

-   ESP32 Development Board
-   Màn hình LCD 20x4 với module I2C (PCF8574)
-   Module thu hồng ngoại (IR Receiver)
-   Relay module (12 kênh: 10 cho van, 2 cho bơm)
-   Cảm biến dòng điện ACS712 (tùy chọn)
-   Nguồn cấp 5V cho ESP32
-   Nguồn cấp cho các van tưới và bơm

## Sơ đồ kết nối

-   **LCD (I2C)**:

    -   SDA -> GPIO13
    -   SCL -> GPIO14
    -   VCC -> 5V
    -   GND -> GND

-   **IR Receiver**:

    -   SIGNAL -> GPIO15
    -   VCC -> 3.3V
    -   GND -> GND

-   **Relay Van**:

    -   IN1-IN10 -> GPIO2, 4, 16, 17, 5, 18, 19, 21, 22, 23
    -   VCC -> 5V
    -   GND -> GND

-   **Relay Bơm**:

    -   IN1-IN2 -> GPIO26, 25
    -   VCC -> 5V
    -   GND -> GND

-   **ACS712**:
    -   OUT -> GPIO34 (ADC)
    -   VCC -> 5V
    -   GND -> GND

## Thư viện cần thiết

-   BlynkSimpleEsp32
-   WiFiManager
-   IRremote
-   LiquidCrystal_PCF8574
-   Preferences
-   Wire
-   Arduino

## Cài đặt

1. Cài đặt Arduino IDE
2. Thêm hỗ trợ ESP32 vào Arduino IDE
3. Cài đặt tất cả thư viện cần thiết
4. Clone hoặc tải xuống mã nguồn từ repository
5. Sửa thông tin Blynk trong file `BlynkCredentials.h` với template ID, template name và auth token của bạn
6. Nạp code lên ESP32

## Hướng dẫn sử dụng

### Khởi động ban đầu

Khi khởi động, hệ thống sẽ:

1. Khởi tạo phần cứng
2. Tải cấu hình đã lưu
3. Kết nối WiFi (sử dụng WiFiManager)
4. Kết nối đến Blynk Cloud
5. Đồng bộ thời gian từ máy chủ NTP
6. Hiển thị menu chính

### Điều hướng menu

Sử dụng các phím trên remote IR:

-   **Mũi tên lên/xuống**: Di chuyển giữa các tùy chọn
-   **Mũi tên trái/phải**: Chuyển đổi giữa các menu con
-   **OK**: Chọn tùy chọn
-   **#**: Lưu/Xác nhận
-   **\***: Quay lại/Hủy

### Chế độ Menu

1. **MENU CHÍNH**:

    - CAU HINH: Cài đặt van và các tùy chọn hệ thống
    - THU CONG: Cài đặt tưới thủ công
    - TU DONG: Cài đặt tưới tự động

2. **CHẾ ĐỘ CẤU HÌNH**:

    - ON/OFF VAN: Bật/tắt từng van
    - DELAY VAN: Cài đặt thời gian trễ giữa các van
    - POWER CHECK: Bật/tắt tính năng kiểm tra nguồn

3. **CHẾ ĐỘ THỦ CÔNG**:

    - Cài đặt giờ và phút để bắt đầu tưới
    - Bấm OK khi đang chỉnh phút hoặc bấm # để khởi chạy ngay

4. **CHẾ ĐỘ TỰ ĐỘNG**:
    - BAT/TAT TU DONG: Bật/tắt chế độ tự động
    - CHON VAN BAT DAU: Chọn van bắt đầu chế độ tự động
    - CAI DAT THOI GIAN: Cài đặt khung giờ hoạt động, thời gian tưới và thời gian nghỉ

### Chi tiết về chế độ thủ công

-   **Điều chỉnh giờ**: Bấm lên/xuống để tăng/giảm 1 đơn vị, hoặc nhập trực tiếp từ 0-23
-   **Điều chỉnh phút**: Bấm lên/xuống để tăng/giảm 15 đơn vị (0, 15, 30, 45), hoặc nhập trực tiếp từ 0-59
-   **Màn hình chạy thủ công**:
    -   Hiển thị "THU CONG (RUN)"
    -   Hiển thị béc đang mở và thời gian còn lại
    -   Hiển thị béc tiếp theo và thời gian delay trước khi mở

### Lưu ý về chuyển đổi giữa các béc

Khi một béc hoạt động hết thời gian, hệ thống sẽ:

1. Bắt đầu mở béc tiếp theo trước khi béc hiện tại kết thúc (thời gian delay)
2. Đóng béc hiện tại sau khi hết thời gian
3. Tiếp tục với béc tiếp theo

## Cấu trúc thư mục

-   `smart_irrigation.ino`: File chính của dự án
-   `BlynkCredentials.h`: Chứa thông tin xác thực Blynk
-   `Config.h/.cpp`: Quản lý cấu hình và lưu trữ
-   `Display.h/.cpp`: Xử lý hiển thị LCD
-   `Hardware.h/.cpp`: Điều khiển phần cứng (relay, LCD, cảm biến)
-   `IRHandler.h/.cpp`: Xử lý điều khiển từ xa IR
-   `Irrigation.h/.cpp`: Logic tưới thủ công và tự động
-   `TimeBlynk.h/.cpp`: Kết nối Blynk và đồng bộ thời gian
-   `Utils.h/.cpp`: Công cụ tiện ích và các tác vụ định kỳ

## Xử lý sự cố

-   **Màn hình không hiển thị**: Kiểm tra kết nối I2C và địa chỉ LCD
-   **Remote không phản hồi**: Kiểm tra pin và kết nối module IR
-   **Không kết nối được WiFi**: Sử dụng portal WiFiManager để cấu hình lại
-   **Không kết nối được Blynk**: Kiểm tra thông tin xác thực trong `BlynkCredentials.h`
-   **Thời gian không chính xác**: Đảm bảo có kết nối internet để đồng bộ NTP

## Phát triển và đóng góp

Dự án này được phát triển để phục vụ nhu cầu tưới tự động. Nếu bạn muốn đóng góp hoặc yêu cầu tính năng mới, vui lòng liên hệ hoặc tạo pull request.

---

© 2025 - Hệ Thống Tưới
