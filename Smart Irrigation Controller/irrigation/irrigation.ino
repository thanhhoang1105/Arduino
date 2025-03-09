// acc thanhhoangngoc.bmt1105@gmail.com
// ======================================
// Định nghĩa thông tin Blynk và thư viện
// ======================================
#define BLYNK_TEMPLATE_ID "TMPL6ZR2keF5J"
#define BLYNK_TEMPLATE_NAME "Smart Irrigation Controller"
#define BLYNK_AUTH_TOKEN "x-Uc3uA1wCZtbDWniO6yWTDrLHsVqOJ-"

#include <Preferences.h>
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <IRremote.hpp>
#include <time.h>
#include <WiFiUdp.h>
#include <NTPClient.h>

// =========================
// Đối tượng & cấu hình
// =========================
Preferences preferences;

WiFiUDP ntpUDP;
NTPClient ntpClient(ntpUDP, "pool.ntp.org", 7 * 3600, 60000); // Múi giờ +7, cập nhật mỗi 60 giây

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

char ssid[] = "VU ne";
char pass[] = "12341234";

// Relay: 10 béc tưới, 2 bơm (relayPins[10] và relayPins[11])
const int relayPins[12] = {2, 4, 16, 17, 5, 18, 19, 21, 22, 23, 26, 25};

// =========================
// IR Remote Setup
// =========================
const int RECV_PIN = 15;
void setupIR()
{
  IrReceiver.begin(RECV_PIN, ENABLE_LED_FEEDBACK);
  Serial.println("IR Receiver đang chờ tín hiệu...");
}

// IR Codes
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

// Debounce cho IR
unsigned long lastCode = 0;
unsigned long lastReceiveTime = 0;
const unsigned long debounceTime = 500;

// =========================
// Các biến global cho MENU & TIME
// =========================
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
int mainMenuSelection = 0; // 0: CAU HINH, 1: KHỞI ĐỘNG THỦ CÔNG, 2: TU ĐỘNG

// CONFIG_MENU
int configMode = 0; // 0: Béc tưới, 1: Delay
bool allowedSprinklers[10];
bool backupConfig[10];
int configSelected = 0;
int configScrollOffset = 0;
const int maxVisibleList = 2;
int transitionDelaySeconds = 10;
int transitionDelayTemp = 10;

// TIME_SELECT_MENU (manual)
unsigned int irrigationTime = 120; // 120 phút mặc định
#define TIME_STEP_HOUR 1
#define TIME_STEP_MIN 15
int currentHour = irrigationTime / 60;
int currentMinute = irrigationTime % 60;
bool timeSelectHour = true;
int tempHour = currentHour;
int tempMinute = currentMinute;
String timeInput = "";
unsigned long lastTimeInput = 0;

// RUNNING (manual)
unsigned long runStartTime = 0;
int runSprinklerIndices[10];
int runSprinklerCount = 0;
int currentRunIndex = 0;
bool cycleStarted = false;
bool paused = false;
unsigned long pauseRemaining = 0;
bool transitionActive = false;
unsigned long transitionStartTime = 0;

// AUTO_RUN (auto)
bool autoTransitionActive = false;
unsigned long autoTransitionStartTime = 0;
// Các biến cho chế độ RUNNING_AUTO (auto mode)
int autoCurrentRunIndex = 0;   // Chỉ số béc hiện hành trong chế độ tự động
int autoRunSprinklerCount = 0; // Số lượng béc được tưới trong chu trình auto
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
enum AutoMode
{
  AUTO_ON,
  AUTO_OFF
};
AutoMode autoMode = AUTO_OFF;
int autoCurrentSprinkler = 0;

// Cấu hình auto mode (theo số phút)
int autoStartMinute = 1320; // 22:00
int autoEndMinute = 240;    // 4:00
int autoDurationHour = 3;
int autoDurationMinute = 0;

