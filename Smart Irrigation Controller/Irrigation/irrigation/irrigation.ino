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

// ------------------ Cấu hình chung ------------------

// WiFi và Blynk
char ssid[] = "VU ne";
char pass[] = "12341234";

// Preferences dùng để lưu cấu hình vào flash
Preferences preferences;

// NTP client (múi giờ +7)
WiFiUDP ntpUDP;
NTPClient ntpClient(ntpUDP, "pool.ntp.org", 7 * 3600, 60000);

// Màn hình OLED SSD1306
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Các chân Relay: 10 béc tưới và 2 bơm (relayPins[10] và relayPins[11])
const int relayPins[12] = {2, 4, 16, 17, 5, 18, 19, 21, 22, 23, 26, 25};

// ------------------ Cấu hình IR Remote ------------------
const int RECV_PIN = 15; // Chân IR receiver
const unsigned long debounceTime = 500;
unsigned long lastCode = 0, lastReceiveTime = 0;

void setupIR()
{
  IrReceiver.begin(RECV_PIN, ENABLE_LED_FEEDBACK);
}

// Định nghĩa mã IR cho các nút
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

// Ánh xạ mã IR sang chuỗi lệnh
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

// ------------------ Các biến trạng thái ------------------

// Menu chính
enum MenuState
{
  MAIN_MENU,
  CONFIG_MENU,
  TIME_SELECT_MENU,
  AUTO_MENU,
  AUTO_CURRENT_SPRINKLER,
  AUTO_TIME_MENU,
  RUNNING,
  RUNNING_AUTO
};
MenuState currentMenu = MAIN_MENU;

// MAIN_MENU
int mainMenuSelection = 0;

// CONFIG_MENU
int configMode = 0; // 0: béc tưới, 1: delay
bool allowedSprinklers[10] = {false};
bool backupConfig[10] = {false};
int configSelected = 0;
int configScrollOffset = 0;
const int maxVisibleList = 2;
int transitionDelaySeconds = 10;
int transitionDelayTemp = 10;

// TIME_SELECT_MENU (chế độ thủ công)
unsigned int irrigationTime = 120; // (phút)
#define TIME_STEP_HOUR 1
#define TIME_STEP_MIN 15
int currentHour = irrigationTime / 60;
int currentMinute = irrigationTime % 60;
bool timeSelectHour = true;
int tempHour = currentHour, tempMinute = currentMinute;
String timeInput = "";
unsigned long lastTimeInput = 0;

// RUNNING (thủ công)
unsigned long runStartTime = 0;
int runSprinklerIndices[10];
int runSprinklerCount = 0;
int currentRunIndex = 0;
bool paused = false;
unsigned long pauseRemaining = 0;
bool transitionActive = false;
unsigned long transitionStartTime = 0;

// AUTO_MENU
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
int autoCurrentSprinkler = 0;     // Đã chạy (để hiển thị ngoài menu)
int autoCurrentSprinklerTemp = 0; // Điều chỉnh qua menu

// Cấu hình thời gian tự động (theo số phút)
int autoStartMinute = 1320; // FROM: 22:00
int autoEndMinute = 240;    // TO: 4:00
int autoDurationHour = 3;   // DUR: 3 giờ
int autoDurationMinute = 0;

// Phân tích chu trình Auto:
// Tính số béc tưới dựa vào khoảng thời gian từ FROM đến TO và DUR (tối đa 10)
int autoRunSprinklerCount = 0;

// Cấu hình thời gian nghỉ giữa các chu trình Auto (REST)
// Ban đầu mặc định 0:05 (5 phút) – cho phép chỉnh theo định dạng [HH:MM]
int autoRestTimeHour = 0, autoRestTimeMinute = 5;

// Để chỉnh sửa thời gian Auto (các trường: FROM, TO, DUR, REST)
int autoTimeSelectField = 0; // 0: FROM, 1: TO, 2: DUR, 3: REST
int autoTimeEditPhase = 0;   // 0: chỉnh giờ, 1: chỉnh phút
// Temp cho FROM, TO, DUR
int autoStartHourTemp = 0, autoStartMinTemp = 0;
int autoEndHourTemp = 0, autoEndMinTemp = 0;
int autoDurHourTemp = autoDurationHour, autoDurMinTemp = autoDurationMinute;
// Temp cho REST
int autoRestHourTemp = autoRestTimeHour, autoRestMinTemp = autoRestTimeMinute;
String autoTimeInput = "";
unsigned long lastAutoTimeInput = 0;
unsigned long lastAutoCycleEndEpoch = 0;

