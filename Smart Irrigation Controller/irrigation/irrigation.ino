#include <gpio_viewer.h> // Phải include thư viện này đầu tiên
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
#define OLED_RESET -1
// OLED sử dụng I2C với SDA = 32, SCL = 33
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

char ssid[] = "VU ne";
char pass[] = "12341234";

// Relay pins: relay 1->10: béc tưới; relay 11: bơm 1; relay 12: bơm 2.
const int relayPins[12] = {2, 4, 16, 17, 5, 18, 19, 21, 22, 23, 26, 25};

// -------------------------
// IR Remote Setup (sử dụng IRremote.hpp)
// -------------------------
const int RECV_PIN = 15; // Chân nhận IR
void setupIR()
{
  IrReceiver.begin(RECV_PIN, ENABLE_LED_FEEDBACK);
  Serial.println("IR Receiver dang cho tin hieu...");
}
// Các mã IR theo giá trị bạn cung cấp:
#define IR_CODE_1 3125149440UL
#define IR_CODE_2 3108437760UL
#define IR_CODE_3 3091726080UL
#define IR_CODE_4 3141861120UL
#define IR_CODE_5 3208707840UL
#define IR_CODE_6 3158572800UL
#define IR_CODE_7 4161273600UL
#define IR_CODE_8 3927310080UL
#define IR_CODE_9 4127850240UL
#define IR_CODE_0 3860463360UL
#define IR_CODE_STAR 3910598400UL // dùng làm BACK/CANCEL
#define IR_CODE_HASH 4061003520UL // dùng làm SAVE (lưu cấu hình)
#define IR_CODE_UP 3877175040UL
#define IR_CODE_DOWN 2907897600UL
#define IR_CODE_RIGHT 2774204160UL
#define IR_CODE_LEFT 4144561920UL
#define IR_CODE_OK 3810328320UL

// Biến debounce cho IR
unsigned long lastCode = 0;
unsigned long lastReceiveTime = 0;
const unsigned long debounceTime = 500; // ms

// -------------------------
// Menu state định nghĩa
// -------------------------
enum MenuState
{
  MAIN_MENU,
  CONFIG_MENU,
  TIME_SELECT_MENU,
  RUNNING
};
MenuState currentMenu = MAIN_MENU;

// MAIN_MENU: 0: CAU HINH, 1: KHOI DONG
int mainMenuSelection = 0;

// CONFIG_MENU: cấu hình 10 béc tưới (index 0->9)
// Khi vào CONFIG_MENU, backup cấu hình hiện tại.
bool allowedSprinklers[10];
bool backupConfig[10];  // lưu cấu hình trước khi thay đổi
int configSelected = 0; // béc hiện được chọn

// TIME_SELECT_MENU:
unsigned int irrigationTime = 1; // thời gian tưới mỗi béc (phút), mặc định 120 = 2 giờ

// define time tăng giảm
#define TIME_STEP 1

// RUNNING:
unsigned long runStartTime = 0;
int runSprinklerIndices[10]; // danh sách béc được bật theo cấu hình
int runSprinklerCount = 0;
int currentRunIndex = 0;
bool cycleStarted = false;

