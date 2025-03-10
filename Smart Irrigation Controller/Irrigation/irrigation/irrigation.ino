// acc thanhhoangngoc.bmt1105@gmail.com
// ===========================================================
// Smart Irrigation Controller với ESP32, Blynk, OLED, IR Remote và ACS712
// ===========================================================

// ------------------ Thông tin cấu hình Blynk ------------------
#define BLYNK_TEMPLATE_ID "TMPL6ZR2keF5J"
#define BLYNK_TEMPLATE_NAME "Smart Irrigation Controller"
#define BLYNK_AUTH_TOKEN "x-Uc3uA1wCZtbDWniO6yWTDrLHsVqOJ-"

// ------------------ Thư viện sử dụng ------------------
#include <Preferences.h>
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <IRremote.hpp>
#include <time.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include "ACS712.h"

// ------------------ Đối tượng và cấu hình chung ------------------

// Cấu hình WiFi và Blynk
char ssid[] = "VU ne";
char pass[] = "12341234";

// Đối tượng Preferences dùng để lưu và tải cấu hình vào flash
Preferences preferences;

// Cấu hình NTP (dùng để cập nhật giờ) với múi giờ +7
WiFiUDP ntpUDP;
NTPClient ntpClient(ntpUDP, "pool.ntp.org", 7 * 3600, 60000);

// Cấu hình màn hình OLED (SSD1306)
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Cấu hình các chân Relay: 10 béc tưới, 2 bơm (chân relayPins[10] và relayPins[11])
const int relayPins[12] = {2, 4, 16, 17, 5, 18, 19, 21, 22, 23, 26, 25};

// ------------------ Cấu hình ACS712 ------------------
// ACS712 được kết nối vào chân 34, nguồn 5V, ADC 12-bit (0-4095) và độ nhạy 100 mV/A (cho module 20A)
ACS712 ACS(34, 5.0, 4095, 100);

// Hàm hiệu chuẩn ACS712 (tự động tính midpoint khi không có dòng điện)
void calibrateACS712()
{
  Serial.println("Calibrating ACS712...");
  ACS.autoMidPoint(); // Tính midpoint tự động
  Serial.print("MidPoint: ");
  Serial.print(ACS.getMidPoint());
  Serial.print(". Noise mV: ");
  Serial.println(ACS.getNoisemV());
}

// ------------------ Cấu hình IR Remote ------------------
const int RECV_PIN = 15; // Chân kết nối IR receiver
// Hàm khởi tạo IR Remote
void setupIR()
{
  IrReceiver.begin(RECV_PIN, ENABLE_LED_FEEDBACK);
  Serial.println("IR Receiver đang chờ tín hiệu...");
}

// Định nghĩa mã IR (mã số của nút điều khiển)
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
#define IR_CODE_STAR 3910598400UL // CANCEL
#define IR_CODE_HASH 4061003520UL // SAVE
#define IR_CODE_UP 3877175040UL
#define IR_CODE_DOWN 2907897600UL
#define IR_CODE_RIGHT 2774204160UL
#define IR_CODE_LEFT 4144561920UL
#define IR_CODE_OK 3810328320UL

// Biến debounce cho IR Remote
unsigned long lastCode = 0;
unsigned long lastReceiveTime = 0;
const unsigned long debounceTime = 500;

// ------------------ Các biến global cho MENU & TIME ------------------

// Các trạng thái menu chính
enum MenuState
{
  MAIN_MENU,
  CONFIG_MENU,
  TIME_SELECT_MENU,
  AUTO_MENU,
  AUTO_TIME_MENU,
  RUNNING,
  RUNNING_AUTO
};
MenuState currentMenu = MAIN_MENU;

// Các lựa chọn trong MAIN_MENU: 0 - CAU HINH, 1 - KHỞI ĐỘNG THỦ CÔNG, 2 - TU ĐỘNG
int mainMenuSelection = 0;

// ------------------ Cấu hình cho CONFIG_MENU ------------------
int configMode = 0;         // 0: Béc tưới, 1: Delay
bool allowedSprinklers[10]; // Trạng thái bật/tắt của 10 béc tưới
bool backupConfig[10];      // Sao lưu cấu hình tạm thời khi chỉnh sửa
int configSelected = 0;     // Chỉ số béc đang được chọn
int configScrollOffset = 0;
const int maxVisibleList = 2; // Số mục hiển thị tối đa
int transitionDelaySeconds = 10;
int transitionDelayTemp = 10;

// ------------------ Cấu hình cho TIME_SELECT_MENU (chế độ thủ công) ------------------
unsigned int irrigationTime = 120; // Thời gian tưới mặc định (120 phút)
#define TIME_STEP_HOUR 1
#define TIME_STEP_MIN 15
int currentHour = irrigationTime / 60;
int currentMinute = irrigationTime % 60;
bool timeSelectHour = true;
int tempHour = currentHour;
int tempMinute = currentMinute;
String timeInput = "";
unsigned long lastTimeInput = 0;

// ------------------ Các biến chạy chế độ RUNNING (thủ công) ------------------
unsigned long runStartTime = 0; // Thời điểm bắt đầu chu trình tưới
int runSprinklerIndices[10];    // Danh sách chỉ số béc sẽ chạy
int runSprinklerCount = 0;      // Số lượng béc được kích hoạt
int currentRunIndex = 0;        // Béc hiện hành trong chu trình
bool cycleStarted = false;
bool paused = false;                   // Cờ cho biết chu trình đã tạm dừng
unsigned long pauseRemaining = 0;      // Thời gian còn lại của chu trình khi bị tạm dừng
bool transitionActive = false;         // Cờ cho biết đang trong giai đoạn chuyển tiếp giữa các béc
unsigned long transitionStartTime = 0; // Thời điểm bắt đầu giai đoạn chuyển tiếp

// ------------------ Các biến chạy chế độ AUTO (tự động) ------------------
bool autoTransitionActive = false;
unsigned long autoTransitionStartTime = 0;
int autoCurrentRunIndex = 0;   // Chỉ số béc hiện hành trong chế độ tự động
int autoRunSprinklerCount = 0; // Số lượng béc sẽ tưới trong chu trình auto

// Các tùy chọn trong menu AUTO
enum AutoOption
{
  AUTO_ON_OPT,
  AUTO_OFF_OPT,
  AUTO_RESET_OPT,
  AUTO_TIME_OPT
};
int autoMenuSelection = 0;
const int maxVisibleAuto = 3;
int autoScrollOffset = 0;