// Biến cho chu trình Auto: đánh dấu đang nghỉ (sau khi tưới xong chu trình)
bool autoCycleResting = false;
unsigned long autoCycleEndTime = 0;

// ------------------ Hàm helper ------------------

String formatTime(int minutes)
{
  int hour = minutes / 60, min = minutes % 60;
  char timeStr[6];
  sprintf(timeStr, "%d:%02d", hour, min);
  return String(timeStr);
}

String formatRemainingTime(unsigned long seconds)
{
  unsigned int h = seconds / 3600, m = (seconds % 3600) / 60, s = seconds % 60;
  char buf[9];
  sprintf(buf, "%02u:%02u:%02u", h, m, s);
  return String(buf);
}

// ------------------ Hàm điều khiển bơm và béc tưới ------------------
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

void turnOffCurrentSprinklerAndPump(int sprinklerIndex)
{
  digitalWrite(relayPins[sprinklerIndex], LOW);
  digitalWrite(relayPins[10], LOW);
  digitalWrite(relayPins[11], LOW);
}

// ------------------ Hàm hiển thị MENU ------------------
void updateMenuDisplay()
{
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  int y = 0;

  // Hiển thị trạng thái Blynk (ON/OFF)
  display.setCursor(110, y);
  display.println(Blynk.connected() ? "ON" : "OFF");

  switch (currentMenu)
  {
  case MAIN_MENU:
  {
    String labels[3] = {"CAU HINH", "KHOI DONG THU CONG", "TU DONG"};
    for (int i = 0; i < 3; i++)
    {
      display.setCursor(0, y);
      display.print(i == mainMenuSelection ? ">" : " ");
      if (i == 2)
      {
        display.print("TU DONG (");
        display.print((autoMode == AUTO_ON) ? "ON - " : "OFF - ");
        display.print(autoCurrentSprinkler + 1);
        display.print(")");
      }
      else
      {
        display.print(labels[i]);
      }
      y += 16;
    }
    display.setCursor(0, y);
    display.print("[");
    display.print(formatTime(autoStartMinute));
    display.print("-");
    display.print(formatTime(autoEndMinute));
    display.println("]");
    break;
  }
  case CONFIG_MENU:
  {
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
  }
  case TIME_SELECT_MENU:
  {
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
  }
  case AUTO_MENU:
  {
    String options[5] = {"AUTO ON", "AUTO OFF", "RESET", "CHINH BEC", "CHINH GIO"};
    display.setCursor(0, 0);
    display.println("TU DONG:");
    y = 16;
    if (autoMenuSelection < autoScrollOffset)
      autoScrollOffset = autoMenuSelection;
    if (autoMenuSelection >= autoScrollOffset + maxVisibleAuto)
      autoScrollOffset = autoMenuSelection - maxVisibleAuto + 1;
    for (int i = autoScrollOffset; i < 5 && i < autoScrollOffset + maxVisibleAuto; i++)
    {
      display.setCursor(0, y);
      display.print(i == autoMenuSelection ? ">" : " ");
      display.println(options[i]);
      y += 16;
    }
    break;
  }
  case AUTO_CURRENT_SPRINKLER:
  {
    display.setCursor(0, 0);
    display.println("CHINH BEC:");
    display.setCursor(0, 16);
    display.print("Value: ");
    display.println(autoCurrentSprinklerTemp + 1);
    display.setCursor(0, SCREEN_HEIGHT - 16);
    display.println("*: Cancel  #: Save");
    break;
  }
  case AUTO_TIME_MENU:
  {
    // Hiển thị 4 trường: FROM, TO, DUR, REST
    bool blink = ((millis() / 500) % 2) == 0;
    int lineY = 0;
    display.setCursor(0, lineY);
    display.println("CHINH GIO:");
    lineY += 16;
    // FROM
    display.setCursor(0, lineY);
    display.print(autoTimeSelectField == 0 ? ">" : " ");
    display.print("F[");
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
    display.print("] -");
    // TO
    display.print(autoTimeSelectField == 1 ? ">" : " ");
    display.print("T[");
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
    display.print("]");
    lineY += 16;
    // DUR
    display.setCursor(0, lineY);
    display.print(autoTimeSelectField == 2 ? ">" : " ");
    display.print("D[");
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
    display.print("] -");
    // REST
    display.print(autoTimeSelectField == 3 ? ">" : " ");
    display.print("R[");
    if (autoTimeSelectField == 3 && autoTimeEditPhase == 0 && blink)
      display.print("  ");
    else
    {
      if (autoRestHourTemp < 10)
        display.print("0");
      display.print(autoRestHourTemp);
    }
    display.print(":");
    if (autoTimeSelectField == 3 && autoTimeEditPhase == 1 && blink)
      display.print("  ");
    else
    {
      if (autoRestMinTemp < 10)
        display.print("0");
      display.print(autoRestMinTemp);
    }
    display.println("]");
    display.setCursor(0, SCREEN_HEIGHT - 16);
    display.println("*: Cancel  #: Save");
    break;
  }
  case RUNNING:
  {
    int yLine = 0;
    if (transitionActive)
    {
      int nextSprinkler = runSprinklerIndices[currentRunIndex + 1];
      unsigned long transElapsed = (millis() - transitionStartTime) / 1000;
      int transRemaining = (transitionDelaySeconds > transElapsed) ? transitionDelaySeconds - transElapsed : 0;
      display.setCursor(0, yLine);
      display.println("THU CONG");
      yLine += 16;
      display.setCursor(0, yLine);
      display.print("BEC ");
      display.print(nextSprinkler + 1);
      display.println(" MO");
      yLine += 16;
      unsigned long elapsed = (millis() - runStartTime) / 1000;
      unsigned long totalSec = irrigationTime * 60;
      unsigned long remain = (totalSec > elapsed) ? totalSec - elapsed : 0;
      display.setCursor(0, yLine);
      display.print("CON LAI: ");
      display.println(formatRemainingTime(remain));
      yLine += 16;
      display.setCursor(0, yLine);
      display.print("BEC ");
      display.print(runSprinklerIndices[currentRunIndex] + 1);
      display.print(" DONG: ");
      display.print(transRemaining);
      display.println("s");
    }
    else if (paused)
    {
      display.setCursor(0, 0);
      display.println("THU CONG");
      display.setCursor(0, 16);
      display.print("BEC ");
      display.print(runSprinklerIndices[currentRunIndex] + 1);
      display.println(" DUNG");
      unsigned long remSec = (pauseRemaining / 1000) + 1;
      display.setCursor(0, 32);
      display.print("CON LAI: ");
      display.println(formatRemainingTime(remSec));
    }
    else
    {
      display.setCursor(0, 0);
      display.println("THU CONG");
      if (currentRunIndex < runSprinklerCount)
      {
        display.setCursor(0, 16);
        display.print("BEC ");
        display.print(runSprinklerIndices[currentRunIndex] + 1);
        display.println(" CHAY");
        unsigned long elapsed = (millis() - runStartTime) / 1000;
        unsigned long totalSec = irrigationTime * 60;
        unsigned long remain = (totalSec > elapsed) ? totalSec - elapsed : 0;
        display.setCursor(0, 32);
        display.print("CON LAI: ");
        display.println(formatRemainingTime(remain));
      }
      else
      {
        display.setCursor(0, 16);
        display.println("XONG CHU TRINH");
        delay(2000);
        currentMenu = MAIN_MENU;
      }
    }
    break;
  }
  case RUNNING_AUTO:
  {
    display.setCursor(0, 0);
    if (paused)
    {
      display.println("TU DONG");
      display.setCursor(0, 16);
      display.print("BEC ");
      display.print(runSprinklerIndices[autoCurrentSprinkler] + 1);
      display.println(" DUNG");
      unsigned long remSec = (pauseRemaining / 1000) + 1;
      display.setCursor(0, 32);
      display.print("CON LAI: ");
      display.println(formatRemainingTime(remSec));
    }
    else
    {
      display.println("TU DONG");
      display.setCursor(0, 16);
      display.print("BEC ");
      display.print(runSprinklerIndices[autoCurrentSprinkler] + 1);
      display.println(" CHAY");

      // Tính thời gian còn lại
      unsigned long elapsed = (millis() - runStartTime) / 1000; // đã trôi qua (giây)
      unsigned long totalSec = ((unsigned long)(autoDurationHour * 60 + autoDurationMinute)) * 60UL;
      unsigned long remain = (totalSec > elapsed) ? totalSec - elapsed : 0;

      display.setCursor(0, 32);
      display.print("CON LAI: ");
      display.println(formatRemainingTime(remain));
    }
    break;
  }
  }
  display.display();
}

