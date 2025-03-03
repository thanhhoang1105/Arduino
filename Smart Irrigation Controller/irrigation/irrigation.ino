#include <gpio_viewer.h> // Phải include thư viện này đầu tiên
GPIOViewer gpio_viewer;

// acc thanhhoangngoc.bmt@gmail.com
// #define BLYNK_TEMPLATE_ID "TMPL6xy4mK0KN"
// #define BLYNK_TEMPLATE_NAME "Smart Irrigation Controller"
// #define BLYNK_AUTH_TOKEN "J34tRXWg_QTXt8bs0SfbZFSc1ARUpL0o"

// acc thanhhoangngoc.bmt1105@gmail.com
#define BLYNK_TEMPLATE_ID "TMPL6ZR2keF5J"
#define BLYNK_TEMPLATE_NAME "Smart Irrigation Controller"
#define BLYNK_AUTH_TOKEN "x-Uc3uA1wCZtbDWniO6yWTDrLHsVqOJ-"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <IRremote.hpp>

// =========================
// OLED, WiFi & Relay Setup
// =========================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1 // Không sử dụng chân reset
// OLED sử dụng I2C với SDA = 32, SCL = 33
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

char ssid[] = "VU ne";
char pass[] = "12341234";

// Relay pins:
// Relay 1->10: Béc tưới; Relay 11: Bơm 1, Relay 12: Bơm 2
const int relayPins[12] = {2, 4, 16, 17, 5, 18, 19, 21, 22, 23, 26, 25};

// =========================
// IR Remote Setup (IRremote.hpp)
// =========================
const int RECV_PIN = 15; // Chân nhận IR
void setupIR()
{
  IrReceiver.begin(RECV_PIN, ENABLE_LED_FEEDBACK);
  Serial.println("IR Receiver đang chờ tín hiệu...");
}

// Định nghĩa các mã IR (theo giá trị cung cấp)
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
#define IR_CODE_STAR 3910598400UL // Dùng làm BACK/CANCEL
#define IR_CODE_HASH 4061003520UL // Dùng làm SAVE (lưu cấu hình)
#define IR_CODE_UP 3877175040UL
#define IR_CODE_DOWN 2907897600UL
#define IR_CODE_RIGHT 2774204160UL
#define IR_CODE_LEFT 4144561920UL
#define IR_CODE_OK 3810328320UL

// Biến debounce cho IR
unsigned long lastCode = 0;
unsigned long lastReceiveTime = 0;
const unsigned long debounceTime = 500; // ms

// =========================
// Menu State Definition
// =========================
enum MenuState
{
  MAIN_MENU,
  CONFIG_MENU,
  TIME_SELECT_MENU,
  RUNNING
};
MenuState currentMenu = MAIN_MENU;

// MAIN_MENU: 0 - CAU HINH, 1 - KHOI DONG
int mainMenuSelection = 0;

// CONFIG_MENU: cấu hình 10 béc tưới (index 0->9)
bool allowedSprinklers[10];
bool backupConfig[10];      // Backup cấu hình khi vào CONFIG_MENU
int configSelected = 0;     // Béc tưới được chọn
int configScrollOffset = 0; // Biến cuộn cho CONFIG_MENU
const int maxVisible = 3;   // Số dòng cấu hình hiển thị (sau header)

// TIME_SELECT_MENU:
unsigned int irrigationTime = 120; // Thời gian tưới mỗi béc (phút) – mặc định 120 phút (2 giờ)
#define TIME_STEP_HOUR 1           // Bước tăng giảm giờ = 1
#define TIME_STEP_MIN 15           // Bước tăng giảm phút = 15
// Các biến cho chế độ chỉnh giờ/phút:
bool timeSelectHour = true;              // true: chỉnh giờ, false: chỉnh phút
int currentHour = irrigationTime / 60;   // Lưu giờ hiện tại
int currentMinute = irrigationTime % 60; // Lưu phút hiện tại
String timeInput = "";                   // Chuỗi tạm nhập số
unsigned long lastTimeInput = 0;         // Thời gian nhập số cuối

// RUNNING:
unsigned long runStartTime = 0; // Thời gian bắt đầu chu trình tưới
int runSprinklerIndices[10];    // Danh sách béc tưới được bật theo cấu hình
int runSprinklerCount = 0;      // Số béc được bật
int currentRunIndex = 0;        // Béc tưới hiện đang chạy
bool cycleStarted = false;      // Đã bắt đầu chu trình?