// Chế độ AUTO ON/OFF
enum AutoMode
{
  AUTO_ON,
  AUTO_OFF
};
AutoMode autoMode = AUTO_OFF;
int autoCurrentSprinkler = 0;

// Cấu hình thời gian tự động (theo số phút)
int autoStartMinute = 1320; // FROM: 22:00 (1320 phút)
int autoEndMinute = 240;    // TO: 4:00 (240 phút)
int autoDurationHour = 3;   // DUR: 3 giờ
int autoDurationMinute = 0;

// ------------------ Các biến cho chế độ AUTO_TIME_MENU (chỉnh sửa thời gian auto) ------------------
int autoTimeSelectField = 0; // 0: FROM, 1: TO, 2: DUR
int autoTimeEditPhase = 0;   // 0: chỉnh giờ, 1: chỉnh phút
int autoStartHourTemp = 0, autoStartMinTemp = 0;
int autoEndHourTemp = 0, autoEndMinTemp = 0;
int autoDurHourTemp = autoDurationHour;
int autoDurMinTemp = autoDurationMinute;
String autoTimeInput = "";
unsigned long lastAutoTimeInput = 0;
unsigned long lastAutoCycleEndEpoch = 0; // Lưu thời gian kết thúc chu trình auto (epoch)

// Biến debug để in thông tin trạng thái auto mode (mỗi 10 giây)
unsigned long lastDebugTime = 0;
const unsigned long debugInterval = 10000; // 10 giây

// ------------------ Biến để theo dõi trạng thái tạm dừng do mất điện ------------------
bool powerPaused = false; // true nếu đã tự động pause do mất điện

// ===========================================================
// Các hàm helper dùng chung
// ===========================================================

// Chuyển đổi số phút sang định dạng HH:MM
String formatTime(int minutes)
{
  int hour = minutes / 60;
  int min = minutes % 60;
  char timeStr[6];
  sprintf(timeStr, "%d:%02d", hour, min);
  return String(timeStr);
}

// Chuyển đổi số giây thành định dạng HH:MM:SS
String formatRemainingTime(unsigned long seconds)
{
  unsigned int h = seconds / 3600;
  unsigned int m = (seconds % 3600) / 60;
  unsigned int s = seconds % 60;
  char buf[9];
  sprintf(buf, "%02u:%02u:%02u", h, m, s);
  return String(buf);
}

// Ánh xạ mã IR sang chuỗi command
String mapIRCodeToCommand(uint32_t code)
{
  switch (code)
  {
  case IR_CODE_UP:
    return "UP";
  case IR_CODE_DOWN:
    return "DOWN";
  case IR_CODE_LEFT:
    return "LEFT";
  case IR_CODE_RIGHT:
    return "RIGHT";
  case IR_CODE_OK:
    return "OK";
  case IR_CODE_1:
    return "1";
  case IR_CODE_2:
    return "2";
  case IR_CODE_3:
    return "3";
  case IR_CODE_4:
    return "4";
  case IR_CODE_5:
    return "5";
  case IR_CODE_6:
    return "6";
  case IR_CODE_7:
    return "7";
  case IR_CODE_8:
    return "8";
  case IR_CODE_9:
    return "9";
  case IR_CODE_0:
    return "0";
  case IR_CODE_STAR:
    return "STAR";
  case IR_CODE_HASH:
    return "HASH";
  default:
    return "UNKNOWN";
  }
}

// ------------------ Hàm điều khiển bơm và béc tưới ------------------