// ------------------ Hàm xử lý IR theo từng menu ------------------

void processMainMenuIR(const String &cmd)
{
  if (cmd == "UP")
    mainMenuSelection = (mainMenuSelection + 2) % 3;
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
    {
      currentMenu = AUTO_MENU;
      autoMenuSelection = 0;
      autoScrollOffset = 0;
    }
  }
}

void processConfigMenuIR(const String &cmd)
{
  if (cmd == "UP")
  {
    if (configMode == 0 && configSelected > 0)
      configSelected--;
    else if (configMode == 1)
      transitionDelayTemp++;
  }
  else if (cmd == "DOWN")
  {
    if (configMode == 0 && configSelected < 9)
      configSelected++;
    else if (configMode == 1 && transitionDelayTemp > 0)
      transitionDelayTemp--;
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

void processTimeSelectMenuIR(const String &cmd)
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
    if (timeSelectHour && tempHour > 0)
      tempHour -= TIME_STEP_HOUR;
    else if (!timeSelectHour && tempMinute >= TIME_STEP_MIN)
      tempMinute -= TIME_STEP_MIN;
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
        activatePumpForSprinkler(runSprinklerIndices[currentRunIndex]);
      }
    }
  }
  else if (cmd == "STAR")
    currentMenu = MAIN_MENU;
  else if (cmd.length() == 1 && isDigit(cmd.charAt(0)))
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

