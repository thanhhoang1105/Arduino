// acc thanhhoangngoc.bmt1105@gmail.com
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

Preferences preferences;

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

// Relay pins: Relay 1->10: Béc tưới; Relay 11: Bơm 1; Relay 12: Bơm 2
const int relayPins[12] = {2, 4, 16, 17, 5, 18, 19, 21, 22, 23, 26, 25};

// =========================
// IR Remote Setup
// =========================
const int RECV_PIN = 15; // Chân nhận IR
void setupIR()
{
  IrReceiver.begin(RECV_PIN, ENABLE_LED_FEEDBACK);
  Serial.println("IR Receiver đang chờ tín hiệu...");
}

// =========================
// IR Codes
// =========================
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
#define IR_CODE_STAR 3910598400UL // Dùng làm BACK/CANCEL (STAR)
#define IR_CODE_HASH 4061003520UL // Dùng làm SAVE (HASH)
#define IR_CODE_UP 3877175040UL
#define IR_CODE_DOWN 2907897600UL
#define IR_CODE_RIGHT 2774204160UL
#define IR_CODE_LEFT 4144561920UL
#define IR_CODE_OK 3810328320UL

// Biến debounce cho IR
unsigned long lastCode = 0;
unsigned long lastReceiveTime = 0;
const unsigned long debounceTime = 500;

// =========================
// Menu & Config
// =========================
enum MenuState
{
  MAIN_MENU,
  CONFIG_MENU,
  TIME_SELECT_MENU,
  AUTO_MENU,
  AUTO_TIME_MENU,
  RUNNING,     // Tưới theo cấu hình thủ công
  RUNNING_AUTO // Tưới tự động theo lịch
};
MenuState currentMenu = MAIN_MENU;
int mainMenuSelection = 0; // 0: CAU HINH, 1: KHOI DONG, 2: TU DONG

// =========================
// Auto Mode: AUTO_MENU
// =========================
enum AutoOption
{
  AUTO_TIME_OPT,
  AUTO_ON_OPT,
  AUTO_OFF_OPT,
  AUTO_RESET_OPT
};
int autoMenuSelection = 0;
const int maxVisibleAuto = 3; // hiển thị 3 dòng trong AUTO_MENU
int autoScrollOffset = 0;
enum AutoMode
{
  AUTO_ON,
  AUTO_OFF
};
AutoMode autoMode = AUTO_OFF;
int autoCurrentSprinkler = 0;

// AUTO_TIME_MENU: chỉnh khoảng thời gian tự động
int autoStartHour = 22; // FROM
int autoEndHour = 4;    // TO
// Thêm biến Duration: số giờ mỗi béc bật, mặc định 3 giờ.
int autoSprayerDuration = 3;
int autoTimeSelectField = 0; // 0: FROM, 1: TO, 2: DUR
String autoTimeInput = "";
unsigned long lastAutoTimeInput = 0;

// CONFIG_MENU:
// configMode = 0: BEC, configMode = 1: DELAY
int configMode = 0;
bool allowedSprinklers[10];
bool backupConfig[10];
int configSelected = 0; // Áp dụng khi configMode==0
int configScrollOffset = 0;
const int maxVisibleList = 2;    // Dành cho danh sách BEC (để hướng dẫn luôn hiện)
int transitionDelaySeconds = 10; // Delay (giây) cho chuyển đổi (áp dụng khi configMode==1)

// TIME_SELECT_MENU:
unsigned int irrigationTime = 120; // mặc định 120 phút (2 giờ)
#define TIME_STEP_HOUR 1           // Bước tăng giảm giờ = 1
#define TIME_STEP_MIN 15           // Bước tăng giảm phút = 15
// Các biến cho chế độ chỉnh giờ/phút:
bool timeSelectHour = true;              // true: chỉnh giờ, false: chỉnh phút
int currentHour = irrigationTime / 60;   // Lưu giờ hiện tại
int currentMinute = irrigationTime % 60; // Lưu phút hiện tại
String timeInput = "";                   // Chuỗi nhập số tạm
unsigned long lastTimeInput = 0;         // Thời gian nhập số cuối

// RUNNING: (chế độ tưới thủ công)
unsigned long runStartTime = 0; // Thời gian bắt đầu chu trình tưới
int runSprinklerIndices[10];    // Danh sách béc được bật theo cấu hình
int runSprinklerCount = 0;      // Số béc được bật
int currentRunIndex = 0;        // Béc hiện đang chạy
bool cycleStarted = false;      // Đã bắt đầu chu trình?
bool paused = false;
unsigned long pauseRemaining = 0;

// =========================
// Blynk Virtual Pin Callbacks
// =========================
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
};