// Bật bơm cho béc tưới tương ứng
void activatePumpForSprinkler(int sprinklerIndex)
{
  digitalWrite(relayPins[sprinklerIndex], HIGH);
  if (sprinklerIndex < 6)
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

// Tắt béc tưới và bơm
void turnOffCurrentSprinklerAndPump(int sprinklerIndex)
{
  digitalWrite(relayPins[sprinklerIndex], LOW);
  digitalWrite(relayPins[10], LOW);
  digitalWrite(relayPins[11], LOW);
}

// Hàm debug in thông tin Auto Mode (dùng để kiểm tra nội bộ)
void debugPrintAutoMode(unsigned long currentEpoch, unsigned long lastEpoch, int currentSprinkler, unsigned long diff, bool skipCycle)
{
  Serial.println("----- Auto Mode Debug Info -----");
  Serial.println("--------------------------------");
}

// ===========================================================
// Hàm hiển thị MENU trên OLED
// ===========================================================
void updateMenuDisplay()
{
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  int y = 0;

  // Hiển thị trạng thái kết nối Blynk (ON/OFF) góc trên bên phải
  display.setCursor(110, y);
  display.println(Blynk.connected() ? "ON" : "OFF");

  switch (currentMenu)
  {
  case MAIN_MENU:
    if (mainMenuSelection == 0)
    {
      display.setCursor(0, y);
      display.println("> CAU HINH");
      y += 16;
      display.setCursor(0, y);
      display.println("  KHOI DONG THU CONG");
      y += 16;
      display.setCursor(0, y);
      display.print("  TU DONG (");
      display.print((autoMode == AUTO_ON) ? "ON - " : "OFF - ");
      display.print(autoCurrentSprinkler + 1);
      display.print(")");
      y += 16;
      display.setCursor(0, y);
      display.print("  [");
      display.print(formatTime(autoStartMinute));
      display.print("-");
      display.print(formatTime(autoEndMinute));
      display.println("]");
    }
    else if (mainMenuSelection == 1)
    {
      display.setCursor(0, y);
      display.println("  CAU HINH");
      y += 16;
      display.setCursor(0, y);
      display.println("> KHOI DONG THU CONG");
      y += 16;
      display.setCursor(0, y);
      display.print("  TU DONG (");
      display.print((autoMode == AUTO_ON) ? "ON - " : "OFF - ");
      display.print(autoCurrentSprinkler + 1);
      display.print(")");
      y += 16;
      display.setCursor(0, y);
      display.print("  [");
      display.print(formatTime(autoStartMinute));
      display.print("-");
      display.print(formatTime(autoEndMinute));
      display.println("]");
    }
    else
    { // mainMenuSelection == 2
      display.setCursor(0, y);
      display.println("  CAU HINH");
      y += 16;
      display.setCursor(0, y);
      display.println("  KHOI DONG THU CONG");
      y += 16;
      display.setCursor(0, y);
      display.print("> TU DONG (");
      display.print((autoMode == AUTO_ON) ? "ON - " : "OFF - ");
      display.print(autoCurrentSprinkler + 1);
      display.print(")");
      y += 16;
      display.setCursor(0, y);
      display.print("  [");
      display.print(formatTime(autoStartMinute));
      display.print("-");
      display.print(formatTime(autoEndMinute));
      display.println("]");
    }
    break;

  case CONFIG_MENU:
    if (configMode == 0)
    {
      display.setCursor(0, y);
      display.println("CAU HINH - BEC:");
      y += 16;
      if (configSelected < configScrollOffset)
        configScrollOffset = configSelected;
      if (configSelected > configScrollOffset + maxVisibleList - 1)
        configScrollOffset = configSelected - (maxVisibleList - 1);
      for (int i = configScrollOffset; i < 10 && i < configScrollOffset + maxVisibleList; i++)
      {
        display.setCursor(0, y);
        display.print(i == configSelected ? ">" : " ");
        display.print("Bec ");
        display.print(i + 1);
        display.print(": ");
        display.println(allowedSprinklers[i] ? "ON" : "OFF");
        y += 16;
      }
      display.setCursor(0, SCREEN_HEIGHT - 16);
      display.println("*: Cancel  #: Save");
    }
    else
    {
      display.setCursor(0, y);
      display.println("CAU HINH - DELAY:");
      y += 16;
      display.setCursor(0, y);
      display.print("Delay (s): ");
      display.println(transitionDelayTemp);
      display.setCursor(0, SCREEN_HEIGHT - 16);
      display.println("*: Cancel  #: Save");
    }
    break;

  case TIME_SELECT_MENU:
    display.setCursor(0, y);
    display.println("CHON THOI GIAN");
    y += 16;
    display.setCursor(0, y);
    display.print(timeSelectHour ? ">" : " ");
    display.print("Hour: ");
    display.print(tempHour);
    display.print("  ");
    display.print(!timeSelectHour ? ">" : " ");
    display.print("Min: ");
    display.println(tempMinute);
    display.setCursor(0, SCREEN_HEIGHT - 16);
    display.println("*: Cancel  #: Save");
    break;

  case AUTO_MENU:
  {
    display.setCursor(0, y);
    display.println("TU DONG:");
    y += 16;
    static int autoScrollOffset = 0;
    if (autoMenuSelection < autoScrollOffset)
      autoScrollOffset = autoMenuSelection;
    if (autoMenuSelection >= autoScrollOffset + maxVisibleAuto)
      autoScrollOffset = autoMenuSelection - maxVisibleAuto + 1;
    String options[4] = {"AUTO ON", "AUTO OFF", "RESET", "CHINH GIO"};
    for (int i = autoScrollOffset; i < 4 && i < autoScrollOffset + maxVisibleAuto; i++)
    {
      display.setCursor(0, y);
      display.print(i == autoMenuSelection ? ">" : " ");
      display.println(options[i]);
      y += 16;
    }
    break;
  }

  case AUTO_TIME_MENU:
  {
    int y = 0;
    display.setCursor(0, y);
    display.println("CHINH GIO:");
    bool blink = ((millis() / 500) % 2) == 0;
    y += 16;
    // Nhóm FROM
    display.setCursor(0, y);
    display.print(autoTimeSelectField == 0 ? ">" : " ");
    display.print("[");
    if (autoTimeSelectField == 0 && autoTimeEditPhase == 0 && blink)
      display.print("  ");
    else
    {
      if (autoStartHourTemp < 10)
        display.print("0");
      display.print(autoStartHourTemp);
    }
    display.print(":");
    if (autoTimeSelectField == 0 && autoTimeEditPhase == 1 && blink)
      display.print("  ");
    else
    {
      if (autoStartMinTemp < 10)
        display.print("0");
      display.print(autoStartMinTemp);
    }
    display.print("] - ");
    // Nhóm TO
    display.print(autoTimeSelectField == 1 ? ">" : " ");
    display.print("[");
    if (autoTimeSelectField == 1 && autoTimeEditPhase == 0 && blink)
      display.print("  ");
    else
    {
      if (autoEndHourTemp < 10)
        display.print("0");
      display.print(autoEndHourTemp);
    }
    display.print(":");
    if (autoTimeSelectField == 1 && autoTimeEditPhase == 1 && blink)
      display.print("  ");
    else
    {
      if (autoEndMinTemp < 10)
        display.print("0");
      display.print(autoEndMinTemp);
    }
    display.print("] - ");
    y += 16;
    // Nhóm DUR
    display.setCursor(0, y);
    display.print(autoTimeSelectField == 2 ? ">" : " ");
    display.print("[");
    if (autoTimeSelectField == 2 && autoTimeEditPhase == 0 && blink)
      display.print("  ");
    else
    {
      if (autoDurHourTemp < 10)
        display.print("0");
      display.print(autoDurHourTemp);
    }
    display.print(":");
    if (autoTimeSelectField == 2 && autoTimeEditPhase == 1 && blink)
      display.print("  ");
    else
    {
      if (autoDurMinTemp < 10)
        display.print("0");
      display.print(autoDurMinTemp);
    }
    display.println("]");
    display.setCursor(0, SCREEN_HEIGHT - 16);
    display.println("*: Cancel  #: Save");
    break;
  }

  case RUNNING:
  {
    if (transitionActive)
    {
      int nextSprinkler = runSprinklerIndices[currentRunIndex + 1];
      unsigned long transElapsed = (millis() - transitionStartTime) / 1000;
      int transRemaining = transitionDelaySeconds - transElapsed;
      if (transRemaining < 0)
        transRemaining = 0;
      int y = 0;
      display.setCursor(0, y);
      display.println("THU CONG");
      y += 16;
      display.setCursor(0, y);
      display.print("BEC ");
      display.print(nextSprinkler + 1);
      display.println(" DANG MO");
      y += 16;
      unsigned long elapsed = (millis() - runStartTime) / 1000;
      unsigned long totalSec = irrigationTime * 60;
      unsigned long remain = (totalSec > elapsed) ? (totalSec - elapsed) : 0;
      display.setCursor(0, y);
      display.print("CON LAI: ");
      display.println(formatRemainingTime(remain));
      y += 16;
      display.setCursor(0, y);
      display.print("BEC ");
      display.print(runSprinklerIndices[currentRunIndex] + 1);
      display.print(" DONG TRONG: ");
      display.print(transRemaining);
      display.println("s");
    }
    else if (paused)
    {
      int y = 0;
      display.setCursor(0, y);
      display.println("THU CONG");
      y += 16;
      display.setCursor(0, y);
      display.print("BEC ");
      display.print(runSprinklerIndices[currentRunIndex] + 1);
      display.println(" DANG DUNG");
      unsigned long remSec = (pauseRemaining / 1000) + 1;
      y += 16;
      display.setCursor(0, y);
      display.print("CON LAI: ");
      display.println(formatRemainingTime(remSec));
    }
    else
    {
      int y = 0;
      display.setCursor(0, y);
      display.println("THU CONG");
      y += 16;
      display.setCursor(0, y);
      if (currentRunIndex < runSprinklerCount)
      {
        display.print("BEC ");
        display.print(runSprinklerIndices[currentRunIndex] + 1);
        display.println(" DANG CHAY");
        unsigned long elapsed = (millis() - runStartTime) / 1000;
        unsigned long totalSec = irrigationTime * 60;
        unsigned long remain = (totalSec > elapsed) ? (totalSec - elapsed) : 0;
        y += 16;
        display.setCursor(0, y);
        display.print("CON LAI: ");
        display.println(formatRemainingTime(remain));
      }
      else
      {
        display.setCursor(0, y);
        display.println("XONG CHU TRINH");
        delay(2000);
        currentMenu = MAIN_MENU;
      }
    }
    break;
  }

  case RUNNING_AUTO:
  {
    if (paused)
    {
      int y = 0;
      display.setCursor(0, y);
      display.println("TU DONG");
      y += 16;
      display.setCursor(0, y);
      display.print("BEC ");
      display.print(runSprinklerIndices[autoCurrentRunIndex] + 1);
      display.println(" DANG DUNG");
      y += 16;
      unsigned long remSec = (pauseRemaining / 1000) + 1;
      display.setCursor(0, y);
      display.print("CON LAI: ");
      display.println(formatRemainingTime(remSec));
    }
    else
    {
      if (autoTransitionActive)
      {
        int nextSprinkler = runSprinklerIndices[autoCurrentRunIndex + 1];
        unsigned long transElapsed = (millis() - autoTransitionStartTime) / 1000;
        int transRemaining = transitionDelaySeconds - transElapsed;
        if (transRemaining < 0)
          transRemaining = 0;
        int y = 0;
        display.setCursor(0, y);
        display.println("TU DONG");
        y += 16;
        display.setCursor(0, y);
        display.print("BEC ");
        display.print(nextSprinkler + 1);
        display.println(" DANG MO");
        y += 16;
        unsigned long elapsed = (millis() - runStartTime) / 1000;
        unsigned long totalSec = ((unsigned long)(autoDurationHour * 60 + autoDurationMinute)) * 60UL;
        unsigned long remain = (totalSec > elapsed) ? (totalSec - elapsed) : 0;
        display.setCursor(0, y);
        display.print("CON LAI: ");
        display.println(formatRemainingTime(remain));
        y += 16;
        display.setCursor(0, y);
        display.print("BEC ");
        display.print(runSprinklerIndices[autoCurrentRunIndex] + 1);
        display.print(" DONG TRONG: ");
        display.print(transRemaining);
        display.println("s");
      }
      else
      {
        int y = 0;
        display.setCursor(0, y);
        display.println("TU DONG");
        y += 16;
        display.setCursor(0, y);
        display.print("BEC ");
        display.print(runSprinklerIndices[autoCurrentRunIndex] + 1);
        display.println(" DANG CHAY");
        y += 16;
        unsigned long elapsed = (millis() - runStartTime) / 1000;
        unsigned long totalSec = ((unsigned long)(autoDurationHour * 60 + autoDurationMinute)) * 60UL;
        unsigned long remain = (totalSec > elapsed) ? (totalSec - elapsed) : 0;
        display.setCursor(0, y);
        display.print("CON LAI: ");
        display.println(formatRemainingTime(remain));
      }
    }
    break;
  }
  }
  display.display();
}

// ===========================================================
// Hàm xử lý IR Remote: đọc tín hiệu, debounce và điều hướng menu
// ===========================================================
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
      String cmd = mapIRCodeToCommand(code);
      Serial.print("IR Command: ");
      Serial.println(cmd);
      lastCode = code;
      lastReceiveTime = currentTime;

      // Xử lý theo từng trạng thái menu:
      if (currentMenu == MAIN_MENU)
      {
        if (cmd == "UP")
          mainMenuSelection = (mainMenuSelection - 1 + 3) % 3;
        else if (cmd == "DOWN")
          mainMenuSelection = (mainMenuSelection + 1) % 3;
        else if (cmd == "OK")
        {
          if (mainMenuSelection == 0)
          {
            for (int i = 0; i < 10; i++)
              backupConfig[i] = allowedSprinklers[i];
            configMode = 0;
            currentMenu = CONFIG_MENU;
          }
          else if (mainMenuSelection == 1)
          {
            tempHour = currentHour;
            tempMinute = currentMinute;
            timeInput = "";
            lastTimeInput = 0;
            currentMenu = TIME_SELECT_MENU;
          }
          else
          { // TU DONG
            currentMenu = AUTO_MENU;
            autoMenuSelection = 0;
            autoScrollOffset = 0;
          }
        }
      }
      else if (currentMenu == CONFIG_MENU)
      {
        if (cmd == "UP")
        {
          if (configMode == 0)
          {
            if (configSelected > 0)
              configSelected--;
          }
          else
          {
            transitionDelayTemp++;
          }
        }
        else if (cmd == "DOWN")
        {
          if (configMode == 0)
          {
            if (configSelected < 9)
              configSelected++;
          }
          else
          {
            if (transitionDelayTemp > 0)
              transitionDelayTemp--;
          }
        }
        else if (cmd == "OK")
        {
          if (configMode == 0)
            allowedSprinklers[configSelected] = !allowedSprinklers[configSelected];
        }
        else if (cmd == "LEFT" || cmd == "RIGHT")
          configMode = (configMode == 0) ? 1 : 0;
        else if (cmd == "STAR")
        {
          for (int i = 0; i < 10; i++)
            allowedSprinklers[i] = backupConfig[i];
          currentMenu = MAIN_MENU;
        }
        else if (cmd == "HASH")
        {
          transitionDelaySeconds = transitionDelayTemp;
          saveConfig();
          currentMenu = MAIN_MENU;
        }
      }
      else if (currentMenu == TIME_SELECT_MENU)
      {
        if (cmd == "UP")
        {
          if (timeSelectHour)
            tempHour += TIME_STEP_HOUR;
          else
          {
            tempMinute += TIME_STEP_MIN;
            if (tempMinute >= 60)
              tempMinute = 59;
          }
        }
        else if (cmd == "DOWN")
        {
          if (timeSelectHour)
          {
            if (tempHour > 0)
              tempHour -= TIME_STEP_HOUR;
          }
          else
          {
            if (tempMinute >= TIME_STEP_MIN)
              tempMinute -= TIME_STEP_MIN;
            else
              tempMinute = 0;
          }
        }
        else if (cmd == "LEFT" || cmd == "RIGHT")
        {
          timeSelectHour = !timeSelectHour;
          timeInput = "";
          lastTimeInput = 0;
        }
        else if (cmd == "OK" || cmd == "HASH")
        {
          if (tempHour == 0 && tempMinute == 0)
          {
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
            currentHour = tempHour;
            currentMinute = tempMinute;
            irrigationTime = currentHour * 60 + currentMinute;
            saveConfig();
            runSprinklerCount = 0;
            for (int i = 0; i < 10; i++)
            {
              if (allowedSprinklers[i])
                runSprinklerIndices[runSprinklerCount++] = i;
            }
            if (runSprinklerCount == 0)
            {
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
              paused = false;
              int bec = runSprinklerIndices[currentRunIndex];
              activatePumpForSprinkler(bec);
            }
          }
        }
        else if (cmd == "STAR")
          currentMenu = MAIN_MENU;
        else if (cmd >= "0" && cmd <= "9")
        {
          unsigned long now = millis();
          if (now - lastTimeInput < 1000)
            timeInput += cmd;
          else
            timeInput = cmd;
          lastTimeInput = now;
          int value = timeInput.toInt();
          if (timeSelectHour)
            tempHour = value;
          else
          {
            tempMinute = value;
            if (tempMinute >= 60)
              tempMinute = 59;
          }
        }
      }
      else if (currentMenu == AUTO_MENU)
      {
        if (cmd == "UP")
          autoMenuSelection = (autoMenuSelection - 1 + 4) % 4;
        else if (cmd == "DOWN")
          autoMenuSelection = (autoMenuSelection + 1) % 4;
        else if (cmd == "OK")
        {
          if (autoMenuSelection == AUTO_TIME_OPT)
          {
            autoTimeSelectField = 0;
            autoTimeEditPhase = 0;
            autoStartHourTemp = autoStartMinute / 60;
            autoStartMinTemp = autoStartMinute % 60;
            autoEndHourTemp = autoEndMinute / 60;
            autoEndMinTemp = autoEndMinute % 60;
            currentMenu = AUTO_TIME_MENU;
          }
          else if (autoMenuSelection == AUTO_ON_OPT)
          {
            autoMode = AUTO_ON;
            saveConfig();
            currentMenu = MAIN_MENU;
          }
          else if (autoMenuSelection == AUTO_OFF_OPT)
          {
            autoMode = AUTO_OFF;
            saveConfig();
            currentMenu = MAIN_MENU;
          }
          else if (autoMenuSelection == AUTO_RESET_OPT)
          {
            autoCurrentSprinkler = 0;
            lastAutoCycleEndEpoch = 0;
            saveConfig();
            currentMenu = MAIN_MENU;
            Serial.println("Auto mode has been reset.");
          }
        }
        else if (cmd == "STAR")
          currentMenu = MAIN_MENU;
      }
      else if (currentMenu == AUTO_TIME_MENU)
      {
        if (cmd == "UP")
        {
          if (autoTimeEditPhase == 0)
          {
            if (autoTimeSelectField == 0)
              autoStartHourTemp = (autoStartHourTemp + 1) % 24;
            else if (autoTimeSelectField == 1)
              autoEndHourTemp = (autoEndHourTemp + 1) % 24;
            else if (autoTimeSelectField == 2)
              autoDurHourTemp = (autoDurHourTemp + 1) % 24;
          }
          else
          {
            if (autoTimeSelectField == 0)
              autoStartMinTemp = (autoStartMinTemp + 1) % 60;
            else if (autoTimeSelectField == 1)
              autoEndMinTemp = (autoEndMinTemp + 1) % 60;
            else if (autoTimeSelectField == 2)
              autoDurMinTemp = (autoDurMinTemp + 1) % 60;
          }
        }
        else if (cmd == "DOWN")
        {
          if (autoTimeEditPhase == 0)
          {
            if (autoTimeSelectField == 0)
              autoStartHourTemp = (autoStartHourTemp - 1 + 24) % 24;
            else if (autoTimeSelectField == 1)
              autoEndHourTemp = (autoEndHourTemp - 1 + 24) % 24;
            else if (autoTimeSelectField == 2)
              autoDurHourTemp = (autoDurHourTemp - 1 + 24) % 24;
          }
          else
          {
            if (autoTimeSelectField == 0)
              autoStartMinTemp = (autoStartMinTemp - 1 + 60) % 60;
            else if (autoTimeSelectField == 1)
              autoEndMinTemp = (autoEndMinTemp - 1 + 60) % 60;
            else if (autoTimeSelectField == 2)
              autoDurMinTemp = (autoDurMinTemp - 1 + 60) % 60;
          }
        }
        else if (cmd == "OK")
        {
          if (autoTimeEditPhase == 0)
            autoTimeEditPhase = 1;
          else
          {
            autoTimeEditPhase = 0;
            if (autoTimeSelectField < 2)
              autoTimeSelectField++;
            else
              autoTimeSelectField = 0;
          }
        }
        else if (cmd == "HASH")
        {
          autoStartMinute = autoStartHourTemp * 60 + autoStartMinTemp;
          autoEndMinute = autoEndHourTemp * 60 + autoEndMinTemp;
          autoDurationHour = autoDurHourTemp;
          autoDurationMinute = autoDurMinTemp;
          saveConfig();
          currentMenu = AUTO_MENU;
        }
        else if (cmd == "LEFT")
        {
          if (autoTimeSelectField > 0)
          {
            autoTimeSelectField--;
            autoTimeEditPhase = 0;
          }
        }
        else if (cmd == "RIGHT")
        {
          if (autoTimeSelectField < 2)
          {
            autoTimeSelectField++;
            autoTimeEditPhase = 0;
          }
        }
        else if (cmd == "STAR")
          currentMenu = AUTO_MENU;
        else if (cmd >= "0" && cmd <= "9")
        {
          unsigned long now = millis();
          if (now - lastAutoTimeInput < 1000)
            autoTimeInput += cmd;
          else
            autoTimeInput = cmd;
          lastAutoTimeInput = now;
          int value = autoTimeInput.toInt();
          if (autoTimeEditPhase == 0)
          {
            if (value >= 0 && value <= 23)
            {
              if (autoTimeSelectField == 0)
                autoStartHourTemp = value;
              else if (autoTimeSelectField == 1)
                autoEndHourTemp = value;
              else if (autoTimeSelectField == 2)
                autoDurHourTemp = value;
            }
          }
          else
          {
            if (value >= 0 && value <= 59)
            {
              if (autoTimeSelectField == 0)
                autoStartMinTemp = value;
              else if (autoTimeSelectField == 1)
                autoEndMinTemp = value;
              else if (autoTimeSelectField == 2)
                autoDurMinTemp = value;
            }
          }
        }
      }
      else if (currentMenu == RUNNING)
      {
        if (cmd == "OK")
        {
          if (!paused)
          {
            paused = true;
            unsigned long cycleDuration = (unsigned long)irrigationTime * 60000UL;
            pauseRemaining = cycleDuration - (millis() - runStartTime);
            for (int i = 0; i < 12; i++)
              digitalWrite(relayPins[i], LOW);
          }
          else
          {
            runStartTime = millis() - ((unsigned long)irrigationTime * 60000UL - pauseRemaining);
            int bec = runSprinklerIndices[currentRunIndex];
            activatePumpForSprinkler(bec);
            paused = false;
          }
        }
        else if (cmd == "STAR")
        {
          for (int i = 0; i < 12; i++)
            digitalWrite(relayPins[i], LOW);
          currentMenu = MAIN_MENU;
          cycleStarted = false;
        }
      }
      else if (currentMenu == RUNNING_AUTO)
      {
        if (cmd == "OK")
        {
          if (!paused)
          {
            paused = true;
            unsigned long cycleDuration = ((unsigned long)(autoDurationHour * 60 + autoDurationMinute)) * 60000UL;
            pauseRemaining = cycleDuration - (millis() - runStartTime);
            for (int i = 0; i < 12; i++)
              digitalWrite(relayPins[i], LOW);
          }
          else
          {
            runStartTime = millis() - (((unsigned long)(autoDurationHour * 60 + autoDurationMinute)) * 60000UL - pauseRemaining);
            int bec = runSprinklerIndices[currentRunIndex];
            activatePumpForSprinkler(bec);
            paused = false;
          }
        }
        else if (cmd == "STAR")
        {
          autoCurrentSprinkler = runSprinklerIndices[currentRunIndex];
          for (int i = 0; i < 12; i++)
            digitalWrite(relayPins[i], LOW);
          autoMode = AUTO_OFF;
          saveConfig();
          currentMenu = MAIN_MENU;
          cycleStarted = false;
        }
      }
    }
    IrReceiver.resume();
  }
}

