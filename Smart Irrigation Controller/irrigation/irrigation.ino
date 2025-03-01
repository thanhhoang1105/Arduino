#include <gpio_viewer.h>  // Phải include thư viện này đầu tiên
GPIOViewer gpio_viewer;

#define BLYNK_TEMPLATE_ID "TMPL6xy4mK0KN"
#define BLYNK_TEMPLATE_NAME "Smart Irrigation Controller"
#define BLYNK_AUTH_TOKEN "J34tRXWg_QTXt8bs0SfbZFSc1ARUpL0o"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <IRremote.hpp>

// -------------------------
// OLED & WiFi & Relay Setup
// -------------------------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

char ssid[] = "VU ne";
char pass[] = "12341234";

// Relay pins (12 relay: 10 sprinklers, relay[10]=pump1, relay[11]=pump2)
const int relayPins[12] = { 2, 4, 16, 17, 5, 18, 19, 21, 22, 23, 26, 25 };

// -------------------------
// IR Remote Setup (sử dụng IRremote.hpp)
// -------------------------
const int RECV_PIN = 15;  // chân nhận IR
// Không cần đối tượng gửi nếu chỉ nhận
// Khởi tạo IR receiver:
 
// Các mã ví dụ (theo remote của bạn – cần hiệu chỉnh nếu cần)
#define IR_KEY_1     0xFF30CF
#define IR_KEY_2     0xFF18E7
#define IR_KEY_3     0xFF7A85
#define IR_KEY_4     0xFF10EF
#define IR_KEY_5     0xFF38C7
#define IR_KEY_6     0xFF5AA5
#define IR_KEY_7     0xFF42BD
#define IR_KEY_8     0xFF4AB5
#define IR_KEY_9     0xFF52AD
#define IR_KEY_STAR  0xFF6897   // Dùng làm "BACK" (hoặc LEFT)
#define IR_KEY_HASH  0xFF9867   // có thể dùng khác
#define IR_KEY_OK    0xFF02FD
#define IR_KEY_UP    0xFF629D
#define IR_KEY_DOWN  0xFFE21D
#define IR_KEY_RIGHT 0xFF22DD
#define IR_KEY_LEFT  0xFF906F

// -------------------------
// Menu state định nghĩa
// -------------------------
enum MenuState { MAIN_MENU, CONFIG_MENU, TIME_SELECT_MENU, RUNNING };
MenuState currentMenu = MAIN_MENU;

// Biến cho menu MAIN_MENU
// 0: CAU HINH, 1: KHOI DONG
int mainMenuSelection = 0;  

// Cho CONFIG_MENU: cấu hình 10 béc tưới (index 0->9)
// true: béc được phép, false: không
bool allowedSprinklers[10];
int configSelected = 0;  // béc hiện đang được chọn trong config

// Cho TIME_SELECT_MENU:
unsigned int irrigationTime = 120; // thời gian tưới cho mỗi béc (phút), mặc định 120 = 2 giờ

// Cho RUNNING:
unsigned long runStartTime = 0;    // thời gian bắt đầu của béc hiện tại
int runSprinklerIndices[10];       // danh sách các béc (index) được bật (theo cấu hình)
int runSprinklerCount = 0;
int currentRunIndex = 0;
bool cycleStarted = false;