// =========================
// Blynk Virtual Pin Callbacks
// =========================
// Điều khiển béc tưới: V1 - V10
BLYNK_WRITE(V1) { digitalWrite(relayPins[0], param.asInt()); }
BLYNK_WRITE(V2) { digitalWrite(relayPins[1], param.asInt()); }
BLYNK_WRITE(V3) { digitalWrite(relayPins[2], param.asInt()); }
BLYNK_WRITE(V4) { digitalWrite(relayPins[3], param.asInt()); }
BLYNK_WRITE(V5) { digitalWrite(relayPins[4], param.asInt()); }
BLYNK_WRITE(V6) { digitalWrite(relayPins[5], param.asInt()); }
BLYNK_WRITE(V7) { digitalWrite(relayPins[6], param.asInt()); }
BLYNK_WRITE(V8) { digitalWrite(relayPins[7], param.asInt()); }
BLYNK_WRITE(V9) { digitalWrite(relayPins[8], param.asInt()); }
BLYNK_WRITE(V10) { digitalWrite(relayPins[9], param.asInt()); }

// Điều khiển bơm: V11 và V12 với cơ chế bảo vệ
BLYNK_WRITE(V11)
{
  int state = param.asInt();
  bool sprayerActive = false;
  for (int i = 0; i < 10; i++)
  {
    if (digitalRead(relayPins[i]) == HIGH)
    {
      sprayerActive = true;
      break;
    }
  }
  if (state == 1 && !sprayerActive)
  {
    Blynk.logEvent("pump_protect", "Không có béc được bật. Tắt bơm 1");
    digitalWrite(relayPins[10], LOW);
  }
  else
  {
    digitalWrite(relayPins[10], state);
  }
}
BLYNK_WRITE(V12)
{
  int state = param.asInt();
  bool sprayerActive = false;
  for (int i = 0; i < 10; i++)
  {
    if (digitalRead(relayPins[i]) == HIGH)
    {
      sprayerActive = true;
      break;
    }
  }
  if (state == 1 && !sprayerActive)
  {
    Blynk.logEvent("pump_protect", "Không có béc được bật. Tắt bơm 2");
    digitalWrite(relayPins[11], LOW);
  }
  else
  {
    digitalWrite(relayPins[11], state);
  }
}

// =========================
// Các hàm bảo vệ, đồng bộ, hiển thị
// =========================

// Kiểm tra bảo vệ pump: nếu không có béc nào bật mà bơm đang bật -> tắt bơm.
void checkPumpProtection()
{
  static bool lastPumpProtectionTriggered = false;
  bool sprayerActive = false;
  for (int i = 0; i < 10; i++)
  {
    if (digitalRead(relayPins[i]) == HIGH)
    {
      sprayerActive = true;
      break;
    }
  }
  bool pumpOn = (digitalRead(relayPins[10]) == HIGH || digitalRead(relayPins[11]) == HIGH);
  if (!sprayerActive && pumpOn)
  {
    if (!lastPumpProtectionTriggered)
    {
      lastPumpProtectionTriggered = true;
      Blynk.logEvent("pump_protect", "Không có béc được bật. Tắt tất cả bơm");
      digitalWrite(relayPins[10], LOW);
      digitalWrite(relayPins[11], LOW);
      Serial.println("Pump protection triggered: No sprayer active, pumps turned off");
    }
  }
  else
  {
    lastPumpProtectionTriggered = false;
  }
}

// =========================
// Hàm đồng bộ trạng thái relay với Blynk (chỉ gửi khi có thay đổi)
// =========================
void syncRelayStatusToBlynk()
{
  static int lastRelayStates[12] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
  for (int i = 0; i < 12; i++)
  {
    int currentState = digitalRead(relayPins[i]);
    if (currentState != lastRelayStates[i])
    {
      lastRelayStates[i] = currentState;
      if (i < 10)
        Blynk.virtualWrite(i + 1, currentState);
      else if (i == 10)
        Blynk.virtualWrite(11, currentState);
      else if (i == 11)
        Blynk.virtualWrite(12, currentState);
      Serial.print("Relay ");
      Serial.print(i);
      Serial.print(" state changed to ");
      Serial.println(currentState);
    }
  }
}

// Cơ chế gửi trạng thái kết nối Blynk (V0 LED) chỉ khi được kích hoạt qua V13.
bool enableConnIndicator = false;
BLYNK_WRITE(V13)
{
  enableConnIndicator = param.asInt() ? true : false;
  Serial.print("Connection indicator: ");
  Serial.println(enableConnIndicator ? "Enabled" : "Disabled");
}

