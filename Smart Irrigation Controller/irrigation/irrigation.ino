#define BLYNK_TEMPLATE_ID "TMPL6xy4mK0KN"
#define BLYNK_TEMPLATE_NAME "Smart Irrigation Controller"
#define BLYNK_AUTH_TOKEN "J34tRXWg_QTXt8bs0SfbZFSc1ARUpL0o"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <IRrecv.h>
#include <IRutils.h>

// Khởi tạo OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Định nghĩa chân relay (có thể thay đổi theo sơ đồ của bạn)
const int relayPins[12] = { 2, 4, 5, 16, 17, 18, 19, 21, 22, 23, 25, 26 };
// relay 1->10: béc tưới; relay 11: bơm 1; relay 12: bơm 2.

// Các biến điều khiển chu trình
unsigned long interval = 9000000; // Mặc định 2 giờ 30 phút = 9000000 ms
unsigned long previousMillis = 0;
int currentRelayIndex = -1; // Chưa có relay nào bật

// Mảng cài đặt cho phép relay (relay 1 đến 10)
bool allowedRelays[10] = { true, true, true, true, true, true, true, true, true, true };

// CREDENTIALS BLYNK & WIFI
char auth[] = "J34tRXWg_QTXt8bs0SfbZFSc1ARUpL0o";
char ssid[] = "VU ne";
char pass[] = "12341234";

// -------------------------
// Cài đặt IR Remote X1838
// -------------------------
const uint16_t kRecvPin = 15; // Chân nhận IR, chọn chân phù hợp với phần cứng của bạn
IRrecv irrecv(kRecvPin);
decode_results results;

// Biến để chuyển đổi giữa chế độ cài đặt thời gian và cài đặt relay
enum RemoteMode { MODE_NONE, MODE_SET_TIME, MODE_SET_RELAY };
RemoteMode remoteMode = MODE_NONE;

// Biến cài đặt thời gian tạm (tính bằng milli giây)
unsigned long tempInterval = interval;

// Các hàm hỗ trợ điều khiển relay
void activateRelay(int index) {
  digitalWrite(relayPins[index], HIGH);
}
void deactivateRelay(int index) {
  digitalWrite(relayPins[index], LOW);
}

// Hàm cập nhật trạng thái bơm dựa theo relay béc tưới hiện hành
void updatePumps(int sprinklerIndex) {
  if(sprinklerIndex >= 0 && sprinklerIndex <= 5) {
    activateRelay(10);    // relay 11: bơm 1
    deactivateRelay(11);  // relay 12: bơm 2
  }
  else if(sprinklerIndex >= 6 && sprinklerIndex <= 9) {
    activateRelay(10);    // bơm 1
    activateRelay(11);    // bơm 2
  }
  else {
    deactivateRelay(10);
    deactivateRelay(11);
  }
}

// -------------------------
// Hàm xử lý IR Remote
// -------------------------
void processIR() {
  if (irrecv.decode(&results)) {
    // Giả sử remote gửi các mã số hex khác nhau cho các phím
    uint32_t code = results.value;

    // Ví dụ:
    // - Phím MODE: chuyển đổi giữa chế độ cài đặt thời gian và cài đặt relay
    // - Phím UP: tăng thời gian nếu đang ở chế độ cài đặt thời gian, hoặc bật relay nếu ở chế độ cài đặt relay
    // - Phím DOWN: giảm thời gian hoặc tắt relay.
    // - Phím OK: lưu lại cài đặt

    // Giả sử mã giả định:
    // MODE: 0xFFA25D, UP: 0xFF629D, DOWN: 0xFFE21D, OK: 0xFF22DD
    // Và các phím số 1 đến 10 tương ứng: 0xFF02FD (1), 0xFF827D (2), ... (các mã này cần được xác định chính xác theo remote của bạn)

    switch(code) {
      case 0xFFA25D:  // Phím MODE
        if(remoteMode == MODE_NONE) {
          remoteMode = MODE_SET_TIME;
        } else if(remoteMode == MODE_SET_TIME) {
          remoteMode = MODE_SET_RELAY;
        } else {
          remoteMode = MODE_NONE;
        }
        break;

      case 0xFF629D:  // Phím UP
        if(remoteMode == MODE_SET_TIME) {
          tempInterval += 60000; // tăng 1 phút (60,000 ms)
        }
        // Nếu ở chế độ cài đặt relay, bạn có thể xử lý bật relay tương ứng (ví dụ phím UP tăng số thứ tự relay được chọn)
        break;

      case 0xFFE21D:  // Phím DOWN
        if(remoteMode == MODE_SET_TIME && tempInterval >= 60000) {
          tempInterval -= 60000; // giảm 1 phút
        }
        break;

      case 0xFF22DD:  // Phím OK
        if(remoteMode == MODE_SET_TIME) {
          interval = tempInterval;  // Lưu lại thời gian đã cài
        }
        // Nếu ở chế độ cài đặt relay, lưu trạng thái cài đặt của relay
        remoteMode = MODE_NONE; // Quay lại chế độ bình thường
        break;

      // THÊM CÁC PHÍM SỐ ĐỂ cài đặt relay (ví dụ: nếu nhận được mã phím số tương ứng, chuyển đổi trạng thái bật/tắt của relay đó)
      // Ví dụ: nếu remote gửi mã của phím số 1, chuyển đổi allowedRelays[0]
      case 0xFF02FD:  // Phím số 1
        allowedRelays[0] = !allowedRelays[0];
        break;
      case 0xFF827D:  // Phím số 2
        allowedRelays[1] = !allowedRelays[1];
        break;
      // Tương tự với các phím số 3 -> 10
      // ...

      default:
        break;
    }
    irrecv.resume(); // Chuẩn bị nhận tín hiệu tiếp theo
  }
}