void processAutoMenuIR(const String &cmd)
{
  if (cmd == "UP")
    autoMenuSelection = (autoMenuSelection + 4) % 5;
  else if (cmd == "DOWN")
    autoMenuSelection = (autoMenuSelection + 1) % 5;
  else if (cmd == "OK")
  {
    if (autoMenuSelection == 0)
    { // AUTO ON
      autoMode = AUTO_ON;
      saveConfig();
      currentMenu = MAIN_MENU;
    }
    else if (autoMenuSelection == 1)
    { // AUTO OFF
      autoMode = AUTO_OFF;
      saveConfig();
      currentMenu = MAIN_MENU;
    }
    else if (autoMenuSelection == 2)
    { // RESET
      autoCurrentSprinkler = 0;
      lastAutoCycleEndEpoch = 0;
      saveConfig();
      currentMenu = MAIN_MENU;
    }
    else if (autoMenuSelection == 3)
    {
      currentMenu = AUTO_CURRENT_SPRINKLER;
      autoCurrentSprinklerTemp = autoCurrentSprinkler;
    }
    else if (autoMenuSelection == 4)
    { // CHINH GIO (bao gồm REST)
      autoTimeSelectField = 0;
      autoTimeEditPhase = 0;
      autoStartHourTemp = autoStartMinute / 60;
      autoStartMinTemp = autoStartMinute % 60;
      autoEndHourTemp = autoEndMinute / 60;
      autoEndMinTemp = autoEndMinute % 60;
      autoDurHourTemp = autoDurationHour;
      autoDurMinTemp = autoDurationMinute;
      autoRestHourTemp = autoRestTimeHour;
      autoRestMinTemp = autoRestTimeMinute;
      currentMenu = AUTO_TIME_MENU;
    }
  }
  else if (cmd == "STAR")
    currentMenu = MAIN_MENU;
}

void processAutoCurrentSprinklerIR(const String &cmd)
{
  if (cmd == "UP" && autoCurrentSprinklerTemp < 9)
    autoCurrentSprinklerTemp++;
  else if (cmd == "DOWN" && autoCurrentSprinklerTemp > 0)
    autoCurrentSprinklerTemp--;
  else if (cmd == "HASH")
  {
    autoCurrentSprinkler = autoCurrentSprinklerTemp;
    saveConfig();
    currentMenu = AUTO_MENU;
  }
  else if (cmd == "STAR")
    currentMenu = AUTO_MENU;
}