// =========================
// Hàm cập nhật trạng thái kết nối Blynk (V0 LED)
// Chỉ gửi khi trạng thái thay đổi và nếu enableConnIndicator được bật (V13)
// =========================
void updateConnectionStatusWrapper()
{
  if (!enableConnIndicator)
    return;
  bool currentConn = Blynk.connected();
  Blynk.virtualWrite(V0, currentConn ? 1 : 0);
  Serial.print("Connection status: ");
  Serial.println(currentConn ? "Connected" : "Disconnected");
}

// Hàm hiển thị menu trên OLED (text size = 1, line spacing = 16)
void updateMenuDisplay()
{
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // Dòng đầu tiên: trạng thái kết nối (đặt ở góc phải)
  display.setCursor(110, 0);
  if (Blynk.connected())
    display.println("ON");
  else
    display.println("OFF");

  int y = 0;

  switch (currentMenu)
  {
  case MAIN_MENU:
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

  case CONFIG_MENU:
    display.setCursor(0, y);
    display.println("CAU HINH:");
    y += 16;
    // Cập nhật configScrollOffset để configSelected luôn nằm trong vùng hiển thị
    if (configSelected < configScrollOffset)
      configScrollOffset = configSelected;
    if (configSelected > configScrollOffset + maxVisible - 1)
      configScrollOffset = configSelected - (maxVisible - 1);
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
    break;

  case TIME_SELECT_MENU:
  {
    display.setCursor(0, y);
    display.println("CHON THOI GIAN");
    y += 16;
    display.setCursor(0, y);
    // Hiển thị con trỏ cho trường đang chỉnh và hiển thị giờ, phút
    display.print(timeSelectHour ? ">" : " ");
    display.print("Hour: ");
    display.print(currentHour);
    display.print("  ");
    display.print(!timeSelectHour ? ">" : " ");
    display.print("Min: ");
    display.println(currentMinute);
    break;
  }

  case RUNNING:
    display.setCursor(0, y);
    if (currentRunIndex < runSprinklerCount)
    {
      display.print("BEC ");
      display.print(runSprinklerIndices[currentRunIndex] + 1);
      display.println(" DANG CHAY");
      y += 16;
      unsigned long elapsed = (millis() - runStartTime) / 1000; // giây
      unsigned long totalSec = irrigationTime * 60;
      unsigned long remain = (totalSec > elapsed) ? (totalSec - elapsed) : 0;
      unsigned int rh = remain / 3600;
      unsigned int rm = (remain % 3600) / 60;
      unsigned int rs = remain % 60;
      char timeStr[9];
      sprintf(timeStr, "%02u:%02u:%02u", rh, rm, rs);
      display.setCursor(0, y);
      display.print("CON LAI: ");
      display.println(timeStr);
      y += 16;
    }
    else
    {
      display.setCursor(0, y);
      display.println("XONG CHU TRINH");
      delay(2000);
      currentMenu = MAIN_MENU;
    }
    break;
  }
  display.display();
}