// ===========================================================
// Hàm cập nhật chu trình RUNNING (chế độ thủ công) - non-blocking
// ===========================================================
void updateRunning()
{
  if (currentMenu != RUNNING || paused)
    return;

  unsigned long cycleDuration = (unsigned long)irrigationTime * 60000UL;
  unsigned long elapsedCycle = millis() - runStartTime;

  if (!transitionActive)
  {
    if (elapsedCycle >= cycleDuration)
    {
      if (currentRunIndex + 1 < runSprinklerCount)
      {
        int nextSprinkler = runSprinklerIndices[currentRunIndex + 1];
        activatePumpForSprinkler(nextSprinkler);
        transitionActive = true;
        transitionStartTime = millis();
      }
      else
      {
        int currentSprinkler = runSprinklerIndices[currentRunIndex];
        turnOffCurrentSprinklerAndPump(currentSprinkler);
        currentMenu = MAIN_MENU;
      }
    }
  }
  else
  {
    unsigned long transElapsed = (millis() - transitionStartTime) / 1000;
    int transRemaining = transitionDelaySeconds - transElapsed;
    if (transRemaining <= 0)
    {
      int currentSprinkler = runSprinklerIndices[currentRunIndex];
      digitalWrite(relayPins[currentSprinkler], LOW);
      currentRunIndex++;
      runStartTime = millis();
      transitionActive = false;
    }
  }
}

