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
int mainMenuSelection = 0; // 0: CAU HINH, 1: KHOI ĐỘNG THỦ CÔNG, 2: TU ĐỘNG

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

// Các biến tạm cho AUTO_TIME_MENU
int autoStartTemp = autoStartMinute;
int autoEndTemp = autoEndMinute;
int autoDurHourTemp = autoDurationHour;
int autoDurMinTemp = autoDurationMinute;
int autoTimeSelectField = 0; // 0: FROM, 1: TO, 2: DUR_HOUR, 3: DUR_MIN
String autoTimeInput = "";
unsigned long lastAutoTimeInput = 0;

// Biến cho auto cycle nghỉ (skip period sau khi tưới hết 10 béc)
unsigned long autoCycleFinishTime = 0;

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
void debugPrintAutoMode(int currentHourRT, int currentMinuteRT, int currentTimeMinutes)
{
  Serial.println("=== Debug checkAutoMode ===");
  Serial.print("Current RTC Hour: ");
  Serial.println(currentHourRT);
  Serial.print("Current RTC Minute: ");
  Serial.println(currentMinuteRT);
  Serial.print("Current Time (min): ");
  Serial.println(currentTimeMinutes);
  Serial.print("autoStartMinute: ");
  Serial.println(autoStartMinute);
  Serial.print("autoEndMinute: ");
  Serial.println(autoEndMinute);
  Serial.print("inAutoTime: ");
  Serial.println(((currentTimeMinutes >= autoStartMinute && currentTimeMinutes < autoEndMinute) ? "true" : "false"));
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
    // 3 mục: CAU HINH, KHOI ĐỘNG THỦ CÔNG, TU ĐỘNG (kèm auto mode info)
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
    display.setCursor(0, 0);
    display.println("CHINH GIO:");
    int yLine = 16;
    display.setCursor(0, yLine);
    // FROM field
    display.print(autoTimeSelectField == 0 ? ">" : " ");
    display.print("FROM: ");
    display.print(autoStartTemp);
    display.print("  ");
    // TO field
    display.print(autoTimeSelectField == 1 ? ">" : " ");
    display.print("TO: ");
    display.print(autoEndTemp);
    yLine += 16;
    display.setCursor(0, yLine);
    // DUR_HOUR field
    display.print(autoTimeSelectField == 2 ? ">" : " ");
    display.print("HOUR: ");
    display.print(autoDurHourTemp);
    display.print("  ");
    // DUR_MIN field
    display.print(autoTimeSelectField == 3 ? ">" : " ");
    display.print("MIN: ");
    display.println(autoDurMinTemp);
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
      display.print(runSprinklerIndices[currentRunIndex] + 1);
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
        int nextSprinkler = runSprinklerIndices[currentRunIndex + 1];
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
        display.print(runSprinklerIndices[currentRunIndex] + 1);
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
        display.print(runSprinklerIndices[currentRunIndex] + 1);
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
            // Lưu backup cấu hình béc tưới
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
            autoStartTemp = autoStartMinute;
            autoEndTemp = autoEndMinute;
            autoDurHourTemp = autoDurationHour;
            autoDurMinTemp = autoDurationMinute;
            autoTimeInput = "";
            lastAutoTimeInput = 0;
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
            if (autoMode == AUTO_ON)
              autoCurrentSprinkler = 0;
            saveConfig();
            currentMenu = MAIN_MENU;
          }
        }
        else if (cmd == "STAR")
        {
          currentMenu = MAIN_MENU;
        }
      }
      else if (currentMenu == AUTO_TIME_MENU)
      {
        if (cmd == "UP")
        {
          if (autoTimeSelectField == 0)
          {
            autoStartTemp = (autoStartTemp + 1) % 1440;
          }
          else if (autoTimeSelectField == 1)
          {
            autoEndTemp = (autoEndTemp + 1) % 1440;
          }
          else if (autoTimeSelectField == 2)
          {
            autoDurHourTemp++;
          }
          else if (autoTimeSelectField == 3)
          {
            autoDurMinTemp += 15;
            if (autoDurMinTemp >= 60)
              autoDurMinTemp = 59;
          }
        }
        else if (cmd == "DOWN")
        {
          if (autoTimeSelectField == 0)
          {
            if (autoStartTemp > 0)
              autoStartTemp--;
          }
          else if (autoTimeSelectField == 1)
          {
            if (autoEndTemp > 0)
              autoEndTemp--;
          }
          else if (autoTimeSelectField == 2)
          {
            if (autoDurHourTemp > 0)
              autoDurHourTemp--;
          }
          else if (autoTimeSelectField == 3)
          {
            if (autoDurMinTemp >= 15)
              autoDurMinTemp -= 15;
            else
              autoDurMinTemp = 0;
          }
        }
        else if (cmd == "LEFT")
        {
          autoTimeSelectField = (autoTimeSelectField - 1 + 4) % 4;
          autoTimeInput = "";
          lastAutoTimeInput = 0;
        }
        else if (cmd == "RIGHT")
        {
          autoTimeSelectField = (autoTimeSelectField + 1) % 4;
          autoTimeInput = "";
          lastAutoTimeInput = 0;
        }
        else if (cmd == "HASH")
        {
          autoStartMinute = autoStartTemp;
          autoEndMinute = autoEndTemp;
          autoDurationHour = autoDurHourTemp;
          autoDurationMinute = autoDurMinTemp;
          saveConfig();
          currentMenu = AUTO_MENU;
        }
        else if (cmd == "STAR")
        {
          currentMenu = AUTO_MENU;
        }
        else if (cmd >= "0" && cmd <= "9")
        {
          unsigned long now = millis();
          if (now - lastAutoTimeInput < 1000)
            autoTimeInput += cmd;
          else
            autoTimeInput = cmd;
          lastAutoTimeInput = now;
          int value = autoTimeInput.toInt();
          if (autoTimeSelectField == 0)
          {
            if (value < 1440)
              autoStartTemp = value;
          }
          else if (autoTimeSelectField == 1)
          {
            if (value < 1440)
              autoEndTemp = value;
          }
          else if (autoTimeSelectField == 2)
          {
            if (value >= 0 && value < 100)
              autoDurHourTemp = value;
          }
          else if (autoTimeSelectField == 3)
          {
            if (value >= 0 && value < 60)
              autoDurMinTemp = value;
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
            if (bec < 6)
            {
              digitalWrite(relayPins[10], HIGH);
            }
            else
            {
              digitalWrite(relayPins[10], HIGH);
              digitalWrite(relayPins[11], HIGH);
            }
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
            if (bec < 6)
            {
              digitalWrite(relayPins[10], HIGH);
            }
            else
            {
              digitalWrite(relayPins[10], HIGH);
              digitalWrite(relayPins[11], HIGH);
            }
            paused = false;
          }
        }
        else if (cmd == "STAR")
        {
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
      if (currentRunIndex + 1 < runSprinklerCount)
      {
        int nextSprinkler = runSprinklerIndices[currentRunIndex + 1];
        activatePumpForSprinkler(nextSprinkler);
        autoTransitionActive = true;
        autoTransitionStartTime = millis();
      }
      else
      {
        // Đã chạy hết các béc, cập nhật autoCurrentSprinkler và kết thúc chu trình auto
        autoCurrentSprinkler = (autoCurrentSprinkler + runSprinklerCount) % 10;
        saveConfig();
        autoCycleFinishTime = millis();
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
      int currentSprinkler = runSprinklerIndices[currentRunIndex];
      digitalWrite(relayPins[currentSprinkler], LOW);
      currentRunIndex++;
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
  // Lấy thời gian thực đã cập nhật qua timer (không gọi ntpClient.update() ở đây)
  int currentHourRT = ntpClient.getHours();
  int currentMinuteRT = ntpClient.getMinutes();
  int currentTimeMinutes = currentHourRT * 60 + currentMinuteRT;

  // Kiểm tra khoảng nghỉ (nếu chu trình auto đã hoàn thành, nghỉ 2 ngày)
  bool skipCycle = false;
  if (autoCycleFinishTime != 0 && (millis() - autoCycleFinishTime < (2UL * 24UL * 3600000UL)))
  {
    skipCycle = true;
  }
  else
  {
    autoCycleFinishTime = 0;
  }
  bool waterDay = !skipCycle;

  // Xác định thời gian nằm trong cửa sổ auto (xử lý cả qua đêm)
  bool inAutoTime = false;
  if (autoStartMinute <= autoEndMinute)
  {
    inAutoTime = (currentTimeMinutes >= autoStartMinute && currentTimeMinutes < autoEndMinute);
  }
  else
  {
    inAutoTime = (currentTimeMinutes >= autoStartMinute || currentTimeMinutes < autoEndMinute);
  }

  // Debug in thông tin (có thể tắt để giảm overhead)
  if (millis() - lastDebugTime >= debugInterval)
  {
    debugPrintAutoMode(currentHourRT, currentMinuteRT, currentTimeMinutes);
    lastDebugTime = millis();
  }

  // Nếu đang chạy auto mode nhưng không còn trong khung giờ, chờ béc hiện hành kết thúc
  if (!inAutoTime && currentMenu == RUNNING_AUTO)
  {
    unsigned long cycleDuration = ((unsigned long)(autoDurationHour * 60 + autoDurationMinute)) * 60000UL;
    if (millis() - runStartTime >= cycleDuration)
    {
      autoCurrentSprinkler = (runSprinklerIndices[currentRunIndex] + 1) % 10;
      int currentSprinkler = runSprinklerIndices[currentRunIndex];
      turnOffCurrentSprinklerAndPump(currentSprinkler);
      Serial.println("Auto mode ended (out of time window).");
      currentMenu = MAIN_MENU;
    }
    return;
  }

  // Nếu điều kiện auto thỏa mãn và chưa ở chế độ RUNNING_AUTO, kích hoạt chế độ auto
  if (autoMode == AUTO_ON && waterDay && inAutoTime && currentMenu != RUNNING_AUTO)
  {
    runSprinklerCount = 10;
    for (int i = 0; i < 10; i++)
    {
      runSprinklerIndices[i] = (autoCurrentSprinkler + i) % 10;
    }
    currentMenu = RUNNING_AUTO;
    currentRunIndex = 0;
    runStartTime = millis();
    paused = false;
    // Bật béc đầu tiên và bơm theo logic
    activatePumpForSprinkler(runSprinklerIndices[currentRunIndex]);
    Serial.print("Auto mode started (RUNNING_AUTO). Current RTC Hour: ");
    Serial.println(currentHourRT);
  }
}

// =========================
// Lưu & tải cấu hình (giữ nguyên)
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
  preferences.putInt("autoStartMinute", autoStartMinute);
  preferences.putInt("autoEndMinute", autoEndMinute);
  int totalAutoDur = autoDurationHour * 60 + autoDurationMinute;
  preferences.putInt("autoDur", totalAutoDur);
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
  autoStartTemp = autoStartMinute;
  autoEndMinute = preferences.getInt("autoEnd", 240);
  autoEndTemp = autoEndMinute;
  int totalAutoDur = preferences.getInt("autoDur", 180);
  autoDurationHour = totalAutoDur / 60;
  autoDurationMinute = totalAutoDur % 60;
  currentHour = irrigationTime / 60;
  currentMinute = irrigationTime % 60;
  preferences.end();
  Serial.println("Config loaded.");
}

// =========================
// Hàm kiểm tra bảo vệ bơm và đồng bộ relay với Blynk (giữ nguyên)
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
  timer.setInterval(60000L, []()
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