// =========================
// Hàm xử lý IR Remote (sử dụng IRremote.hpp)
// =========================
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

      // =========================
      // Xử lý theo trạng thái menu
      // =========================
      if (currentMenu == MAIN_MENU)
      {
        if (cmd == "UP" || cmd == "DOWN")
        {
          mainMenuSelection = 1 - mainMenuSelection; // Toggle
        }
        else if (cmd == "OK")
        {
          if (mainMenuSelection == 0)
          {
            // Backup cấu hình trước khi vào CONFIG_MENU
            for (int i = 0; i < 10; i++)
            {
              backupConfig[i] = allowedSprinklers[i];
            }
            currentMenu = CONFIG_MENU;
          }
          else
          {
            currentMenu = TIME_SELECT_MENU;
            timeSelectHour = true; // Mặc định chỉnh giờ
            currentHour = irrigationTime / 60;
            currentMinute = irrigationTime % 60;
            timeInput = "";
            lastTimeInput = 0;
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
        else if (cmd == "STAR")
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
          if (timeSelectHour)
          {
            currentHour += TIME_STEP_HOUR;
          }
          else
          {
            currentMinute += TIME_STEP_MIN;
            if (currentMinute >= 60)
              currentMinute = 59;
          }
          irrigationTime = currentHour * 60 + currentMinute;
        }
        else if (cmd == "DOWN")
        {
          if (timeSelectHour)
          {
            if (currentHour > 0)
              currentHour -= TIME_STEP_HOUR;
          }
          else
          {
            if (currentMinute >= TIME_STEP_MIN)
              currentMinute -= TIME_STEP_MIN;
            else
              currentMinute = 0;
          }
          irrigationTime = currentHour * 60 + currentMinute;
        }
        else if (cmd == "LEFT" || cmd == "RIGHT")
        {
          // Chuyển đổi giữa chỉnh giờ và chỉnh phút
          timeSelectHour = !timeSelectHour;
          timeInput = "";
          lastTimeInput = 0;
        }
        else if (cmd == "OK")
        {
          // Nếu giá trị giờ và phút bằng 0, không cho chạy mà quay lại menu
          if (currentHour == 0 && currentMinute == 0)
          {
            Serial.println("Time is 0. Exiting TIME_SELECT_MENU.");
            display.clearDisplay();
            display.setTextSize(1);
            display.setTextColor(SSD1306_WHITE);
            display.setCursor(0, 0);
            display.println("0 HOUR 0 MIN");
            display.display();
            delay(2000);
            currentMenu = MAIN_MENU;
          }
          else
          {
            // Tạo danh sách các béc bật theo cấu hình và chuyển sang RUNNING
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
              Serial.println("Chưa chọn béc nào!");
              display.clearDisplay();
              display.setTextSize(1);
              display.setTextColor(SSD1306_WHITE);
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
        }
        else if (cmd >= "0" && cmd <= "9")
        {
          // Xử lý nhập số 2 chữ số cho giờ/phút
          unsigned long now = millis();
          if (now - lastTimeInput < 1000)
          {
            timeInput += cmd;
          }
          else
          {
            timeInput = cmd;
          }
          lastTimeInput = now;
          int value = timeInput.toInt();
          if (timeSelectHour)
          {
            currentHour = value;
          }
          else
          {
            currentMinute = value;
            if (currentMinute >= 60)
              currentMinute = 59;
          }
          irrigationTime = currentHour * 60 + currentMinute;
        }
        else if (cmd == "STAR")
        { // Dùng STAR để hủy TIME_SELECT_MENU
          currentMenu = MAIN_MENU;
        }
      }
      else if (currentMenu == RUNNING)
      {
        if (cmd == "STAR")
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

// =========================
// Hàm cập nhật chu trình RUNNING:
// Mỗi béc chạy trong irrigationTime phút.
// Khi hết thời gian của béc hiện hành, nếu có béc kế tiếp, mở béc mới trước (đợi 10 giây) rồi tắt béc cũ.
// Nếu béc cuối cùng, tắt tất cả và kết thúc chu trình.
// =========================
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
      int nextBec = runSprinklerIndices[currentRunIndex + 1];
      digitalWrite(relayPins[nextBec], HIGH); // Mở béc mới trước
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
      delay(10000); // Đợi 10 giây
      int currentBec = runSprinklerIndices[currentRunIndex];
      digitalWrite(relayPins[currentBec], LOW); // Tắt béc cũ
      currentRunIndex++;
      runStartTime = millis();
    }
    else
    {
      int currentBec = runSprinklerIndices[currentRunIndex];
      digitalWrite(relayPins[currentBec], LOW);
      digitalWrite(relayPins[10], LOW);
      digitalWrite(relayPins[11], LOW);
      currentMenu = MAIN_MENU;
    }
  }
}

BlynkTimer timer; // Timer toàn cục

// =========================
// setup()
// =========================
void setup()
{
  Serial.begin(115200);
  for (int i = 0; i < 12; i++)
  {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], LOW);
  }
  // Khởi tạo cấu hình mặc định: tất cả béc OFF
  for (int i = 0; i < 10; i++)
  {
    allowedSprinklers[i] = false;
  }
  Wire.begin(32, 33);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  {
    Serial.println("OLED SSD1306 không khởi tạo được");
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
  timer.setInterval(2000L, updateConnectionStatusWrapper); // Cập nhật trạng thái kết nối mỗi 2 giây
}

// =========================
// loop()
// =========================
void loop()
{
  Blynk.run();
  processIRRemote();
  if (currentMenu == RUNNING)
  {
    updateRunning();
  }
  updateMenuDisplay();
  checkPumpProtection();
  syncRelayStatusToBlynk();
  delay(100);
}