// ===========================================================
// Hàm cập nhật chu trình RUNNING_AUTO (chế độ tự động) - non-blocking
// ===========================================================
void updateRunningAuto()
{
  if (currentMenu != RUNNING_AUTO || paused)
    return;

  unsigned long cycleDuration = ((unsigned long)(autoDurationHour * 60 + autoDurationMinute)) * 60000UL;
  unsigned long elapsedCycle = millis() - runStartTime;

  if (!autoTransitionActive)
  {
    if (elapsedCycle >= cycleDuration)
    {
      if (autoCurrentRunIndex + 1 < autoRunSprinklerCount)
      {
        int nextSprinkler = runSprinklerIndices[autoCurrentRunIndex + 1];
        activatePumpForSprinkler(nextSprinkler);
        autoTransitionActive = true;
        autoTransitionStartTime = millis();
      }
      else
      {
        int currentSprinkler = runSprinklerIndices[autoCurrentRunIndex];
        turnOffCurrentSprinklerAndPump(currentSprinkler);
        autoCurrentSprinkler = (autoCurrentSprinkler + autoRunSprinklerCount) % 10;
        ntpClient.update();
        lastAutoCycleEndEpoch = ntpClient.getEpochTime();
        saveConfig();
        currentMenu = MAIN_MENU;
      }
    }
  }
  else
  {
    unsigned long transElapsed = (millis() - autoTransitionStartTime) / 1000;
    int transRemaining = transitionDelaySeconds - transElapsed;
    if (transRemaining <= 0)
    {
      int currentSprinkler = runSprinklerIndices[autoCurrentRunIndex];
      digitalWrite(relayPins[currentSprinkler], LOW);
      autoCurrentRunIndex++;
      runStartTime = millis();
      autoTransitionActive = false;
    }
  }
}