// -------------------------
// Hàm hiển thị menu trên OLED (text size = 1, line spacing = 16)
// -------------------------
void updateMenuDisplay()
{
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  int y = 0;
  switch (currentMenu)
  {
  case MAIN_MENU:
  {
    if (mainMenuSelection == 0)
    {
      display.setCursor(0, y);
      display.println("> CAU HINH");
      y += 16;
      display.setCursor(0, y);
      display.println("  KHOI DONG");
    }
    else
    {
      display.setCursor(0, y);
      display.println("  CAU HINH");
      y += 16;
      display.setCursor(0, y);
      display.println("> KHOI DONG");
    }
    break;
  }
  case CONFIG_MENU:
  {
    display.setCursor(0, y);
    display.println("CAU HINH:");
    y += 16;
    // Số dòng cấu hình hiển thị được (màn hình 64px - 16px header = 48px, mỗi dòng 16px → 3 dòng)
    int configScrollOffset = 0;
    const int maxVisible = 3; // Số dòng cấu hình hiển thị được
    // Cập nhật configScrollOffset sao cho configSelected luôn nằm trong vùng hiển thị
    if (configSelected < configScrollOffset)
      configScrollOffset = configSelected;
    if (configSelected > configScrollOffset + maxVisible - 1)
      configScrollOffset = configSelected - (maxVisible - 1);

    // In các mục cấu hình từ configScrollOffset đến configScrollOffset+maxVisible-1
    for (int i = configScrollOffset; i < 10 && i < configScrollOffset + maxVisible; i++)
    {
      display.setCursor(0, y);
      if (i == configSelected)
        display.print(">");
      else
        display.print(" ");
      display.print("Bec ");
      display.print(i + 1);
      display.print(": ");
      display.println(allowedSprinklers[i] ? "ON" : "OFF");
      y += 16;
    }
    // Hiển thị hướng dẫn ở cuối danh sách
    display.setCursor(0, y);
    display.println("*: Cancel  #: Save");
    break;
  }
  case TIME_SELECT_MENU:
  {
    display.setCursor(0, y);
    display.println("CHON THOI GIAN");
    y += 16;
    unsigned int hours = irrigationTime / 60;
    unsigned int minutes = irrigationTime % 60;
    char timeStr[6];
    sprintf(timeStr, "%02u:%02u", hours, minutes);
    display.setCursor(0, y);
    display.println(timeStr);
    break;
  }
  case RUNNING:
  {
    display.setCursor(0, y);
    if (currentRunIndex < runSprinklerCount)
    {
      display.print("Bec ");
      display.print(runSprinklerIndices[currentRunIndex] + 1);
      display.println(" dang chay");
      y += 16;
      unsigned long elapsed = (millis() - runStartTime) / 1000;
      unsigned long totalSec = irrigationTime * 60;
      unsigned long remain = (totalSec > elapsed) ? (totalSec - elapsed) : 0;
      unsigned int rh = remain / 3600;
      unsigned int rm = (remain % 3600) / 60;
      unsigned int rs = remain % 60;                  // Thêm giây
      char timeStr[9];                                // Tăng kích thước buffer để chứa thêm giây
      sprintf(timeStr, "%02u:%02u:%02u", rh, rm, rs); // Thêm %02u cho giây
      display.setCursor(0, y);
      display.print("Con lai: ");
      display.println(timeStr);
      y += 16;
    }
    else
    {
      display.setCursor(0, y);
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
void processIRRemote()
{
  if (IrReceiver.decode())
  {
    uint32_t code = IrReceiver.decodedIRData.decodedRawData;
    unsigned long currentTime = millis();
    if (code != 0 && (code != lastCode || (currentTime - lastReceiveTime > debounceTime)))
    {
      Serial.print("IR Code: ");
      Serial.println(code);
      String cmd = "";
      switch (code)
      {
      case IR_CODE_UP:
        cmd = "UP";
        break;
      case IR_CODE_DOWN:
        cmd = "DOWN";
        break;
      case IR_CODE_LEFT:
        cmd = "LEFT";
        break;
      case IR_CODE_RIGHT:
        cmd = "RIGHT";
        break;
      case IR_CODE_OK:
        cmd = "OK";
        break;
      case IR_CODE_1:
        cmd = "1";
        break;
      case IR_CODE_2:
        cmd = "2";
        break;
      case IR_CODE_3:
        cmd = "3";
        break;
      case IR_CODE_4:
        cmd = "4";
        break;
      case IR_CODE_5:
        cmd = "5";
        break;
      case IR_CODE_6:
        cmd = "6";
        break;
      case IR_CODE_7:
        cmd = "7";
        break;
      case IR_CODE_8:
        cmd = "8";
        break;
      case IR_CODE_9:
        cmd = "9";
        break;
      case IR_CODE_0:
        cmd = "0";
        break;
      case IR_CODE_STAR:
        cmd = "STAR";
        break;
      case IR_CODE_HASH:
        cmd = "HASH";
        break;
      default:
        cmd = "UNKNOWN";
        break;
      }
      Serial.print("IR Command: ");
      Serial.println(cmd);
      lastCode = code;
      lastReceiveTime = currentTime;

      // Xử lý theo trạng thái menu:
      if (currentMenu == MAIN_MENU)
      {
        if (cmd == "UP" || cmd == "DOWN")
        {
          mainMenuSelection = 1 - mainMenuSelection; // toggle
        }
        else if (cmd == "OK")
        {
          if (mainMenuSelection == 0)
          {
            // Backup cấu hình hiện tại trước khi vào config
            for (int i = 0; i < 10; i++)
            {
              backupConfig[i] = allowedSprinklers[i];
            }
            currentMenu = CONFIG_MENU;
          }
          else
          {
            currentMenu = TIME_SELECT_MENU;
          }
        }
      }
      else if (currentMenu == CONFIG_MENU)
      {
        if (cmd == "UP")
        {
          if (configSelected > 0)
            configSelected--;
        }
        else if (cmd == "DOWN")
        {
          if (configSelected < 9)
            configSelected++;
        }
        else if (cmd == "OK")
        {
          allowedSprinklers[configSelected] = !allowedSprinklers[configSelected];
        }
        else if (cmd == "LEFT" || cmd == "STAR")
        {
          // Hủy cấu hình: khôi phục backup và quay lại MAIN_MENU
          for (int i = 0; i < 10; i++)
          {
            allowedSprinklers[i] = backupConfig[i];
          }
          currentMenu = MAIN_MENU;
        }
        else if (cmd == "HASH")
        {
          // Lưu cấu hình và quay lại MAIN_MENU
          currentMenu = MAIN_MENU;
        }
      }
      else if (currentMenu == TIME_SELECT_MENU)
      {
        if (cmd == "UP")
        {
          irrigationTime += TIME_STEP; // tăng 15 phút
        }
        else if (cmd == "DOWN")
        {
          if (irrigationTime > TIME_STEP)
            irrigationTime -= TIME_STEP;
        }
        else if (cmd == "OK")
        {
          // Tạo danh sách các béc được bật theo cấu hình
          runSprinklerCount = 0;
          for (int i = 0; i < 10; i++)
          {
            if (allowedSprinklers[i])
            {
              runSprinklerIndices[runSprinklerCount++] = i;
            }
          }
          if (runSprinklerCount == 0)
          {
            // Hiển thị lỗi trên OLED khi chưa bật béc nào
            display.clearDisplay();
            display.setCursor(0, 0);
            display.println("CHUA BAT BEC NAO");
            display.display();
            delay(2000);
            currentMenu = MAIN_MENU;
          }
          else
          {
            currentMenu = RUNNING;
            currentRunIndex = 0;
            runStartTime = millis();
            int bec = runSprinklerIndices[currentRunIndex];
            digitalWrite(relayPins[bec], HIGH);
            if (bec < 6)
            {
              digitalWrite(relayPins[10], HIGH);
              digitalWrite(relayPins[11], LOW);
            }
            else
            {
              digitalWrite(relayPins[10], HIGH);
              digitalWrite(relayPins[11], HIGH);
            }
          }
        }
        else if (cmd == "LEFT")
        {
          currentMenu = MAIN_MENU;
        }
      }
      else if (currentMenu == RUNNING)
      {
        if (cmd == "LEFT")
        {
          for (int i = 0; i < 12; i++)
          {
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
// Hàm cập nhật chu trình RUNNING (mỗi béc tưới chạy trong irrigationTime phút)
// Mỗi khi hết thời gian của béc hiện hành, nếu có béc kế tiếp, mở béc mới trước, sau đó tắt béc cũ.
// Nếu là béc cuối cùng, tắt tất cả và kết thúc chu trình.
// -------------------------
void updateRunning()
{
  if (currentMenu != RUNNING)
    return;
  unsigned long duration = (unsigned long)irrigationTime * 60000UL;
  unsigned long elapsed = millis() - runStartTime;
  if (elapsed >= duration)
  {
    if (currentRunIndex + 1 < runSprinklerCount)
    {
      // Mở béc kế tiếp trước
      int nextBec = runSprinklerIndices[currentRunIndex + 1];
      digitalWrite(relayPins[nextBec], HIGH);
      if (nextBec < 6)
      {
        digitalWrite(relayPins[10], HIGH);
        digitalWrite(relayPins[11], LOW);
      }
      else
      {
        digitalWrite(relayPins[10], HIGH);
        digitalWrite(relayPins[11], HIGH);
      }
      delay(100); // Delay nhỏ để đảm bảo béc mới bật
      // Sau đó tắt béc cũ
      int currentBec = runSprinklerIndices[currentRunIndex];
      digitalWrite(relayPins[currentBec], LOW);
      currentRunIndex++;
      runStartTime = millis();
    }
    else
    {
      // Béc cuối cùng: tắt sau khi đủ thời gian
      int currentBec = runSprinklerIndices[currentRunIndex];
      digitalWrite(relayPins[currentBec], LOW);
      digitalWrite(relayPins[10], LOW);
      digitalWrite(relayPins[11], LOW);
      currentMenu = MAIN_MENU;
    }
  }
}

// -------------------------
// Hàm cập nhật trạng thái kết nối Blynk
// -------------------------
void updateConnectionStatusWrapper()
{
  if (Blynk.connected())
  {
    static bool ledStatus = false;
    ledStatus = !ledStatus;
    Blynk.virtualWrite(V0, ledStatus ? 1 : 0);
  }
  else
  {
    Blynk.virtualWrite(V0, 0);
  }
}

// -------------------------
// setup()
// -------------------------
void setup()
{
  Serial.begin(115200);
  for (int i = 0; i < 12; i++)
  {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], LOW);
  }
  // Khởi tạo cấu hình mặc định cho 10 béc: ban đầu tắt hết
  for (int i = 0; i < 10; i++)
  {
    allowedSprinklers[i] = false;
  }
  Wire.begin(32, 33);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  {
    Serial.println("OLED SSD1306 khong khoi tao duoc");
    while (true)
      ;
  }
  display.clearDisplay();
  display.display();
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  Serial.println("ESP32 connected to Blynk!");
  gpio_viewer.connectToWifi(ssid, pass);
  gpio_viewer.begin();
  setupIR();
}

// -------------------------
// loop()
// -------------------------
void loop()
{
  Blynk.run();
  processIRRemote();
  if (currentMenu == RUNNING)
  {
    updateRunning();
  }
  updateMenuDisplay();
  updateConnectionStatusWrapper();
  delay(100);
}