// =========================
// Các biến mới cho chế độ AUTO_TIME_MENU theo yêu cầu:
// Chỉnh sửa 3 nhóm: FROM, TO, DUR
// Mỗi nhóm hiển thị dạng [HH:MM]
int autoTimeSelectField = 0; // 0: FROM, 1: TO, 2: DUR
// Biến để phân biệt giữa chỉnh giờ (phase 0) và chỉnh phút (phase 1)
int autoTimeEditPhase = 0;

// Các biến tạm cho FROM và TO (tách riêng giờ và phút)
int autoStartHourTemp = 0, autoStartMinTemp = 0;
int autoEndHourTemp = 0, autoEndMinTemp = 0;
// Dùng autoDurHourTemp, autoDurMinTemp cho DUR (đã có từ code cũ)
int autoDurHourTemp = autoDurationHour;
int autoDurMinTemp = autoDurationMinute;

// Biến tạm nhập số cho chế độ AUTO_TIME_MENU (nếu cần nhập số thay vì sử dụng UP/DOWN)
String autoTimeInput = "";
unsigned long lastAutoTimeInput = 0;

// Thêm biến để lưu thời gian kết thúc chu trình auto (dưới dạng epoch, tính bằng giây)
unsigned long lastAutoCycleEndEpoch = 0;

// Debug time cho checkAutoMode
unsigned long lastDebugTime = 0;
const unsigned long debugInterval = 10000; // 10 giây

// =========================
// Các hàm helper dùng chung
// =========================

// Hàm chuyển đổi số phút thành định dạng HH:MM
String formatTime(int minutes)
{
  int hour = minutes / 60;
  int min = minutes % 60;
  char timeStr[6];
  sprintf(timeStr, "%d:%02d", hour, min);
  return String(timeStr);
}

// Hàm chuyển đổi số giây thành chuỗi định dạng HH:MM:SS
String formatRemainingTime(unsigned long seconds)
{
  unsigned int h = seconds / 3600;
  unsigned int m = (seconds % 3600) / 60;
  unsigned int s = seconds % 60;
  char buf[9];
  sprintf(buf, "%02u:%02u:%02u", h, m, s);
  return String(buf);
}

// Hàm ánh xạ IR Code sang chuỗi command
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

// Hàm bật bơm theo béc tưới
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

// Hàm tắt béc tưới và bơm
void turnOffCurrentSprinklerAndPump(int sprinklerIndex)
{
  digitalWrite(relayPins[sprinklerIndex], LOW);
  digitalWrite(relayPins[10], LOW);
  digitalWrite(relayPins[11], LOW);
}

// Hàm debug in thông tin auto mode (có thể tắt khi không cần debug)
void debugPrintAutoMode(unsigned long currentEpoch, unsigned long lastEpoch, int currentSprinkler, unsigned long diff, bool skipCycle)
{
  Serial.println("----- Auto Mode Debug Info -----");
  Serial.print("currentEpoch: ");
  Serial.println(currentEpoch);
  Serial.print("lastAutoCycleEndEpoch: ");
  Serial.println(lastEpoch);
  Serial.print("autoCurrentSprinkler: ");
  Serial.println(currentSprinkler);
  Serial.print("Time since last cycle end (sec): ");
  Serial.println(diff);
  Serial.print("skipCycle: ");
  Serial.println(skipCycle ? "true" : "false");
  Serial.println("--------------------------------");
}