// ===========================================================
// Hàm kiểm tra Auto Mode dựa theo thời gian thực (NTP)
// ===========================================================
void checkAutoMode()
{
  int currentHourRT = ntpClient.getHours();
  int currentMinuteRT = ntpClient.getMinutes();
  int currentTimeMinutes = currentHourRT * 60 + currentMinuteRT;
  unsigned long currentEpoch = ntpClient.getEpochTime();

  bool skipCycle = false;
  if (lastAutoCycleEndEpoch != 0 && autoCurrentSprinkler == 0)
    skipCycle = (currentEpoch - lastAutoCycleEndEpoch < (2UL * 60UL));
  else
    lastAutoCycleEndEpoch = 0;

  bool waterDay = !skipCycle;
  bool inAutoTime = false;
  if (autoStartMinute <= autoEndMinute)
    inAutoTime = (currentTimeMinutes >= autoStartMinute && currentTimeMinutes < autoEndMinute);
  else
    inAutoTime = (currentTimeMinutes >= autoStartMinute || currentTimeMinutes < autoEndMinute);

  if (millis() - lastDebugTime >= debugInterval)
  {
    debugPrintAutoMode(currentEpoch, lastAutoCycleEndEpoch, autoCurrentSprinkler, currentEpoch - lastAutoCycleEndEpoch, skipCycle);
    lastDebugTime = millis();
  }

  if (autoMode == AUTO_ON && waterDay && inAutoTime && currentMenu != RUNNING_AUTO)
  {
    int availableMinutes = (autoStartMinute <= autoEndMinute)
                               ? (autoEndMinute - autoStartMinute)
                               : ((1440 - autoStartMinute) + autoEndMinute);

    int durMinutes = autoDurationHour * 60 + autoDurationMinute;
    if (durMinutes <= 0)
      durMinutes = 1;

    int sprinklersPerNight = availableMinutes / durMinutes;
    if (sprinklersPerNight < 1)
      sprinklersPerNight = 1;

    autoRunSprinklerCount = (sprinklersPerNight > 10) ? 10 : sprinklersPerNight;
    for (int i = 0; i < autoRunSprinklerCount; i++)
      runSprinklerIndices[i] = (autoCurrentSprinkler + i) % 10;

    currentMenu = RUNNING_AUTO;
    autoCurrentRunIndex = 0;
    runStartTime = millis();
    paused = false;
    activatePumpForSprinkler(runSprinklerIndices[autoCurrentRunIndex]);
  }
}