void processAutoTimeMenuIR(const String &cmd)
{
  // Xử lý 4 trường: 0: FROM, 1: TO, 2: DUR, 3: REST
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
      else if (autoTimeSelectField == 3)
        autoRestHourTemp = (autoRestHourTemp + 1) % 24;
    }
    else
    {
      if (autoTimeSelectField == 0)
        autoStartMinTemp = (autoStartMinTemp + 1) % 60;
      else if (autoTimeSelectField == 1)
        autoEndMinTemp = (autoEndMinTemp + 1) % 60;
      else if (autoTimeSelectField == 2)
        autoDurMinTemp = (autoDurMinTemp + 1) % 60;
      else if (autoTimeSelectField == 3)
        autoRestMinTemp = (autoRestMinTemp + 1) % 60;
    }
  }
  else if (cmd == "DOWN")
  {
    if (autoTimeEditPhase == 0)
    {
      if (autoTimeSelectField == 0)
        autoStartHourTemp = (autoStartHourTemp + 23) % 24;
      else if (autoTimeSelectField == 1)
        autoEndHourTemp = (autoEndHourTemp + 23) % 24;
      else if (autoTimeSelectField == 2)
        autoDurHourTemp = (autoDurHourTemp + 23) % 24;
      else if (autoTimeSelectField == 3)
        autoRestHourTemp = (autoRestHourTemp + 23) % 24;
    }
    else
    {
      if (autoTimeSelectField == 0)
        autoStartMinTemp = (autoStartMinTemp + 59) % 60;
      else if (autoTimeSelectField == 1)
        autoEndMinTemp = (autoEndMinTemp + 59) % 60;
      else if (autoTimeSelectField == 2)
        autoDurMinTemp = (autoDurMinTemp + 59) % 60;
      else if (autoTimeSelectField == 3)
        autoRestMinTemp = (autoRestMinTemp + 59) % 60;
    }
  }
  else if (cmd == "OK")
  {
    if (autoTimeEditPhase == 0)
      autoTimeEditPhase = 1;
    else
    {
      autoTimeEditPhase = 0;
      autoTimeSelectField = (autoTimeSelectField + 1) % 4;
    }
  }
  else if (cmd == "HASH")
  {
    autoStartMinute = autoStartHourTemp * 60 + autoStartMinTemp;
    autoEndMinute = autoEndHourTemp * 60 + autoEndMinTemp;
    autoDurationHour = autoDurHourTemp;
    autoDurationMinute = autoDurMinTemp;
    autoRestTimeHour = autoRestHourTemp;
    autoRestTimeMinute = autoRestMinTemp;
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
    if (autoTimeSelectField < 3)
    {
      autoTimeSelectField++;
      autoTimeEditPhase = 0;
    }
  }
  else if (cmd == "STAR")
    currentMenu = AUTO_MENU;
  else if (cmd.length() == 1 && isDigit(cmd.charAt(0)))
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
      if (autoTimeSelectField == 0 && value >= 0 && value <= 23)
        autoStartHourTemp = value;
      else if (autoTimeSelectField == 1 && value >= 0 && value <= 23)
        autoEndHourTemp = value;
      else if (autoTimeSelectField == 2 && value >= 0 && value <= 23)
        autoDurHourTemp = value;
      else if (autoTimeSelectField == 3 && value >= 0 && value <= 23)
        autoRestHourTemp = value;
    }
    else
    {
      if (autoTimeSelectField == 0 && value >= 0 && value <= 59)
        autoStartMinTemp = value;
      else if (autoTimeSelectField == 1 && value >= 0 && value <= 59)
        autoEndMinTemp = value;
      else if (autoTimeSelectField == 2 && value >= 0 && value <= 59)
        autoDurMinTemp = value;
      else if (autoTimeSelectField == 3 && value >= 0 && value <= 59)
        autoRestMinTemp = value;
    }
  }
}

void processRunningIR(const String &cmd)
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
      activatePumpForSprinkler(runSprinklerIndices[currentRunIndex]);
      paused = false;
    }
  }
  else if (cmd == "STAR")
  {
    for (int i = 0; i < 12; i++)
      digitalWrite(relayPins[i], LOW);
    currentMenu = MAIN_MENU;
  }
}