// =========================
// Hàm hiển thị MENU trên OLED
// =========================
void updateMenuDisplay()
{
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  int y = 0;

  // Hiển thị trạng thái kết nối Blynk ở góc trên bên phải
  display.setCursor(110, y);
  display.println(Blynk.connected() ? "ON" : "OFF");

  switch (currentMenu)
  {
  case MAIN_MENU:
    // 3 mục: CAU HINH, KHỞI ĐỘNG THỦ CÔNG, TU ĐỘNG (kèm auto mode info)
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
    const int maxVisibleAuto = 3;
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
    // Chế độ chỉnh sửa thời gian tự động theo định dạng: [HH:MM] - [HH:MM] - [HH:MM]
    // Nhóm 0: FROM, 1: TO, 2: DUR
    int y = 0;
    display.setCursor(0, y);
    display.println("CHINH GIO:");

    // Biến blink: true nếu (millis()/500) là số chẵn, tạo hiệu ứng nhấp nháy
    bool blink = ((millis() / 500) % 2) == 0;
    y += 16;
    display.setCursor(0, y);

    // ---- Nhóm FROM ----
    // In marker cho nhóm FROM (nếu đang chọn)
    if (autoTimeSelectField == 0)
      display.print(">");
    else
      display.print(" ");

    display.print("[");
    // Phần HH của FROM: nếu đang chỉnh giờ (phase 0) và nhóm FROM đang được chọn, hiển thị nhấp nháy
    if (autoTimeSelectField == 0 && autoTimeEditPhase == 0 && blink)
      display.print("  "); // Hiển thị khoảng trắng thay cho số
    else
    {
      if (autoStartHourTemp < 10)
        display.print("0");
      display.print(autoStartHourTemp);
    }
    display.print(":");
    // Phần MM của FROM: nếu đang chỉnh phút (phase 1) và nhóm FROM đang được chọn, hiển thị nhấp nháy
    if (autoTimeSelectField == 0 && autoTimeEditPhase == 1 && blink)
      display.print("  ");
    else
    {
      if (autoStartMinTemp < 10)
        display.print("0");
      display.print(autoStartMinTemp);
    }
    display.print("] - ");

    // ---- Nhóm TO ----
    if (autoTimeSelectField == 1)
      display.print(">");
    else
      display.print(" ");

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

    // ---- Nhóm DUR ----
    y += 16;
    display.setCursor(0, y);
    if (autoTimeSelectField == 2)
      display.print(">");
    else
      display.print(" ");

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

    // Hướng dẫn thao tác
    display.setCursor(0, SCREEN_HEIGHT - 16);
    display.println("*: Cancel  #: Save");
    break;
  }

  case RUNNING:
  {
    // Chế độ thủ công
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
    // Chế độ tự động
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

// =========================
// Hàm xử lý IR Remote
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
      String cmd = mapIRCodeToCommand(code);
      Serial.print("IR Command: ");
      Serial.println(cmd);
      lastCode = code;
      lastReceiveTime = currentTime;

      // Xử lý theo trạng thái menu
      if (currentMenu == MAIN_MENU)
      {
        if (cmd == "UP")
        {
          mainMenuSelection = (mainMenuSelection - 1 + 3) % 3;
        }
        else if (cmd == "DOWN")
        {
          mainMenuSelection = (mainMenuSelection + 1) % 3;
        }
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
          {
            allowedSprinklers[configSelected] = !allowedSprinklers[configSelected];
          }
        }
        else if (cmd == "LEFT" || cmd == "RIGHT")
        {
          configMode = (configMode == 0) ? 1 : 0;
        }
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
        {
          currentMenu = MAIN_MENU;
        }
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
        {
          autoMenuSelection = (autoMenuSelection - 1 + 4) % 4;
        }
        else if (cmd == "DOWN")
        {
          autoMenuSelection = (autoMenuSelection + 1) % 4;
        }
        else if (cmd == "OK")
        {
          if (autoMenuSelection == AUTO_TIME_OPT)
          {
            // Khi chuyển sang AUTO_TIME_MENU, khởi tạo các giá trị tạm từ ROM
            autoTimeSelectField = 0;
            autoTimeEditPhase = 0;
            autoStartHourTemp = autoStartMinute / 60;
            autoStartMinTemp = autoStartMinute % 60;
            autoEndHourTemp = autoEndMinute / 60;
            autoEndMinTemp = autoEndMinute % 60;
            // DÙNG autoDurHourTemp và autoDurMinTemp đã có
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
        {
          currentMenu = MAIN_MENU;
        }
      }
      else if (currentMenu == AUTO_TIME_MENU)
      {
        // Chế độ chỉnh sửa thời gian tự động theo định dạng: [HH:MM] - [HH:MM] - [HH:MM]
        // Nhóm 0: FROM, 1: TO, 2: DUR
        if (cmd == "UP")
        {
          if (autoTimeEditPhase == 0)
          { // Đang chỉnh giờ
            if (autoTimeSelectField == 0)
              autoStartHourTemp = (autoStartHourTemp + 1) % 24;
            else if (autoTimeSelectField == 1)
              autoEndHourTemp = (autoEndHourTemp + 1) % 24;
            else if (autoTimeSelectField == 2)
              autoDurHourTemp = (autoDurHourTemp + 1) % 24;
          }
          else
          { // Đang chỉnh phút
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
          // Nhấn OK: nếu đang chỉnh giờ thì chuyển sang chỉnh phút,
          // nếu đang chỉnh phút thì chuyển sang nhóm tiếp theo (không lưu cấu hình)
          if (autoTimeEditPhase == 0)
          {
            autoTimeEditPhase = 1; // Chuyển sang chỉnh phút của nhóm hiện tại
          }
          else
          {
            // Đã chỉnh xong phút của nhóm hiện tại, reset phase về 0
            autoTimeEditPhase = 0;
            if (autoTimeSelectField < 2)
            {
              // Chuyển sang nhóm tiếp theo
              autoTimeSelectField++;
            }
            else
            {
              // Nếu đã ở nhóm DUR, nhấn OK chỉ chuyển về nhóm đầu tiên (không lưu)
              autoTimeSelectField = 0;
            }
          }
        }
        else if (cmd == "HASH")
        {
          // Nhấn HASH: Lưu cấu hình ngay tức thì
          autoStartMinute = autoStartHourTemp * 60 + autoStartMinTemp;
          autoEndMinute = autoEndHourTemp * 60 + autoEndMinTemp;
          // Sửa lỗi: Cập nhật luôn DUR khi lưu
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
        { // Hủy chỉnh sửa
          currentMenu = AUTO_MENU;
        }
        else if (cmd >= "0" && cmd <= "9")
        {
          // Cho phép nhập số để chỉnh nhanh
          unsigned long now = millis();
          if (now - lastAutoTimeInput < 1000)
            autoTimeInput += cmd;
          else
            autoTimeInput = cmd;
          lastAutoTimeInput = now;
          int value = autoTimeInput.toInt();
          if (autoTimeEditPhase == 0)
          { // Đang chỉnh giờ: giá trị hợp lệ từ 0 đến 23
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
          { // Đang chỉnh phút: giá trị hợp lệ từ 0 đến 59
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
            digitalWrite(relayPins[10], LOW);
            digitalWrite(relayPins[11], LOW);
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
            digitalWrite(relayPins[10], LOW);
            digitalWrite(relayPins[11], LOW);
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

// =========================
// Hàm cập nhật chu trình RUNNING (manual mode, non-blocking transition)
// =========================
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

// =========================
// Hàm cập nhật chu trình RUNNING_AUTO (auto mode, non-blocking transition)
// =========================
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
        // Chưa đến béc cuối cùng:
        int nextSprinkler = runSprinklerIndices[autoCurrentRunIndex + 1];
        activatePumpForSprinkler(nextSprinkler);
        // Bắt đầu giai đoạn chuyển tiếp (transition)
        autoTransitionActive = true;
        autoTransitionStartTime = millis();
      }
      else
      {
        // Đã chạy hết chu trình, tắt béc cuối cùng (và bơm)
        int currentSprinkler = runSprinklerIndices[autoCurrentRunIndex];
        turnOffCurrentSprinklerAndPump(currentSprinkler);
        // Cập nhật autoCurrentSprinkler theo vòng xoay của 10 béc
        autoCurrentSprinkler = (autoCurrentSprinkler + autoRunSprinklerCount) % 10;
        // Cập nhật NTP trước khi lấy epoch
        ntpClient.update();
        lastAutoCycleEndEpoch = ntpClient.getEpochTime();
        saveConfig();
        currentMenu = MAIN_MENU;
      }
    }
  }
  else
  {
    // Đang ở trạng thái transition, chờ delay kết thúc
    unsigned long transElapsed = (millis() - autoTransitionStartTime) / 1000;
    int transRemaining = transitionDelaySeconds - transElapsed;
    if (transRemaining <= 0)
    {
      // Đã hết delay, tắt béc cũ (có thể tắt bơm cùng lúc nếu cần)
      int currentSprinkler = runSprinklerIndices[autoCurrentRunIndex];
      // Tắt béc hiện hành (chỉ tắt béc hiện hành, không ảnh hưởng đến béc mới đã bật)
      digitalWrite(relayPins[currentSprinkler], LOW);

      // Tiếp tục chuyển sang béc tiếp theo nếu có
      autoCurrentRunIndex++;
      runStartTime = millis();
      autoTransitionActive = false;
    }
  }
}

// =========================
// Hàm kiểm tra Auto Mode theo thời gian thực
// =========================
void checkAutoMode()
{
  int currentHourRT = ntpClient.getHours();
  int currentMinuteRT = ntpClient.getMinutes();
  int currentTimeMinutes = currentHourRT * 60 + currentMinuteRT;

  // Lấy thời gian hiện tại theo epoch (giây)
  unsigned long currentEpoch = ntpClient.getEpochTime();

  // Chỉ tính skipCycle nếu đã hoàn thành một chu trình đầy đủ (autoCurrentSprinkler == 0)
  // và lastAutoCycleEndEpoch đã được thiết lập (khác 0)
  bool skipCycle = false;
  if (lastAutoCycleEndEpoch != 0 && autoCurrentSprinkler == 0)
  {
    if (currentEpoch - lastAutoCycleEndEpoch < (2UL * 60UL))
      skipCycle = true;
    else
      skipCycle = false;
  }
  // Nếu điều kiện không thỏa, thì reset lastAutoCycleEndEpoch (cho chu trình auto mới)
  else
  {
    lastAutoCycleEndEpoch = 0;
  }

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

  // Nếu chưa ở chế độ RUNNING_AUTO và đang trong khung giờ, kích hoạt chu trình
  if (autoMode == AUTO_ON && waterDay && inAutoTime && currentMenu != RUNNING_AUTO)
  {
    // Tính availableMinutes dựa trên FROM–TO
    int availableMinutes = 0;
    if (autoStartMinute <= autoEndMinute)
      availableMinutes = autoEndMinute - autoStartMinute;
    else
      availableMinutes = (1440 - autoStartMinute) + autoEndMinute;

    int durMinutes = autoDurationHour * 60 + autoDurationMinute;
    if (durMinutes <= 0)
      durMinutes = 1;

    // Tính số béc dự kiến chạy trong đêm này
    int sprinklersPerNight = availableMinutes / durMinutes;
    if (sprinklersPerNight < 1)
      sprinklersPerNight = 1;

    // Ở môi trường sản xuất, bạn sẽ luôn chạy chu trình 10 béc (vòng xoay)
    // Trong mỗi đêm, autoRunSprinklerCount = sprinklersPerNight, và autoCurrentSprinkler được cập nhật dần
    autoRunSprinklerCount = sprinklersPerNight;
    if (autoRunSprinklerCount > 10)
      autoRunSprinklerCount = 10;

    // Xây dựng danh sách thứ tự béc dựa trên autoCurrentSprinkler
    for (int i = 0; i < autoRunSprinklerCount; i++)
      runSprinklerIndices[i] = (autoCurrentSprinkler + i) % 10;

    currentMenu = RUNNING_AUTO;
    autoCurrentRunIndex = 0;
    runStartTime = millis();
    paused = false;
    // Kích hoạt béc đầu tiên và bơm theo logic
    activatePumpForSprinkler(runSprinklerIndices[autoCurrentRunIndex]);
  }
}

// =========================
// Lưu & tải cấu hình (chỉnh sửa để thống nhất key lưu auto)
// =========================
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
  // Lưu các giá trị auto (FROM, TO, DUR)
  preferences.putInt("autoStart", autoStartMinute);
  preferences.putInt("autoEnd", autoEndMinute);
  int totalAutoDur = autoDurationHour * 60 + autoDurationMinute;
  preferences.putInt("autoDur", totalAutoDur);
  // Lưu trạng thái autoMode (ON/OFF)
  preferences.putInt("autoMode", (int)autoMode);
  // Lưu thời gian kết thúc chu trình auto
  preferences.putUInt("lastAutoCycleEndEpoch", lastAutoCycleEndEpoch);
  // Lưu các biến riêng cho chế độ RUNNING_AUTO:
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
  // Đọc các giá trị auto từ key thống nhất
  autoStartMinute = preferences.getInt("autoStart", 1320);
  autoStartHourTemp = autoStartMinute / 60;
  autoStartMinTemp = autoStartMinute % 60;
  autoEndMinute = preferences.getInt("autoEnd", 240);
  autoEndHourTemp = autoEndMinute / 60;
  autoEndMinTemp = autoEndMinute % 60;
  int totalAutoDur = preferences.getInt("autoDur", 180);
  autoDurationHour = totalAutoDur / 60;
  autoDurationMinute = totalAutoDur % 60;
  // DÙNG autoDurHourTemp và autoDurMinTemp cho DUR
  autoDurHourTemp = autoDurationHour;
  autoDurMinTemp = autoDurationMinute;
  // Đọc trạng thái autoMode (ON/OFF)
  autoMode = (AutoMode)preferences.getInt("autoMode", AUTO_OFF);
  // Đọc thời gian kết thúc chu trình auto từ ROM
  lastAutoCycleEndEpoch = preferences.getUInt("lastAutoCycleEndEpoch", 0);
  // Đọc các biến riêng cho chế độ RUNNING_AUTO
  autoCurrentRunIndex = preferences.getInt("autoCurrentRunIndex", 0);
  autoRunSprinklerCount = preferences.getInt("autoRunSprinklerCount", 0);

  currentHour = irrigationTime / 60;
  currentMinute = irrigationTime % 60;
  preferences.end();
  Serial.println("Config loaded.");
}

// =========================
// Hàm kiểm tra bảo vệ bơm và đồng bộ relay với Blynk
// =========================
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

// =========================
// Blynk Timer và setup, loop
// =========================
BlynkTimer timer;

void setup()
{
  Serial.begin(115200);
  for (int i = 0; i < 12; i++)
  {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], LOW);
  }
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
  loadConfig();
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  Serial.println("ESP32 connected to Blynk!");
  setupIR();
  ntpClient.begin();
  ntpClient.update();

  // Cập nhật màn hình mỗi 500ms
  timer.setInterval(500L, updateMenuDisplay);
  // Cập nhật NTP mỗi 60 giây (không gọi ntpClient.update() ở checkAutoMode)
  timer.setInterval(30000L, []()
                    { ntpClient.update(); });
}

void loop()
{
  Blynk.run();
  timer.run();
  processIRRemote();
  if (currentMenu == RUNNING)
    updateRunning();
  else if (currentMenu == RUNNING_AUTO)
    updateRunningAuto();
  checkPumpProtection();
  syncRelayStatusToBlynk();
  checkAutoMode();
}