// =========================
// Save & Load Config
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
  preferences.putInt("autoStart", autoStartHour);
  preferences.putInt("autoEnd", autoEndHour);
  preferences.putInt("autoDur", autoSprayerDuration);
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
  irrigationTime = preferences.getUInt("irrigationTime", 120);
  autoCurrentSprinkler = preferences.getInt("autoSprinkler", 0);
  autoStartHour = preferences.getInt("autoStart", 22);
  autoEndHour = preferences.getInt("autoEnd", 4);
  autoSprayerDuration = preferences.getInt("autoDur", 3);
  currentHour = irrigationTime / 60;
  currentMinute = irrigationTime % 60;
  preferences.end();
  Serial.println("Config loaded.");
}

// =========================
// Other Functions
// =========================
// Hàm kiểm tra bảo vệ bơm
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

// =========================
// Sync Relay Status to Blynk
// =========================
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
// Menu Display
// =========================
void updateMenuDisplay()
{
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  int y = 0;
  display.setCursor(110, y);
  display.println(Blynk.connected() ? "ON" : "OFF");

  switch (currentMenu)
  {
  case MAIN_MENU:
    // MAIN_MENU có 3 mục: CAU HINH, KHOI DONG, TU DONG (hiển thị trạng thái AUTO)
    if (mainMenuSelection == 0)
    {
      display.setCursor(0, y);
      display.println("> CAU HINH");
      y += 16;
      display.setCursor(0, y);
      display.println("  KHOI DONG");
      y += 16;
      display.setCursor(0, y);
      display.print("  TU DONG (");
      display.print((autoMode == AUTO_ON) ? "ON - " : "OFF - ");
      display.print(autoCurrentSprinkler + 1);
      display.print(")");
      y += 16;
      display.setCursor(0, y);
      display.print("  [");
      display.print(autoStartHour);
      display.print("-");
      display.print(autoEndHour);
      display.println("]");
    }
    else if (mainMenuSelection == 1)
    {
      display.setCursor(0, y);
      display.println("  CAU HINH");
      y += 16;
      display.setCursor(0, y);
      display.println("> KHOI DONG");
      y += 16;
      display.setCursor(0, y);
      display.print("  TU DONG (");
      display.print((autoMode == AUTO_ON) ? "ON - " : "OFF - ");
      display.print(autoCurrentSprinkler + 1);
      display.print(")");
      y += 16;
      display.setCursor(0, y);
      display.print("  [");
      display.print(autoStartHour);
      display.print("-");
      display.print(autoEndHour);
      display.println("]");
    }
    else
    { // mainMenuSelection == 2 => TU DONG
      display.setCursor(0, y);
      display.println("  CAU HINH");
      y += 16;
      display.setCursor(0, y);
      display.println("  KHOI DONG");
      y += 16;
      display.setCursor(0, y);
      display.print("> TU DONG (");
      display.print((autoMode == AUTO_ON) ? "ON - " : "OFF - ");
      display.print(autoCurrentSprinkler + 1);
      display.print(")");
      y += 16;
      display.setCursor(0, y);
      display.print("  [");
      display.print(autoStartHour);
      display.print("-");
      display.print(autoEndHour);
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
      display.println(transitionDelaySeconds);
      display.setCursor(0, SCREEN_HEIGHT - 16);
      display.println("*: Cancel  #: Save");
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

  case AUTO_MENU:
  {
    display.setCursor(0, y);
    display.println("TU DONG:");
    y += 16;
    const int maxVisibleAuto = 3;
    // Cập nhật autoScrollOffset
    static int autoScrollOffset = 0;
    if (autoMenuSelection < autoScrollOffset)
      autoScrollOffset = autoMenuSelection;
    if (autoMenuSelection >= autoScrollOffset + maxVisibleAuto)
      autoScrollOffset = autoMenuSelection - maxVisibleAuto + 1;
    String options[4] = {"CHINH GIO", "AUTO ON", "AUTO OFF", "RESET"};
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
    display.setCursor(0, y);
    display.println("CHINH GIO:");
    y += 16;
    display.setCursor(0, y);
    if (autoTimeSelectField == 0)
      display.print(">");
    else
      display.print(" ");
    display.print("FROM: ");
    display.print(autoStartHour);
    display.print("  ");
    if (autoTimeSelectField == 1)
      display.print(">");
    else
      display.print(" ");
    display.print("TO: ");
    display.println(autoEndHour);
    y += 16;
    display.setCursor(0, y);
    if (autoTimeSelectField == 2)
      display.print(">");
    else
      display.print(" ");
    display.print("DUR: ");
    display.println(autoSprayerDuration);
    y += 16;
    display.setCursor(0, y);
    display.println("*: Cancel  #: Save");
    break;
  }

  case RUNNING:
  {
    // Chế độ RUNNING (cấu hình thủ công)
    extern bool transitionActive;
    extern unsigned long transitionStartTime;
    extern int transitionDelaySeconds;
    extern bool paused;
    extern unsigned long pauseRemaining;
    if (transitionActive)
    {
      int nextBec = runSprinklerIndices[currentRunIndex + 1];
      int currentBec = runSprinklerIndices[currentRunIndex];
      unsigned long transElapsed = (millis() - transitionStartTime) / 1000;
      int transRemaining = transitionDelaySeconds - transElapsed;
      if (transRemaining < 0)
        transRemaining = 0;
      // Dòng 1: BEC mới đang mở
      display.setCursor(0, y);
      display.print("BEC ");
      display.print(nextBec + 1);
      display.println(" DANG MO");
      // Dòng 2: Thời gian còn lại của chu kỳ hiện hành
      unsigned long elapsed = (millis() - runStartTime) / 1000;
      unsigned long totalSec = irrigationTime * 60;
      unsigned long remain = (totalSec > elapsed) ? (totalSec - elapsed) : 0;
      unsigned int rh = remain / 3600;
      unsigned int rm = (remain % 3600) / 60;
      unsigned int rs = remain % 60;
      char timeStr[9];
      sprintf(timeStr, "%02u:%02u:%02u", rh, rm, rs);
      y += 16;
      display.setCursor(0, y);
      display.print("CON LAI: ");
      display.println(timeStr);
      // Dòng 3: BEC hiện đang đóng trong: transRemaining s
      y += 16;
      display.setCursor(0, y);
      display.print("BEC ");
      display.print(currentBec + 1);
      display.print(" DONG TRONG: ");
      display.print(transRemaining);
      display.println("s");
    }
    else if (paused)
    {
      display.setCursor(0, y);
      display.println("RUNNING PAUSED");
      // Chuyển đổi pauseRemaining (ms) sang HH:MM:SS
      unsigned long remSec = pauseRemaining / 1000;
      unsigned int rh = remSec / 3600;
      unsigned int rm = (remSec % 3600) / 60;
      unsigned int rs = remSec % 60;
      char timeStr[9];
      sprintf(timeStr, "%02u:%02u:%02u", rh, rm, rs);
      y += 16;
      display.setCursor(0, y);
      display.print("BEC ");
      display.print(runSprinklerIndices[currentRunIndex] + 1);
      display.print(" - CON LAI: ");
      y += 16;
      display.setCursor(0, y);
      display.println(timeStr);
    }
    else
    {
      display.setCursor(0, y);
      if (currentRunIndex < runSprinklerCount)
      {
        display.print("BEC ");
        display.print(runSprinklerIndices[currentRunIndex] + 1);
        display.println(" DANG CHAY");
        unsigned long elapsed = (millis() - runStartTime) / 1000;
        unsigned long totalSec = irrigationTime * 60;
        unsigned long remain = (totalSec > elapsed) ? (totalSec - elapsed) : 0;
        unsigned int rh = remain / 3600;
        unsigned int rm = (remain % 3600) / 60;
        unsigned int rs = remain % 60;
        char timeStr[9];
        sprintf(timeStr, "%02u:%02u:%02u", rh, rm, rs);
        y += 16;
        display.setCursor(0, y);
        display.print("CON LAI: ");
        display.println(timeStr);
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
    // Chế độ RUNNING_AUTO (tưới tự động)
    // Hiển thị: "AUTO RUNNING" ở dòng đầu, sau đó thông tin béc hiện đang chạy và thời gian còn lại.
    display.setCursor(0, 0);
    display.println("AUTO RUNNING");
    int yLine = 16;
    display.setCursor(0, yLine);
    display.print("BEC ");
    display.print(runSprinklerIndices[currentRunIndex] + 1);
    display.println(" DANG CHAY");
    yLine += 16;
    unsigned long elapsed = (millis() - runStartTime) / 1000;
    unsigned long totalSec = autoSprayerDuration * 3600UL; // autoSprayerDuration giờ
    unsigned long remain = (totalSec > elapsed) ? (totalSec - elapsed) : 0;
    unsigned int rh = remain / 3600;
    unsigned int rm = (remain % 3600) / 60;
    unsigned int rs = remain % 60;
    char timeStr[9];
    sprintf(timeStr, "%02u:%02u:%02u", rh, rm, rs);
    display.setCursor(0, yLine);
    display.print("CON LAI: ");
    display.println(timeStr);
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

      // MAIN_MENU
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
            {
              backupConfig[i] = allowedSprinklers[i];
            }
            configMode = 0; // Mặc định ở chế độ BEC
            currentMenu = CONFIG_MENU;
          }
          else if (mainMenuSelection == 1)
          {
            currentMenu = TIME_SELECT_MENU;
            timeSelectHour = true;
            currentHour = irrigationTime / 60;
            currentMinute = irrigationTime % 60;
            timeInput = "";
            lastTimeInput = 0;
          }
          else
          { // TU DONG
            currentMenu = AUTO_MENU;
            autoMenuSelection = 0;
            autoScrollOffset = 0;
          }
        }
      }
      // CONFIG_MENU
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
            transitionDelaySeconds++;
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
            if (transitionDelaySeconds > 0)
              transitionDelaySeconds--;
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
          {
            allowedSprinklers[i] = backupConfig[i];
          }
          currentMenu = MAIN_MENU;
        }
        else if (cmd == "HASH")
        {
          saveConfig();
          currentMenu = MAIN_MENU;
        }
      }
      // TIME_SELECT_MENU
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
          timeSelectHour = !timeSelectHour;
          timeInput = "";
          lastTimeInput = 0;
        }
        else if (cmd == "OK")
        {
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
            saveConfig();
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
              Serial.println("Chua bat bec nao!");
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
        {
          currentMenu = MAIN_MENU;
        }
      }
      // AUTO_MENU
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
            currentMenu = AUTO_TIME_MENU;
            autoTimeSelectField = 0; // Mặc định chỉnh FROM
            autoTimeInput = "";
            lastAutoTimeInput = 0;
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
      // AUTO_TIME_MENU
      else if (currentMenu == AUTO_TIME_MENU)
      {
        if (cmd == "UP")
        {
          if (autoTimeSelectField == 0)
          {
            autoStartHour = (autoStartHour + 1) % 24;
          }
          else if (autoTimeSelectField == 1)
          {
            autoEndHour = (autoEndHour + 1) % 24;
          }
          else if (autoTimeSelectField == 2)
          {
            autoSprayerDuration++;
          }
        }
        else if (cmd == "DOWN")
        {
          if (autoTimeSelectField == 0)
          {
            if (autoStartHour > 0)
              autoStartHour--;
          }
          else if (autoTimeSelectField == 1)
          {
            if (autoEndHour > 0)
              autoEndHour--;
          }
          else if (autoTimeSelectField == 2)
          {
            if (autoSprayerDuration > 1)
              autoSprayerDuration--;
          }
        }
        else if (cmd == "LEFT")
        {
          autoTimeSelectField = (autoTimeSelectField - 1);
          if (autoTimeSelectField < 0)
            autoTimeSelectField = 2;
          autoTimeInput = "";
          lastAutoTimeInput = 0;
        }
        else if (cmd == "RIGHT")
        {
          autoTimeSelectField = (autoTimeSelectField + 1) % 3;
          autoTimeInput = "";
          lastAutoTimeInput = 0;
        }
        else if (cmd == "HASH") // Nhấn # để lưu
        {
          saveConfig();
          currentMenu = AUTO_MENU;
        }
        else if (cmd == "STAR") // Nhấn * để hủy
        {
          currentMenu = AUTO_MENU;
        }
        else if (cmd >= "0" && cmd <= "9")
        {
          unsigned long now = millis();
          if (now - lastAutoTimeInput < 1000)
          {
            autoTimeInput += cmd;
          }
          else
          {
            autoTimeInput = cmd;
          }
          lastAutoTimeInput = now;
          int value = autoTimeInput.toInt();
          if (autoTimeSelectField == 0)
          {
            if (value < 24)
              autoStartHour = value;
          }
          else if (autoTimeSelectField == 1)
          {
            if (value < 24)
              autoEndHour = value;
          }
          else
          {
            if (value > 0 && value < 100)
              autoSprayerDuration = value;
          }
        }
      }
      // RUNNING
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
            Serial.println("RUNNING paused.");
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
            Serial.println("RUNNING resumed.");
          }
        }
        else if (cmd == "STAR")
        {
          for (int i = 0; i < 12; i++)
          {
            digitalWrite(relayPins[i], LOW);
          }
          currentMenu = MAIN_MENU;
          cycleStarted = false;
        }
      }
      // RUNNING_AUTO
      else if (currentMenu == RUNNING_AUTO)
      {
        if (cmd == "STAR")
        {
          // Tắt tất cả các relay
          for (int i = 0; i < 12; i++)
          {
            digitalWrite(relayPins[i], LOW);
          }
          // Chuyển về AUTO_OFF và quay về MAIN_MENU
          autoMode = AUTO_OFF;
          saveConfig(); // Lưu trạng thái AUTO_OFF
          currentMenu = MAIN_MENU;
          cycleStarted = false;
        }
      }
    }
    IrReceiver.resume();
  }
}