void processRunningAutoIR(const String &cmd)
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
      activatePumpForSprinkler(runSprinklerIndices[currentRunIndex]);
      paused = false;
    }
  }
  else if (cmd == "STAR")
  {
    for (int i = 0; i < 12; i++)
      digitalWrite(relayPins[i], LOW);
    autoMode = AUTO_OFF;
    currentMenu = MAIN_MENU;
    saveConfig();
  }
}

// ------------------ Xử lý tín hiệu IR ------------------
void processIRRemote()
{
  if (!IrReceiver.decode())
    return;

  uint32_t code = IrReceiver.decodedIRData.decodedRawData;
  unsigned long currentTime = millis();
  if (code == 0 || (code == lastCode && currentTime - lastReceiveTime < debounceTime))
  {
    IrReceiver.resume();
    return;
  }

  String cmd = mapIRCodeToCommand(code);
  lastCode = code;
  lastReceiveTime = currentTime;

  switch (currentMenu)
  {
  case MAIN_MENU:
    processMainMenuIR(cmd);
    break;
  case CONFIG_MENU:
    processConfigMenuIR(cmd);
    break;
  case TIME_SELECT_MENU:
    processTimeSelectMenuIR(cmd);
    break;
  case AUTO_MENU:
    processAutoMenuIR(cmd);
    break;
  case AUTO_CURRENT_SPRINKLER:
    processAutoCurrentSprinklerIR(cmd);
    break;
  case AUTO_TIME_MENU:
    processAutoTimeMenuIR(cmd);
    break;
  case RUNNING:
    processRunningIR(cmd);
    break;
  case RUNNING_AUTO:
    processRunningAutoIR(cmd);
    break;
  }
  IrReceiver.resume();
}

// ------------------ Hàm tính toán chu trình Auto ------------------
void computeAutoCycle()
{
  // Tính khoảng thời gian khả dụng (tính theo phút)
  int availableTime;
  if (autoStartMinute < autoEndMinute)
    availableTime = autoEndMinute - autoStartMinute;
  else
    availableTime = (1440 - autoStartMinute) + autoEndMinute;

  int autoDur = autoDurationHour * 60 + autoDurationMinute;
  int count = availableTime / autoDur;
  if (count > 10)
    count = 10;

  autoRunSprinklerCount = 0;
  // Chỉ thêm các béc được bật (theo thứ tự từ 0 đến 9)
  for (int i = 0; i < 10 && autoRunSprinklerCount < count; i++)
  {
    if (allowedSprinklers[i])
      runSprinklerIndices[autoRunSprinklerCount++] = i;
  }
}

// ------------------ Cập nhật chu trình RUNNING_AUTO (non-blocking) ------------------
void updateRunningAuto()
{
  if (currentMenu != RUNNING_AUTO || paused)
    return;

  // Nếu đang nghỉ giữa chu trình
  if (autoCycleResting)
  {
    if (millis() - autoCycleEndTime >= (unsigned long)autoRestTimeMinute * 60000UL)
    {
      // Nghỉ xong, tính lại chu trình Auto và bắt đầu lại từ béc 1
      computeAutoCycle();
      if (autoRunSprinklerCount > 0)
      {
        autoCurrentSprinkler = 0;
        runStartTime = millis();
        autoCycleResting = false;
        activatePumpForSprinkler(runSprinklerIndices[autoCurrentSprinkler]);
        saveConfig();
      }
      else
      {
        currentMenu = MAIN_MENU;
      }
    }
    return;
  }

  // Đang trong chu trình tưới
  unsigned long cycleDuration = ((unsigned long)(autoDurationHour * 60 + autoDurationMinute)) * 60000UL;
  unsigned long elapsedCycle = millis() - runStartTime;

  if (!transitionActive)
  {
    if (elapsedCycle >= cycleDuration)
    {
      if (autoCurrentSprinkler + 1 < autoRunSprinklerCount)
      {
        // Chuyển sang béc kế tiếp
        activatePumpForSprinkler(runSprinklerIndices[autoCurrentSprinkler + 1]);
        transitionActive = true;
        transitionStartTime = millis();
      }
      else
      {
        // Đã tưới xong béc cuối, tắt relay và bắt đầu nghỉ
        turnOffCurrentSprinklerAndPump(runSprinklerIndices[autoCurrentSprinkler]);
        autoCycleResting = true;
        autoCycleEndTime = millis();
      }
    }
  }
  else
  {
    unsigned long transElapsed = (millis() - transitionStartTime) / 1000;
    if (transElapsed >= transitionDelaySeconds)
    {
      digitalWrite(relayPins[runSprinklerIndices[autoCurrentSprinkler]], LOW);
      autoCurrentSprinkler++;
      runStartTime = millis();
      transitionActive = false;
      activatePumpForSprinkler(runSprinklerIndices[autoCurrentSprinkler]);
      saveConfig();
    }
  }
}