// -------------------------
// Các hàm hiển thị menu trên OLED
// -------------------------
void updateMenuDisplay() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  switch (currentMenu) {
    case MAIN_MENU: {
      // Hiển thị menu chính: 2 dòng với lựa chọn CAU HINH và KHOI DONG
      display.setTextSize(2);
      if (mainMenuSelection == 0) {
        display.setCursor(0, 0);
        display.println("> CAU HINH");
        display.setCursor(0, 32);
        display.println("  KHOI DONG");
      } else {
        display.setCursor(0, 0);
        display.println("  CAU HINH");
        display.setCursor(0, 32);
        display.println("> KHOI DONG");
      }
      break;
    }
    case CONFIG_MENU: {
      // Hiển thị danh sách cấu hình béc
      display.setTextSize(1);
      display.setCursor(0, 0);
      display.println("CAU HINH:");
      // Hiển thị các béc từ 1 đến 10, đánh dấu béc hiện được chọn bằng dấu ">"
      for (int i = 0; i < 10; i++) {
        if (i == configSelected) display.print(">");
        else display.print(" ");
        display.print("Bec ");
        display.print(i + 1);
        display.print(": ");
        if (allowedSprinklers[i])
          display.println("ON");
        else
          display.println("OFF");
      }
      break;
    }
    case TIME_SELECT_MENU: {
      // Hiển thị chọn thời gian tưới (mặc định 02:00)
      display.setTextSize(2);
      display.setCursor(0, 0);
      display.println("CHON THOI GIAN");
      display.setTextSize(2);
      display.setCursor(0, 32);
      // Hiển thị dạng HH:MM
      unsigned int hours = irrigationTime / 60;
      unsigned int minutes = irrigationTime % 60;
      char timeStr[6];
      sprintf(timeStr, "%02u:%02u", hours, minutes);
      display.println(timeStr);
      break;
    }
    case RUNNING: {
      // Hiển thị trạng thái đang chạy: hiện béc hiện hành và thời gian còn lại của béc
      display.setTextSize(2);
      display.setCursor(0, 0);
      if (currentRunIndex < runSprinklerCount) {
        display.print("Bec ");
        display.print(runSprinklerIndices[currentRunIndex] + 1);
        display.println(" dang chay");
        // Tính thời gian còn lại (giả sử irrigationTime tính bằng phút)
        unsigned long elapsed = (millis() - runStartTime) / 1000; // giây
        unsigned long totalSec = irrigationTime * 60;
        unsigned long remain = (totalSec > elapsed) ? (totalSec - elapsed) : 0;
        unsigned int rh = remain / 3600;
        unsigned int rm = (remain % 3600) / 60;
        char timeStr[6];
        sprintf(timeStr, "%02u:%02u", rh, rm);
        display.setCursor(0, 32);
        display.print("Con lai: ");
        display.println(timeStr);
      } else {
        display.setTextSize(2);
        display.setCursor(0, 0);
        display.println("Xong chu trinh");
      }
      break;
    }
  }
  display.display();
}

// -------------------------
// Hàm xử lý IR Remote (sử dụng IRremote.hpp)
// -------------------------
void processIRRemote() {
  if (IrReceiver.decode()) {
    uint32_t code = IrReceiver.decodedIRData.decodedRawData;
    // Chỉ xử lý nếu code khác 0
    if (code != 0) {
      // Map các mã thành chuỗi lệnh:
      String cmd = "";
      switch(code) {
        case IR_KEY_UP:    cmd = "UP"; break;
        case IR_KEY_DOWN:  cmd = "DOWN"; break;
        case IR_KEY_LEFT:  cmd = "LEFT"; break;
        case IR_KEY_OK:    cmd = "OK"; break;
        // Bạn có thể mở rộng với các phím khác nếu cần
        default: 
          cmd = "UNKNOWN";
          break;
      }
      Serial.print("IR Command: ");
      Serial.println(cmd);
      // Xử lý lệnh theo trạng thái menu
      // -------- MAIN_MENU --------
      if (currentMenu == MAIN_MENU) {
        if (cmd == "UP" || cmd == "DOWN") {
          mainMenuSelection = 1 - mainMenuSelection; // toggle giữa 0 và 1
        } else if (cmd == "OK") {
          if (mainMenuSelection == 0) {
            currentMenu = CONFIG_MENU;
          } else {
            currentMenu = TIME_SELECT_MENU;
          }
        }
      }
      // -------- CONFIG_MENU --------
      else if (currentMenu == CONFIG_MENU) {
        if (cmd == "UP") {
          if (configSelected > 0) configSelected--;
        } else if (cmd == "DOWN") {
          if (configSelected < 9) configSelected++;
        } else if (cmd == "OK") {
          allowedSprinklers[configSelected] = !allowedSprinklers[configSelected];
        } else if (cmd == "LEFT") {
          currentMenu = MAIN_MENU;
        }
      }
      // -------- TIME_SELECT_MENU --------
      else if (currentMenu == TIME_SELECT_MENU) {
        if (cmd == "UP") {
          irrigationTime += 5;  // tăng 5 phút
        } else if (cmd == "DOWN") {
          if (irrigationTime > 5) irrigationTime -= 5;
        } else if (cmd == "OK") {
          // Khi xác nhận thời gian, chuyển sang RUNNING
          // Tạo danh sách các béc được bật theo thứ tự
          runSprinklerCount = 0;
          for (int i = 0; i < 10; i++) {
            if (allowedSprinklers[i]) {
              runSprinklerIndices[runSprinklerCount++] = i;
            }
          }
          if (runSprinklerCount == 0) {
            // Nếu không có béc nào được cấu hình, thông báo và quay lại MAIN_MENU
            Serial.println("Chua chon bec nao!");
            currentMenu = MAIN_MENU;
          } else {
            currentMenu = RUNNING;
            currentRunIndex = 0;
            runStartTime = millis();
            // Bật béc đầu tiên và bật pump theo logic:
            int bec = runSprinklerIndices[currentRunIndex];
            digitalWrite(relayPins[bec], HIGH);
            // Điều khiển pump: nếu bec < 6 (tức 1->6) bật pump1; nếu >= 6 (7->10) bật cả pump1 và pump2
            if (bec < 6) {
              digitalWrite(relayPins[10], HIGH);
              digitalWrite(relayPins[11], LOW);
            } else {
              digitalWrite(relayPins[10], HIGH);
              digitalWrite(relayPins[11], HIGH);
            }
          }
        } else if (cmd == "LEFT") {
          currentMenu = MAIN_MENU;
        }
      }
      // -------- RUNNING --------
      else if (currentMenu == RUNNING) {
        // Cho phép hủy chu trình nếu nhấn LEFT
        if (cmd == "LEFT") {
          // Tắt tất cả relay
          for (int i = 0; i < 12; i++) {
            digitalWrite(relayPins[i], LOW);
          }
          currentMenu = MAIN_MENU;
          cycleStarted = false;
        }
      }
    }
    IrReceiver.resume();
  }
}