// =========================
// Hàm cập nhật chu trình RUNNING (non-blocking transition) - chế độ thủ công
// =========================
bool transitionActive = false;
unsigned long transitionStartTime = 0;

void updateRunning()
{
  if (currentMenu != RUNNING)
    return;
  if (paused)
    return;
  unsigned long cycleDuration = (unsigned long)irrigationTime * 60000UL;
  unsigned long elapsedCycle = millis() - runStartTime;
  if (!transitionActive)
  {
    if (elapsedCycle >= cycleDuration)
    {
      if (currentRunIndex + 1 < runSprinklerCount)
      {
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
        transitionActive = true;
        transitionStartTime = millis();
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
  else
  {
    unsigned long transElapsed = (millis() - transitionStartTime) / 1000;
    int transRemaining = transitionDelaySeconds - transElapsed;
    if (transRemaining <= 0)
    {
      int currentBec = runSprinklerIndices[currentRunIndex];
      digitalWrite(relayPins[currentBec], LOW);
      currentRunIndex++;
      runStartTime = millis();
      transitionActive = false;
    }
  }
}

// =========================
// Hàm cập nhật chu trình RUNNING_AUTO (non-blocking transition) - chế độ tự động
// =========================
bool autoTransitionActive = false;
unsigned long autoTransitionStartTime = 0;

void updateRunningAuto()
{
  if (currentMenu != RUNNING_AUTO)
    return;
  if (paused)
    return;
  unsigned long cycleDuration = (unsigned long)autoSprayerDuration * 3600000UL; // autoSprayerDuration giờ
  unsigned long elapsedCycle = millis() - runStartTime;
  if (!autoTransitionActive)
  {
    if (elapsedCycle >= cycleDuration)
    {
      if (currentRunIndex + 1 < runSprinklerCount)
      {
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
        autoTransitionActive = true;
        autoTransitionStartTime = millis();
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
  else
  {
    unsigned long transElapsed = (millis() - autoTransitionStartTime) / 1000;
    int transRemaining = transitionDelaySeconds - transElapsed;
    if (transRemaining <= 0)
    {
      int currentBec = runSprinklerIndices[currentRunIndex];
      digitalWrite(relayPins[currentBec], LOW);
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
  if (autoMode == AUTO_ON)
  {
    time_t now = time(NULL);
    struct tm *timeinfo = localtime(&now);
    int currentHourRT = timeinfo->tm_hour;
    int currentWday = timeinfo->tm_wday; // 0 = Sunday, 6 = Saturday
    // Giả sử tưới 5 ngày: từ Monday (1) đến Friday (5)
    bool waterDay = (currentWday >= 1 && currentWday <= 5);
    bool inAutoTime = false;
    if (autoStartHour <= autoEndHour)
    {
      if (currentHourRT >= autoStartHour && currentHourRT < autoEndHour)
        inAutoTime = true;
    }
    else
    { // qua đêm
      if (currentHourRT >= autoStartHour || currentHourRT < autoEndHour)
        inAutoTime = true;
    }
    if (waterDay && inAutoTime && currentMenu != RUNNING_AUTO)
    {
      // Thiết lập RUNNING_AUTO
      // Dùng autoSprayerDuration (giờ) làm thời gian mỗi béc
      irrigationTime = autoSprayerDuration * 60;
      runSprinklerCount = 0;
      for (int i = 0; i < 10; i++)
      {
        if (allowedSprinklers[i])
        {
          runSprinklerIndices[runSprinklerCount++] = i;
        }
      }
      if (runSprinklerCount > 0)
      {
        currentMenu = RUNNING_AUTO;
        currentRunIndex = 0;
        runStartTime = millis();
        paused = false;
        digitalWrite(relayPins[runSprinklerIndices[currentRunIndex]], HIGH);
        int bec = runSprinklerIndices[currentRunIndex];
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
        Serial.println("Auto mode started (RUNNING_AUTO).");
      }
    }
  }
}

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
}

void loop()
{
  Blynk.run();
  processIRRemote();
  if (currentMenu == RUNNING)
  {
    updateRunning();
  }
  else if (currentMenu == RUNNING_AUTO)
  {
    updateRunningAuto();
  }
  updateMenuDisplay();
  checkPumpProtection();
  syncRelayStatusToBlynk();
  checkAutoMode();
  delay(100);
}