// ------------------ Kiểm tra Auto Mode ------------------
void checkAutoMode()
{
  if (autoMode == AUTO_OFF)
    return;
  unsigned long epoch = ntpClient.getEpochTime();
  tm *lt = localtime((time_t *)&epoch);
  int currentMinutes = lt->tm_hour * 60 + lt->tm_min;
  bool inAutoTime;
  if (autoStartMinute < autoEndMinute)
    inAutoTime = (currentMinutes >= autoStartMinute && currentMinutes < autoEndMinute);
  else
    inAutoTime = (currentMinutes >= autoStartMinute || currentMinutes < autoEndMinute);

  if (inAutoTime)
  {
    if (currentMenu != RUNNING_AUTO)
    {
      computeAutoCycle();
      if (autoRunSprinklerCount > 0)
      {
        currentMenu = RUNNING_AUTO;
        autoCurrentSprinkler = 0;
        runStartTime = millis();
        autoCycleResting = false;
        activatePumpForSprinkler(runSprinklerIndices[autoCurrentSprinkler]);
        saveConfig();
      }
    }
  }
  else
  {
    if (currentMenu == RUNNING_AUTO)
    {
      for (int i = 0; i < 12; i++)
        digitalWrite(relayPins[i], LOW);
      currentMenu = MAIN_MENU;
    }
  }
}

// ------------------ Lưu & tải cấu hình ------------------
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
  preferences.putInt("autoRest", autoRestTimeMinute); // lưu thời gian nghỉ (tính theo phút)
  preferences.putUInt("lastAutoCycleEndEpoch", lastAutoCycleEndEpoch);
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
  autoRestTimeMinute = preferences.getInt("autoRest", 5);
  autoRestTimeHour = 0;
  lastAutoCycleEndEpoch = preferences.getUInt("lastAutoCycleEndEpoch", 0);
  preferences.end();
  currentHour = irrigationTime / 60;
  currentMinute = irrigationTime % 60;
  Serial.println("Config loaded.");
}

// ------------------ Kiểm tra bảo vệ bơm và đồng bộ relay với Blynk ------------------
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
  if (!sprayerActive && pumpOn && !lastPumpProtectionTriggered)
  {
    lastPumpProtectionTriggered = true;
    Blynk.logEvent("pump_protect", "Khong co bec. Tat bom");
    digitalWrite(relayPins[10], LOW);
    digitalWrite(relayPins[11], LOW);
    Serial.println("Pump protection triggered.");
  }
  else if (sprayerActive || !pumpOn)
    lastPumpProtectionTriggered = false;
}

void syncRelayStatusToBlynk()
{
  static int lastStates[12] = {-1};
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
    }
  }
}

// ------------------ Setup và Loop ------------------
BlynkTimer timer;

void setup()
{
  Serial.begin(115200);
  for (int i = 0; i < 12; i++)
  {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], LOW);
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
  loadConfig();
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  Serial.println("ESP32 connected to Blynk!");
  setupIR();
  ntpClient.begin();
  ntpClient.update();
  timer.setInterval(500L, updateMenuDisplay);
  timer.setInterval(30000L, []()
                    { ntpClient.update(); });
}

void loop()
{
  Blynk.run();
  timer.run();
  processIRRemote();

  if (currentMenu == RUNNING_AUTO)
    updateRunningAuto();

  checkPumpProtection();
  syncRelayStatusToBlynk();
  checkAutoMode();
}