// -------------------------
// Hàm cập nhật trạng thái RUNNING (chu trình tưới)
// -------------------------
void updateRunning() {
  if (currentMenu != RUNNING) return;
  // Tính thời gian của béc hiện hành (chuyển từ phút sang ms)
  unsigned long duration = (unsigned long)irrigationTime * 60000UL;
  unsigned long elapsed = millis() - runStartTime;
  if (elapsed >= duration) {
    // Tắt béc hiện hành và pump
    int currentBec = runSprinklerIndices[currentRunIndex];
    digitalWrite(relayPins[currentBec], LOW);
    digitalWrite(relayPins[10], LOW);
    digitalWrite(relayPins[11], LOW);
    // Chuyển sang béc kế tiếp
    currentRunIndex++;
    if (currentRunIndex >= runSprinklerCount) {
      // Nếu đã chạy hết chu trình, dừng và quay lại MAIN_MENU
      currentMenu = MAIN_MENU;
    } else {
      // Bật béc kế tiếp
      int nextBec = runSprinklerIndices[currentRunIndex];
      digitalWrite(relayPins[nextBec], HIGH);
      // Bật pump theo logic
      if (nextBec < 6) {
        digitalWrite(relayPins[10], HIGH);
        digitalWrite(relayPins[11], LOW);
      } else {
        digitalWrite(relayPins[10], HIGH);
        digitalWrite(relayPins[11], HIGH);
      }
      runStartTime = millis();
    }
  }
}

// -------------------------
// Hàm cập nhật trạng thái kết nối Blynk (như mẫu ban đầu)
// -------------------------
void updateConnectionStatus() {
  if (Blynk.connected()) {
    static bool ledStatus = false;
    ledStatus = !ledStatus;
    Blynk.virtualWrite(V0, ledStatus ? 1 : 0);
  } else {
    Blynk.virtualWrite(V0, 0);
  }
}

// -------------------------
// setup()
// -------------------------
void setup() {
  Serial.begin(115200);

  // Cài đặt các chân relay làm OUTPUT, khởi tạo trạng thái tắt (LOW)
  for (int i = 0; i < 12; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], LOW);
  }
  
  // Khởi tạo giá trị mặc định cho cấu hình: ban đầu tắt hết
  for (int i = 0; i < 10; i++) {
    allowedSprinklers[i] = false;
  }
  
  // Khởi tạo I2C cho OLED (SDA=32, SCL=33)
  Wire.begin(32, 33);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED SSD1306 khong khoi tao duoc");
    while(true);
  }
  display.clearDisplay();
  display.display();
  
  // Kết nối đến Blynk
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  Serial.println("ESP32 connected to Blynk!");
  
  // Thiết lập GPIO Viewer
  gpio_viewer.connectToWifi(ssid, pass);
  gpio_viewer.begin();
  
  // Khởi động IR Receiver
  IrReceiver.begin(RECV_PIN, ENABLE_LED_FEEDBACK);
  Serial.println("IR Receiver dang cho tin hieu...");
}

// -------------------------
// loop()
// -------------------------
void loop() {
  Blynk.run();
  processIRRemote();  // Xử lý tín hiệu IR qua remote
  
  // Nếu chu trình RUNNING, cập nhật chuyển đổi béc tưới
  if (currentMenu == RUNNING) {
    updateRunning();
  }
  
  // Cập nhật màn hình OLED menu theo trạng thái hiện tại
  updateMenuDisplay();
  
  delay(100);  // Delay nhỏ để giảm tần số cập nhật (có thể điều chỉnh)
}