// ===========================================================
// Hàm lưu & tải cấu hình sử dụng Preferences (flash)
// ===========================================================
void saveConfig()
{
  preferences.begin("config", false);
  for (int i = 0; i < 10; i++)
  {
    String key = "sprayer" + String(i);
    preferences.putBool(key.c_str(), allowedSprinklers[i]);
  }
  preferences.putInt("delay", transitionDelaySeconds);
  preferences.putUInt("irrigationTime", irrigationTime);
  preferences.putInt("autoSprinkler", autoCurrentSprinkler);
  preferences.putInt("autoStart", autoStartMinute);
  preferences.putInt("autoEnd", autoEndMinute);
  int totalAutoDur = autoDurationHour * 60 + autoDurationMinute;
  preferences.putInt("autoDur", totalAutoDur);
  preferences.putInt("autoMode", (int)autoMode);
  preferences.putUInt("lastAutoCycleEndEpoch", lastAutoCycleEndEpoch);
  preferences.putInt("autoCurrentRunIndex", autoCurrentRunIndex);
  preferences.putInt("autoRunSprinklerCount", autoRunSprinklerCount);
  preferences.end();
  Serial.println("Config saved.");
}

void loadConfig()
{
  preferences.begin("config", true);
  for (int i = 0; i < 10; i++)
  {
    String key = "sprayer" + String(i);
    allowedSprinklers[i] = preferences.getBool(key.c_str(), false);
  }
  transitionDelaySeconds = preferences.getInt("delay", 10);
  transitionDelayTemp = transitionDelaySeconds;
  irrigationTime = preferences.getUInt("irrigationTime", 120);
  autoCurrentSprinkler = preferences.getInt("autoSprinkler", 0);
  autoStartMinute = preferences.getInt("autoStart", 1320);
  autoStartHourTemp = autoStartMinute / 60;
  autoStartMinTemp = autoStartMinute % 60;
  autoEndMinute = preferences.getInt("autoEnd", 240);
  autoEndHourTemp = autoEndMinute / 60;
  autoEndMinTemp = autoEndMinute % 60;
  int totalAutoDur = preferences.getInt("autoDur", 180);
  autoDurationHour = totalAutoDur / 60;
  autoDurationMinute = totalAutoDur % 60;
  autoDurHourTemp = autoDurationHour;
  autoDurMinTemp = autoDurationMinute;
  autoMode = (AutoMode)preferences.getInt("autoMode", AUTO_OFF);
  lastAutoCycleEndEpoch = preferences.getUInt("lastAutoCycleEndEpoch", 0);
  autoCurrentRunIndex = preferences.getInt("autoCurrentRunIndex", 0);
  autoRunSprinklerCount = preferences.getInt("autoRunSprinklerCount", 0);

  currentHour = irrigationTime / 60;
  currentMinute = irrigationTime % 60;
  preferences.end();
  Serial.println("Config loaded.");
}

// ===========================================================
// Hàm kiểm tra bảo vệ bơm và đồng bộ trạng thái relay với Blynk
// ===========================================================
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
      Blynk.logEvent("pump_protect", "Khong co bec. Tat bom");
      digitalWrite(relayPins[10], LOW);
      digitalWrite(relayPins[11], LOW);
      Serial.println("Pump protection triggered.");
    }
  }
  else
  {
    lastPumpProtectionTriggered = false;
  }
}

void syncRelayStatusToBlynk()
{
  static int lastStates[12] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
  for (int i = 0; i < 12; i++)
  {
    int cur = digitalRead(relayPins[i]);
    if (cur != lastStates[i])
    {
      lastStates[i] = cur;
      if (i < 10)
        Blynk.virtualWrite(i + 1, cur);
      else if (i == 10)
        Blynk.virtualWrite(11, cur);
      else if (i == 11)
        Blynk.virtualWrite(12, cur);
      Serial.print("Relay ");
      Serial.print(i);
      Serial.print(" -> ");
      Serial.println(cur);
    }
  }
}