BLYNK_WRITE(V1) {
  // Nhận giá trị thời gian từ Blynk (đơn vị phút) và chuyển đổi thành mili giây
  int minutes = param.asInt();
  interval = minutes * 60000UL;
  tempInterval = interval;
}

BLYNK_WRITE(V2) {
  // Nhận dữ liệu dạng chuỗi, ví dụ "1,3,5,7" và chuyển đổi thành mảng bool cho allowedRelays
  String data = param.asString();
  // Reset mảng cài đặt relay (mặc định false)
  for (int i = 0; i < 10; i++) {
    allowedRelays[i] = false;
  }
  // Tách chuỗi theo dấu phẩy
  int startIndex = 0;
  while (true) {
    int commaIndex = data.indexOf(',', startIndex);
    String token;
    if (commaIndex != -1) {
      token = data.substring(startIndex, commaIndex);
      startIndex = commaIndex + 1;
    } else {
      token = data.substring(startIndex);
    }
    int relayNum = token.toInt();
    if(relayNum >= 1 && relayNum <= 10) {
      allowedRelays[relayNum - 1] = true;
    }
    if (commaIndex == -1) break;
  }
}

void setup() {
  Serial.begin(115200);

  // Khởi tạo các chân relay
  for (int i = 0; i < 12; i++) {
    pinMode(relayPins[i], OUTPUT);
    deactivateRelay(i);
  }

  // Khởi tạo OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED SSD1306 khong khoi tao duoc");
    while(true);
  }
  display.clearDisplay();
  display.display();

  // Khởi tạo IR Remote
  irrecv.enableIRIn();

  // Khởi tạo kết nối Blynk và WiFi
  Blynk.begin(auth, ssid, pass);
}

void loop() {
  Blynk.run();
  processIR();  // Xử lý tín hiệu remote từ X1838

  unsigned long currentMillis = millis();
  if(currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    // Tìm relay kế tiếp trong danh sách cho phép
    int nextRelay = -1;
    int count = 0;
    do {
      currentRelayIndex = (currentRelayIndex + 1) % 10; // duyệt vòng qua relay 1->10
      if(allowedRelays[currentRelayIndex]) {
        nextRelay = currentRelayIndex;
        break;
      }
      count++;
    } while(count < 10);

    // Nếu tìm thấy relay được phép
    if(nextRelay != -1) {
      // Tắt hết relay béc tưới
      for (int i = 0; i < 10; i++) {
        deactivateRelay(i);
      }
      // Bật relay kế tiếp
      activateRelay(nextRelay);
      updatePumps(nextRelay);

      // Cập nhật hiển thị OLED
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(0, 0);
      display.print("Relay hien hanh: ");
      display.println(nextRelay + 1);
      display.print("Thoi gian: ");
      display.println(interval / 60000);
      display.display();
    }
    else {
      // Nếu không có relay nào được bật, tắt tất cả (bảo vệ hệ thống)
      for (int i = 0; i < 12; i++) {
        deactivateRelay(i);
      }
    }
  }

  // Các chức năng khác: đọc cảm biến ACS712, xử lý giao diện Blynk nâng cao, ...
}