// ===========================================================
// Hàm kiểm tra nguồn điện qua ACS712 và tự động pause/resume
// ===========================================================
// Nếu dòng điện đo được dưới 100 mA => mất điện
bool isPowerAvailable()
{
  static const int numReadings = 10;      // Số lần đọc dùng để lấy trung bình
  static int readings[numReadings] = {0}; // Mảng lưu các giá trị đọc
  static int readIndex = 0;               // Chỉ số đọc hiện tại
  static long total = 0;                  // Tổng của các giá trị đọc

  // Đọc giá trị mA từ ACS712
  int mA = ACS.mA_AC_sampling();

  // Cập nhật tổng: trừ giá trị cũ, thêm giá trị mới vào mảng
  total = total - readings[readIndex];
  readings[readIndex] = mA;
  total = total + readings[readIndex];

  // Cập nhật chỉ số đọc (vòng lặp)
  readIndex = (readIndex + 1) % numReadings;

  // Tính giá trị trung bình
  int averageCurrent = total / numReadings;
  Serial.print("Dòng điện trung bình (mA): ");
  Serial.println(averageCurrent);

  if (averageCurrent > 100)
  { // Nếu trung bình trên 100 mA, coi như có điện
    Serial.println("Có điện - Dòng điện AC đang chạy qua cảm biến");
    return true;
  }
  else
  {
    Serial.println("Mất điện - Không có dòng điện qua cảm biến");
    return false;
  }
}

// Phiên bản handlePowerMonitoring mới: khi điện khôi phục, nếu đang ở chế độ RUNNING hoặc RUNNING_AUTO và hệ thống đang pause do mất điện, tự động resume ngay lập tức.
void handlePowerMonitoring()
{
  // Lưu trạng thái nguồn của lần gọi trước (ban đầu giả sử có điện)
  static bool lastPowerState = true;

  // Lấy trạng thái nguồn hiện tại (true: có điện, false: mất điện)
  bool currentPowerState = isPowerAvailable();

  // Nếu trạng thái thay đổi so với lần gọi trước, in ra kết quả và cập nhật lại lastPowerState
  if (currentPowerState != lastPowerState)
  {
    Serial.print("Power state changed: ");
    Serial.println(currentPowerState ? "Available" : "Not available");
    lastPowerState = currentPowerState;
  }

  // Nếu có điện và chương trình đang ở chế độ running (manual hoặc auto) và đã bị tạm dừng do mất điện, resume ngay
  if (currentPowerState && powerPaused && paused &&
      (currentMenu == RUNNING || currentMenu == RUNNING_AUTO))
  {
    if (currentMenu == RUNNING)
    {
      runStartTime = millis() - ((unsigned long)irrigationTime * 60000UL - pauseRemaining);
      int bec = runSprinklerIndices[currentRunIndex];
      // activatePumpForSprinkler(bec);
    }
    else if (currentMenu == RUNNING_AUTO)
    {
      runStartTime = millis() - (((unsigned long)(autoDurationHour * 60 + autoDurationMinute)) * 60000UL - pauseRemaining);
      int bec = runSprinklerIndices[currentRunIndex];
      // activatePumpForSprinkler(bec);
    }
    paused = false;
    powerPaused = false;
    Serial.println("Resumed after power restored.");
  }

  // Nếu mất điện và chương trình đang ở chế độ running (manual hoặc auto) và chưa bị tạm dừng, thực hiện pause
  if (!currentPowerState && !paused &&
      (currentMenu == RUNNING || currentMenu == RUNNING_AUTO))
  {
    paused = true;
    powerPaused = true;
    if (currentMenu == RUNNING)
    {
      unsigned long cycleDuration = (unsigned long)irrigationTime * 60000UL;
      pauseRemaining = cycleDuration - (millis() - runStartTime);
    }
    else if (currentMenu == RUNNING_AUTO)
    {
      unsigned long cycleDuration = ((unsigned long)(autoDurationHour * 60 + autoDurationMinute)) * 60000UL;
      pauseRemaining = cycleDuration - (millis() - runStartTime);
    }
    // Tắt toàn bộ relay
    for (int i = 0; i < 12; i++)
    {
      digitalWrite(relayPins[i], LOW);
    }
    Serial.println("Paused due to power loss.");
  }
}

// ===========================================================
// Blynk Timer và hàm setup, loop chính
// ===========================================================
BlynkTimer timer;

void setup()
{
  Serial.begin(115200);

  // Hiệu chuẩn ACS712 ngay khi khởi động
  calibrateACS712();

  // Khởi tạo các chân Relay (OUTPUT) và tắt tất cả
  for (int i = 0; i < 12; i++)
  {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], LOW);
  }
  // Khởi tạo trạng thái cho các béc tưới (mặc định false)
  for (int i = 0; i < 10; i++)
    allowedSprinklers[i] = false;

  // Khởi tạo giao tiếp I2C cho OLED (chân 32: SDA, 33: SCL)
  Wire.begin(32, 33);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  {
    Serial.println("OLED SSD1306 không khởi tạo được");
    while (true)
      ;
  }
  display.clearDisplay();
  display.display();

  // Tải cấu hình từ bộ nhớ flash
  loadConfig();

  // Kết nối Blynk
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  Serial.println("ESP32 connected to Blynk!");

  // Khởi tạo IR Remote và NTP
  setupIR();
  ntpClient.begin();
  ntpClient.update();

  // Cài đặt các khoảng thời gian gọi hàm (timer)
  timer.setInterval(500L, updateMenuDisplay); // Cập nhật màn hình OLED mỗi 500ms
  timer.setInterval(30000L, []()
                    { ntpClient.update(); }); // Cập nhật NTP mỗi 30 giây
}

void loop()
{
  Blynk.run();
  timer.run();
  processIRRemote(); // Xử lý tín hiệu IR Remote
  if (currentMenu == RUNNING)
    updateRunning(); // Cập nhật chu trình tưới thủ công
  else if (currentMenu == RUNNING_AUTO)
    updateRunningAuto();    // Cập nhật chu trình tưới tự động
  checkPumpProtection();    // Kiểm tra bảo vệ bơm
  syncRelayStatusToBlynk(); // Đồng bộ trạng thái relay với Blynk
  checkAutoMode();          // Kiểm tra và kích hoạt Auto Mode nếu cần

  // Kiểm tra nguồn điện qua ACS712, tự động pause/resume
  handlePowerMonitoring();
}
