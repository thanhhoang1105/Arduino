/*
  ESP32 Coffee Garden Irrigation System
  - 10 Valves, 2 Pumps (Relays)
  - IR Remote Control for Menu Navigation
  - LCD 20x4 via I2C (PCF8574) using LiquidCrystal_PCF8574
  - ACS712 Current Sensor for Power Check
  - Blynk Integration (12 Switches: 10 Valves + 2 Pumps)
  - Preferences (RTC Memory) for Saving Configurations
  - Manual (Delay-Based) and Automatic (Time-Based) Irrigation Modes

  Note:
  - Thay đổi Blynk Auth Token, WiFi SSID, Password cho phù hợp.
  - Địa chỉ I²C của LCD phải đúng (mặc định 0x27).
  - Mô-đun ACS712 30A: độ nhạy ~66mV/A. Điều chỉnh nếu dùng loại khác.
  - Sử dụng NTP để đồng bộ thời gian (Asia/Ho_Chi_Minh, UTC+7).
*/

#define BLYNK_TEMPLATE_ID "TMPL6xy4mK0KN"
#define BLYNK_TEMPLATE_NAME "Smart Irrigation Controller"
#define BLYNK_AUTH_TOKEN "J34tRXWg_QTXt8bs0SfbZFSc1ARUpL0o"

#include <Arduino.h>
#include <Preferences.h>
#include <Wire.h>
#include <LiquidCrystal_PCF8574.h>   // Thư viện LCD PCF8574
#include <IRremote.h>
#include <BlynkSimpleEsp32.h>
#include <time.h>
#include <WiFi.h>

// ======================
// === HARDWARE MAP ===
// ======================

// Relay pins: 10 Valves, 2 Pumps
static const uint8_t relayValvePins[10] = {2, 4, 16, 17, 5, 18, 19, 21, 22, 23};
static const uint8_t relayPumpPins[2]   = {26, 25};

// IR Receiver
static const uint8_t IR_RECEIVER_PIN = 15;

// LCD (I2C PCF8574)
static const uint8_t LCD_I2C_ADDR = 0x27;
LiquidCrystal_PCF8574 lcd(LCD_I2C_ADDR);

// ACS712 (Analog Current Sensor)
static const uint8_t ACS712_PIN = 34;

// ======================
// === IR CODES ===
// ======================
#define IR_CODE_1      0xBA45FF00UL
#define IR_CODE_2      0xB946FF00UL
#define IR_CODE_3      0xB847FF00UL
#define IR_CODE_4      0xBB44FF00UL
#define IR_CODE_5      0xBF40FF00UL
#define IR_CODE_6      0xBC43FF00UL
#define IR_CODE_7      0xF807FF00UL
#define IR_CODE_8      0xEA15FF00UL
#define IR_CODE_9      0xF609FF00UL
#define IR_CODE_0      0xE619FF00UL

#define IR_CODE_STAR   0xE916FF00UL  // CANCEL
#define IR_CODE_HASH   0xF20DFF00UL  // SAVE

#define IR_CODE_UP     0xE718FF00UL
#define IR_CODE_DOWN   0xAD52FF00UL
#define IR_CODE_RIGHT  0xA55AFF00UL
#define IR_CODE_LEFT   0xF708FF00UL
#define IR_CODE_OK     0xE31CFF00UL

// ======================
// === BLYNK CONFIG ===
// ======================
char ssid[] = "VU ne";
char pass[] = "12341234";

// Virtual Pins: V0..V9 for Valves 1..10, V10..V11 for Pumps 1..2

// ======================
// === PREFERENCES ===
// ======================
Preferences prefs;

// ======================
// === TYPE DEFINITIONS ===
// ======================
enum AppState {
  STATE_MAIN,
  STATE_CONFIG,
  STATE_MANUAL,
  STATE_AUTO
};

enum ConfigMode {
  MODE_ONOFF,
  MODE_DELAY,
  MODE_POWERCHECK
};

struct TimeHM {
  uint8_t hour;
  uint8_t minute;
};

// ======================
// === GLOBAL STATE ===
// ======================

// Relay states
bool savedValveState[10];
bool workingValveState[10];
bool savedPumpState[2];
bool workingPumpState[2];

// Delay (seconds) for all valves
uint16_t savedDelaySec;
uint16_t workingDelaySec;

// Power Check (ACS712)
bool savedPowerCheckEnabled;
bool workingPowerCheckEnabled;

// Automatic mode
bool savedAutoEnabled;
bool workingAutoEnabled;

uint8_t savedAutoStartIndex;
uint8_t workingAutoStartIndex;

TimeHM savedAutoFrom, workingAutoFrom;
TimeHM savedAutoTo,   workingAutoTo;
TimeHM savedDuration, workingDuration;
TimeHM savedRest,     workingRest;

// Manual mode (delay-based)
uint8_t manualHour;      // 0..99
uint8_t manualMin;       // 0..59
bool editingManualHour;  // true = editing hour, false = editing minute
bool manualScheduled;    // true = a manual timer has been set
uint32_t nextManualMillis;

// Application state
AppState currentAppState = STATE_MAIN;
uint8_t mainMenuIndex = 0;       // 0=Cấu hình, 1=Thủ công, 2=Tự động
ConfigMode currentConfigMode = MODE_ONOFF;
uint8_t configMenuPage = 0;      // For ON/OFF mode paging
uint8_t configMenuIndex = 0;     // Index within current page

// Auto mode selection state
uint8_t autoMenuSelectedIndex = 0; // 0=ON,1=OFF,2=RESET,3=SELECT_VALVE,4=TIME_SETUP
uint8_t autoMenuPage = 0; // page for auto menu
const uint8_t autoMenuItems = 5;
const uint8_t autoMenuItemsOnPage = 3;
uint8_t autoSelectField = 0; // field for time selection
bool inSelectValve = false;
bool inTimeSetup = false;

// Flags for running tasks
bool autoRunning = false;
uint32_t autoLastActionMillis = 0;
bool autoStep = false;  // false = waiting to start next valve; true = in rest/duration wait

// Utility
bool nowInTimeRange(const TimeHM &from, const TimeHM &to);
TimeHM getCurrentTimeHM();

// Blink state for manual and auto menus
unsigned long lastManualBlink = 0;
bool manualBlinkState = false;
unsigned long lastAutoBlink = 0;
bool autoBlinkState = false;

// ======================
// === SETUP ===
// ======================
void setup() {
  // Serial for debugging
  Serial.begin(115200);
  while (!Serial) { delay(10); }

  // Initialize hardware (relay pins, IR pin, LCD, etc.)
  initializeHardware();

  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Serial.print(".");
  }
  Serial.println("WiFi connected");


  // Initialize Preferences
  prefs.begin("cfg", false);
  loadConfigFromPrefs();
  // Initialize working state from saved state
  memcpy(workingValveState, savedValveState, sizeof(savedValveState));
  memcpy(workingPumpState,  savedPumpState,  sizeof(savedPumpState));
  workingDelaySec           = savedDelaySec;
  workingPowerCheckEnabled  = savedPowerCheckEnabled;
  workingAutoEnabled        = savedAutoEnabled;
  workingAutoStartIndex     = savedAutoStartIndex;
  workingAutoFrom           = savedAutoFrom;
  workingAutoTo             = savedAutoTo;
  workingDuration           = savedDuration;
  workingRest               = savedRest;
  manualScheduled           = false;
  editingManualHour         = true;
  manualHour                = 0;
  manualMin                 = 0;

  // Initialize Blynk
  Blynk.config(BLYNK_AUTH_TOKEN);
  Blynk.connect(1000);

  // Initialize IR
  IrReceiver.begin(IR_RECEIVER_PIN, ENABLE_LED_FEEDBACK); // Bắt đầu nhận IR

  // Initialize NTP for local time (Asia/Ho_Chi_Minh = UTC+7)
  configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov");

  // Initial LCD display (hiển thị menu chính ngay sau khởi động)
  drawMainMenu();

  // Sync relays và Blynk trên startup
  updateOutputsFromWorking();
}

// ======================
// === MAIN LOOP ===
// ======================
void loop() {
  // Blynk
  Blynk.run();


  // IR processing
  if (IrReceiver.decode()) {
    uint32_t code = IrReceiver.decodedIRData.decodedRawData;
    Serial.println(code, HEX);
    handleIR(code);
    IrReceiver.resume();
  }

  // Blink for manual menu
  if (currentAppState == STATE_MANUAL) {
    if (millis() - lastManualBlink > 300) {
      manualBlinkState = !manualBlinkState;
      lastManualBlink = millis();
      drawManualMenu();
    }
  }
  // Blink for auto time select
  if (currentAppState == STATE_AUTO && autoMenuSelectedIndex == 4) {
    if (millis() - lastAutoBlink > 300) {
      autoBlinkState = !autoBlinkState;
      lastAutoBlink = millis();
      drawAutoMenu();
    }
  }

  // Power Check
  if (workingPowerCheckEnabled) {
    float currentA = readACS712Current();
    if (currentA < 0.1) {
      // Power lost
      turnAllOff();
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("MẤT NGUỒN");
        lcd.setCursor(0, 1);
      lcd.print("CHECK POWER");
      delay(2000);
      drawMainMenu();
      // Restore app state
      currentAppState = STATE_MAIN;
    }
  }

  // Manual mode check (delay-based)
  if (manualScheduled) {
    uint32_t nowMs = millis();
    if (nowMs >= nextManualMillis) {
      manualScheduled = false;
      // Execute watering as per savedValveState
      for (uint8_t i = 0; i < 10; i++) {
        workingValveState[i] = savedValveState[i];
      }
      updateOutputsFromWorking();
      delay(savedDelaySec * 1000UL);
      // Turn off all valves
      for (uint8_t i = 0; i < 10; i++) {
        workingValveState[i] = false;
      }
      updateOutputsFromWorking();
    }
  }

  // Automatic mode check (time-based)
  if (workingAutoEnabled) {
    if (nowInTimeRange(savedAutoFrom, savedAutoTo)) {
      if (!autoRunning) {
        // Start first cycle
        autoRunning = true;
        autoStep = false;
      }
      if (!autoStep) {
        // Turn on the current valve
        for (uint8_t i = 0; i < 10; i++) {
          workingValveState[i] = (i == (savedAutoStartIndex - 1));
        }
        updateOutputsFromWorking();
        autoLastActionMillis = millis();
        autoStep = true;
      } else {
        // We are in duration/rest cycle
        uint32_t elapsed = millis() - autoLastActionMillis;
        uint32_t durMs = (savedDuration.hour * 3600UL + savedDuration.minute * 60UL) * 1000UL;
        uint32_t restMs = (savedRest.hour * 3600UL + savedRest.minute * 60UL) * 1000UL;
        if (elapsed >= durMs) {
          // Duration elapsed: turn off current valve
          for (uint8_t i = 0; i < 10; i++) {
            workingValveState[i] = false;
          }
          updateOutputsFromWorking();
          // Wait rest
          autoLastActionMillis = millis();
          while (millis() - autoLastActionMillis < restMs) {
            // Check power and Blynk during rest
            if (workingPowerCheckEnabled && (readACS712Current() < 0.1)) {
              turnAllOff();
              autoRunning = false;
      break;
    }
            Blynk.run();
          }
          // Advance to next valve
          savedAutoStartIndex++;
          if (savedAutoStartIndex > 10) savedAutoStartIndex = 1;
          autoStep = false; // Next: start next valve
        }
      }
    } else {
      // Outside auto window: turn all off và reset
      if (autoRunning) {
        turnAllOff();
        autoRunning = false;
      }
    }
  }

  // Tránh vòng lặp quá nhanh
  delay(50);
}

// ======================
// === INITIALIZATION FUNCTIONS ===
// ======================
void initializeHardware() {
  // Relay pins
  for (uint8_t i = 0; i < 10; i++) {
    pinMode(relayValvePins[i], OUTPUT);
    digitalWrite(relayValvePins[i], HIGH);  // OFF (LOW = ON)
  }
  for (uint8_t j = 0; j < 2; j++) {
    pinMode(relayPumpPins[j], OUTPUT);
    digitalWrite(relayPumpPins[j], HIGH);  // OFF
  }

  // IR Receiver
  pinMode(IR_RECEIVER_PIN, INPUT);

  // LCD Initialization
  Wire.begin(13, 14);               // Khởi tạo I2C
  lcd.begin(20, 4);            // Đặt kích thước 20x4
  lcd.setBacklight(255);       // Bật đèn nền tối đa
  lcd.setCursor(0, 0);
  lcd.print("LCD initialized");
  delay(100);                  // Chờ ổn định LCD
}

// ======================
// === PREFERENCES ===
// ======================
void loadConfigFromPrefs() {
  // Valves
  for (uint8_t i = 0; i < 10; i++) {
    String key = "v" + String(i);
    savedValveState[i] = prefs.getBool(key.c_str(), false);
  }
  // Pumps
  savedPumpState[0] = prefs.getBool("p0", false);
  savedPumpState[1] = prefs.getBool("p1", false);
  // Delay
  savedDelaySec = prefs.getUInt("delay", 5);
  // Power Check
  savedPowerCheckEnabled = prefs.getBool("power", false);
  // Auto
  savedAutoEnabled = prefs.getBool("ae", false);
  savedAutoStartIndex = prefs.getUInt("as", 1);
  savedAutoFrom.hour   = prefs.getUInt("afh", 0);
  savedAutoFrom.minute = prefs.getUInt("afm", 0);
  savedAutoTo.hour     = prefs.getUInt("ath", 0);
  savedAutoTo.minute   = prefs.getUInt("atm", 0);
  savedDuration.hour   = prefs.getUInt("adh", 0);
  savedDuration.minute = prefs.getUInt("adm", 1);
  savedRest.hour       = prefs.getUInt("arh", 0);
  savedRest.minute     = prefs.getUInt("arm", 1);
}

void saveConfigToPrefs() {
  // Valves
  for (uint8_t i = 0; i < 10; i++) {
    String key = "v" + String(i);
    prefs.putBool(key.c_str(), savedValveState[i]);
  }
  // Pumps
  prefs.putBool("p0", savedPumpState[0]);
  prefs.putBool("p1", savedPumpState[1]);
  // Delay
  prefs.putUInt("delay", savedDelaySec);
  // Power Check
  prefs.putBool("power", savedPowerCheckEnabled);
  // Auto
  prefs.putBool("ae", savedAutoEnabled);
  prefs.putUInt("as", savedAutoStartIndex);
  prefs.putUInt("afh", savedAutoFrom.hour);
  prefs.putUInt("afm", savedAutoFrom.minute);
  prefs.putUInt("ath", savedAutoTo.hour);
  prefs.putUInt("atm", savedAutoTo.minute);
  prefs.putUInt("adh", savedDuration.hour);
  prefs.putUInt("adm", savedDuration.minute);
  prefs.putUInt("arh", savedRest.hour);
  prefs.putUInt("arm", savedRest.minute);
}

// ======================
// === ADC / POWER CHECK ===
// ======================
float readACS712Current() {
  int raw = analogRead(ACS712_PIN);
  float voltage = (raw / 4095.0) * 3.3;       // ESP32 ADC reference 3.3V
  float offset = 2.5;                         // ACS712 zero-current voltage
  float sensitivity = 0.066;                  // 66 mV/A cho module 30A
  float current = (voltage - offset) / sensitivity;
  if (current < 0) current = -current;        // Lấy trị tuyệt đối
  return current;
}

void turnAllOff() {
  for (uint8_t i = 0; i < 10; i++) {
    workingValveState[i] = false;
  }
  for (uint8_t j = 0; j < 2; j++) {
    workingPumpState[j] = false;
  }
  updateOutputsFromWorking();
}

// ======================
// === UTILITY FUNCTIONS ===
// ======================
bool anyValveOpen() {
  for (uint8_t i = 0; i < 10; i++) {
    if (workingValveState[i]) return true;
  }
  return false;
}

bool anySavedValveOpen() {
  for (uint8_t i = 0; i < 10; i++) {
    if (savedValveState[i]) return true;
  }
  return false;
}

bool nowInTimeRange(const TimeHM &from, const TimeHM &to) {
  TimeHM nowHM = getCurrentTimeHM();
  uint16_t nowMinutes = nowHM.hour * 60 + nowHM.minute;
  uint16_t fromMinutes = from.hour * 60 + from.minute;
  uint16_t toMinutes   = to.hour * 60 + to.minute;
  if (fromMinutes <= toMinutes) {
    return (nowMinutes >= fromMinutes && nowMinutes <= toMinutes);
  } else {
    // Khoảng wrap qua nửa đêm
    return (nowMinutes >= fromMinutes || nowMinutes <= toMinutes);
  }
}

TimeHM getCurrentTimeHM() {
  time_t now;
  struct tm timeInfo;
  time(&now);
  localtime_r(&now, &timeInfo);
  TimeHM result;
  result.hour = timeInfo.tm_hour;
  result.minute = timeInfo.tm_min;
  return result;
}

// ======================
// === OUTPUT UPDATES ===
// ======================
void updateOutputsFromWorking() {
  // Van
  for (uint8_t i = 0; i < 10; i++) {
    // LOW = ON, HIGH = OFF
    digitalWrite(relayValvePins[i], workingValveState[i] ? LOW : HIGH);
    Blynk.virtualWrite(i, workingValveState[i] ? 1 : 0);
  }
  // Kiểm tra nếu có van mở mới cho phép bơm
  bool anyOpen = anyValveOpen();
  // Bơm
  for (uint8_t j = 0; j < 2; j++) {
    if (!anyOpen) {
      workingPumpState[j] = false;
    }
    digitalWrite(relayPumpPins[j], workingPumpState[j] ? LOW : HIGH);
    Blynk.virtualWrite(10 + j, workingPumpState[j] ? 1 : 0);
  }
}

// ======================
// === MENU DRAWING ===
// ======================
void drawMainMenu() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("MENU CHINH");
  // Các lựa chọn: 0="CẤU HÌNH",1="THỦ CÔNG",2="TỰ ĐỘNG"
  const char *options[3] = {"CAU HINH", "THU CONG", "TU DONG"};
  for (uint8_t i = 0; i < 3; i++) {
    lcd.setCursor(0, i + 1);
    if (i == mainMenuIndex) {
      lcd.print(">");
    } else {
      lcd.print(" ");
    }
    if (i == 2) {
      // Hiển thị trạng thái AUTO bên cạnh
      lcd.print(options[i]);
      lcd.print(" (");
      lcd.print(workingAutoEnabled ? "ON-" : "OFF-");
      lcd.print(String(workingAutoStartIndex));
      lcd.print(")");
    } else {
      lcd.print(options[i]);
    }
  }
  currentAppState = STATE_MAIN;
}

void drawConfigMenu() {
  lcd.clear();
  // Tiêu đề
  if (currentConfigMode == MODE_ONOFF) {
    lcd.setCursor(0, 0);
    lcd.print("Mode 0: ON/OFF VAN");
    // Hiển thị 3 mục/trang
    uint8_t startIndex = configMenuPage * 3;
    for (uint8_t row = 0; row < 3; row++) {
      uint8_t idx = startIndex + row;
      if (idx >= 10) break;
      lcd.setCursor(0, row + 1);
      if (row == configMenuIndex) lcd.print(">");
      else lcd.print(" ");
      lcd.print("BEC ");
      lcd.print(String(idx + 1));
      lcd.print(": [");
      lcd.print(workingValveState[idx] ? "ON " : "OFF");
      lcd.print("]");
    }
    // Mũi tên lên nếu vẫn còn trang trước
    if (configMenuPage > 0) {
      lcd.setCursor(18, 1);
      lcd.print("^");
    }
    // Mũi tên xuống nếu còn trang sau
    if ((configMenuPage + 1) * 3 < 10) {
      lcd.setCursor(18, 3);
      lcd.print("v");
    }
  } else if (currentConfigMode == MODE_DELAY) {
    lcd.setCursor(0, 0);
    lcd.print("Mode 1: DELAY VAN");
    lcd.setCursor(0, 1);
    lcd.print("DELAY: ");
    lcd.print(String(workingDelaySec));
    lcd.print("s");
  } else if (currentConfigMode == MODE_POWERCHECK) {
    lcd.setCursor(0, 0);
    lcd.print("Mode 2: POWER CHECK");
    lcd.setCursor(0, 1);
    lcd.print("POWER CHECK: ");
    lcd.print(workingPowerCheckEnabled ? "ON " : "OFF");
  }
  currentAppState = STATE_CONFIG;
}

void drawManualMenu() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("THU CONG");
  lcd.setCursor(0, 1);
  // Hiển thị [HH:MM], phần đang chỉnh sẽ nhấp nháy
  if (editingManualHour) {
    lcd.print("[");
    if (manualBlinkState) {
      lcd.print("  ");
    } else {
      lcd.print(String(manualHour < 10 ? "0" : "") + String(manualHour));
    }
    lcd.print(":");
    lcd.print(String(manualMin < 10 ? "0" : "") + String(manualMin));
    lcd.print("]");
  } else {
    lcd.print("[");
    lcd.print(String(manualHour < 10 ? "0" : "") + String(manualHour));
    lcd.print(":");
    if (manualBlinkState) {
      lcd.print("  ");
    } else {
      lcd.print(String(manualMin < 10 ? "0" : "") + String(manualMin));
    }
    lcd.print("]");
  }
  currentAppState = STATE_MANUAL;
}

void drawAutoMenu() {
  lcd.clear();
  lcd.setCursor(0, 0);
  if (workingAutoEnabled) {
    lcd.print("TU DONG (ON-");
    lcd.print(String(workingAutoStartIndex));
    lcd.print(")");
  } else {
    lcd.print("TU DONG (OFF-");
    lcd.print(String(workingAutoStartIndex));
    lcd.print(")");
  }
  // Nếu đang ở giao diện chọn van
  if (inSelectValve) {
    lcd.setCursor(0, 1);
    lcd.print("SELECT VALVE: [");
    lcd.print(String(workingAutoStartIndex));
    lcd.print("]");
    return;
  }
  // Nếu đang ở giao diện chỉnh giờ
  if (inTimeSetup) {
    lcd.setCursor(0, 1);
    lcd.print("F[");
    if (autoSelectField == 0 && autoBlinkState) lcd.print("  ");
    else lcd.print(String(workingAutoFrom.hour < 10 ? "0" : "") + String(workingAutoFrom.hour));
    lcd.print(":");
    if (autoSelectField == 1 && autoBlinkState) lcd.print("  ");
    else lcd.print(String(workingAutoFrom.minute < 10 ? "0" : "") + String(workingAutoFrom.minute));
    lcd.print("]-T[");
    if (autoSelectField == 2 && autoBlinkState) lcd.print("  ");
    else lcd.print(String(workingAutoTo.hour < 10 ? "0" : "") + String(workingAutoTo.hour));
    lcd.print(":");
    if (autoSelectField == 3 && autoBlinkState) lcd.print("  ");
    else lcd.print(String(workingAutoTo.minute < 10 ? "0" : "") + String(workingAutoTo.minute));
    lcd.print("]");
    lcd.setCursor(0, 2);
    lcd.print("D[");
    if (autoSelectField == 4 && autoBlinkState) lcd.print("  ");
    else lcd.print(String(workingDuration.hour < 10 ? "0" : "") + String(workingDuration.hour));
    lcd.print(":");
    if (autoSelectField == 5 && autoBlinkState) lcd.print("  ");
    else lcd.print(String(workingDuration.minute < 10 ? "0" : "") + String(workingDuration.minute));
    lcd.print("]-R[");
    if (autoSelectField == 6 && autoBlinkState) lcd.print("  ");
    else lcd.print(String(workingRest.hour < 10 ? "0" : "") + String(workingRest.hour));
    lcd.print(":");
    if (autoSelectField == 7 && autoBlinkState) lcd.print("  ");
    else lcd.print(String(workingRest.minute < 10 ? "0" : "") + String(workingRest.minute));
    lcd.print("]");
    return;
  }
  // Danh sách menu chính
  const char *options[5] = {
    "AUTO_ON", "AUTO_OFF", "RESET", "SELECT VALVE", "TIME SETUP"
  };
  uint8_t pageStart = autoMenuPage * autoMenuItemsOnPage;
  uint8_t itemsOnPage = min(autoMenuItemsOnPage, (uint8_t)(autoMenuItems - pageStart));
  for (uint8_t row = 0; row < itemsOnPage; row++) {
    uint8_t idx = pageStart + row;
    lcd.setCursor(0, row + 1);
    if (row == (autoMenuSelectedIndex)) lcd.print(">");
    else lcd.print(" ");
    lcd.print(options[idx]);
  }
  // Dấu cuộn trang
  if (autoMenuPage > 0) {
    lcd.setCursor(18, 1);
    lcd.print("^");
  }
  if ((autoMenuPage + 1) * autoMenuItemsOnPage < autoMenuItems) {
    lcd.setCursor(18, 3);
    lcd.print("v");
  }
  currentAppState = STATE_AUTO;
}

// ======================
// === IR HANDLING ===
// ======================
void handleIR(uint32_t code) {
  switch (currentAppState) {
    case STATE_MAIN:
      handleMainIR(code);
      break;
    case STATE_CONFIG:
      handleConfigIR(code);
      break;
    case STATE_MANUAL:
      handleManualIR(code);
      break;
    case STATE_AUTO:
      handleAutoIR(code);
      break;
    default:
      drawMainMenu();
      break;
  }
}

void handleMainIR(uint32_t code) {
  if (code == IR_CODE_UP) {
    if (mainMenuIndex > 0) mainMenuIndex--;
    else mainMenuIndex = 2;
    drawMainMenu();
  } else if (code == IR_CODE_DOWN) {
    if (mainMenuIndex < 2) mainMenuIndex++;
    else mainMenuIndex = 0;
    drawMainMenu();
  } else if (code == IR_CODE_OK) {
    if (mainMenuIndex == 0) {
      // CONFIG
      currentConfigMode = MODE_ONOFF;
      configMenuPage = 0;
      configMenuIndex = 0;
      drawConfigMenu();
    } else if (mainMenuIndex == 1) {
      // MANUAL
      editingManualHour = true;
      drawManualMenu();
    } else if (mainMenuIndex == 2) {
      // AUTO
      autoMenuSelectedIndex = 0;
      drawAutoMenu();
    }
  }
}

void handleConfigIR(uint32_t code) {
  if (currentConfigMode == MODE_ONOFF) {
    uint8_t pageStart = configMenuPage * 3;
    uint8_t itemsOnPage = min(3, 10 - pageStart);
    if (code == IR_CODE_UP) {
      if (configMenuIndex > 0) {
        configMenuIndex--;
      } else if (configMenuPage > 0) {
        configMenuPage--;
        itemsOnPage = min(3, 10 - configMenuPage * 3);
        configMenuIndex = itemsOnPage - 1;
      }
      drawConfigMenu();
    } else if (code == IR_CODE_DOWN) {
      if (configMenuIndex < itemsOnPage - 1) {
        configMenuIndex++;
      } else if ((configMenuPage + 1) * 3 < 10) {
        configMenuPage++;
        configMenuIndex = 0;
      }
      drawConfigMenu();
    } else if (code == IR_CODE_LEFT) {
      if (currentConfigMode == MODE_ONOFF) {
        currentConfigMode = MODE_POWERCHECK;
      } else if (currentConfigMode == MODE_DELAY) {
        currentConfigMode = MODE_ONOFF;
      } else if (currentConfigMode == MODE_POWERCHECK) {
        currentConfigMode = MODE_DELAY;
      }
      drawConfigMenu();
    } else if (code == IR_CODE_RIGHT) {
      if (currentConfigMode == MODE_ONOFF) {
        currentConfigMode = MODE_DELAY;
      } else if (currentConfigMode == MODE_DELAY) {
        currentConfigMode = MODE_POWERCHECK;
      } else if (currentConfigMode == MODE_POWERCHECK) {
        currentConfigMode = MODE_ONOFF;
      }
      drawConfigMenu();
    } else if (code == IR_CODE_OK) {
      uint8_t actualIndex = pageStart + configMenuIndex;
      workingValveState[actualIndex] = !workingValveState[actualIndex];
      drawConfigMenu();
    } else if (code == IR_CODE_HASH) {
      // Commit changes
      memcpy(savedValveState, workingValveState, sizeof(savedValveState));
      saveConfigToPrefs();
      updateOutputsFromWorking();
      drawMainMenu();
    } else if (code == IR_CODE_STAR) {
      // Cancel
      memcpy(workingValveState, savedValveState, sizeof(savedValveState));
      updateOutputsFromWorking();
      drawMainMenu();
    }
  } else if (currentConfigMode == MODE_DELAY) {
    if (code == IR_CODE_UP) {
      if (workingDelaySec < 65535) workingDelaySec++;
      drawConfigMenu();
    } else if (code == IR_CODE_DOWN) {
      if (workingDelaySec > 1) workingDelaySec--;
      drawConfigMenu();
    } else if (code == IR_CODE_HASH) {
      savedDelaySec = workingDelaySec;
      saveConfigToPrefs();
      drawMainMenu();
    } else if (code == IR_CODE_STAR) {
      workingDelaySec = savedDelaySec;
      drawMainMenu();
    } else if (code == IR_CODE_LEFT) {
      currentConfigMode = MODE_ONOFF;
      configMenuPage = 0;
      configMenuIndex = 0;
      drawConfigMenu();
    } else if (code == IR_CODE_RIGHT) {
      currentConfigMode = MODE_POWERCHECK;
      drawConfigMenu();
    }
  } else if (currentConfigMode == MODE_POWERCHECK) {
    if (code == IR_CODE_OK) {
      workingPowerCheckEnabled = !workingPowerCheckEnabled;
      drawConfigMenu();
    } else if (code == IR_CODE_HASH) {
      savedPowerCheckEnabled = workingPowerCheckEnabled;
      saveConfigToPrefs();
      drawMainMenu();
    } else if (code == IR_CODE_STAR) {
      workingPowerCheckEnabled = savedPowerCheckEnabled;
      drawMainMenu();
    } else if (code == IR_CODE_LEFT) {
      currentConfigMode = MODE_DELAY;
      drawConfigMenu();
    } else if (code == IR_CODE_RIGHT) {
      currentConfigMode = MODE_ONOFF;
      configMenuPage = 0;
      configMenuIndex = 0;
      drawConfigMenu();
    }
  }
}

void handleManualIR(uint32_t code) {
  if (code == IR_CODE_UP) {
    if (editingManualHour) {
      if (manualHour < 99) manualHour++;
      else manualHour = 0;
    } else {
      // minute step = 15
      if (manualMin <= 45) manualMin += 15;
      else manualMin = 0;
    }
    drawManualMenu();
  } else if (code == IR_CODE_DOWN) {
    if (editingManualHour) {
      if (manualHour > 0) manualHour--;
      else manualHour = 99;
    } else {
      if (manualMin >= 15) manualMin -= 15;
      else manualMin = 45;
    }
    drawManualMenu();
  } else if (code == IR_CODE_OK) {
    editingManualHour = !editingManualHour;
    drawManualMenu();
  } else if (code == IR_CODE_HASH) {
    // Schedule manual watering
    uint32_t delayMs = (uint32_t)manualHour * 3600000UL + (uint32_t)manualMin * 60000UL;
    nextManualMillis = millis() + delayMs;
    manualScheduled = true;
    drawMainMenu();
  } else if (code == IR_CODE_STAR) {
    drawMainMenu();
  }
}

void handleAutoIR(uint32_t code) {
  if (code == IR_CODE_STAR) {
    if (inSelectValve || inTimeSetup) {
      inSelectValve = false;
      inTimeSetup = false;
      drawAutoMenu();
      return;
    }
    drawMainMenu();
    currentAppState = STATE_MAIN;
    return;
  }
  // Nếu đang ở giao diện chọn van
  if (inSelectValve) {
    if (code == IR_CODE_UP) {
      if (workingAutoStartIndex < 10) workingAutoStartIndex++;
      else workingAutoStartIndex = 1;
      drawAutoMenu();
    } else if (code == IR_CODE_DOWN) {
      if (workingAutoStartIndex > 1) workingAutoStartIndex--;
      else workingAutoStartIndex = 10;
      drawAutoMenu();
    } else if (code == IR_CODE_HASH) {
      savedAutoStartIndex = workingAutoStartIndex;
      saveConfigToPrefs();
      inSelectValve = false;
      drawAutoMenu();
    } else if (code == IR_CODE_STAR || code == IR_CODE_LEFT) {
      workingAutoStartIndex = savedAutoStartIndex;
      inSelectValve = false;
      drawAutoMenu();
    }
    return;
  }
  // Nếu đang ở giao diện chỉnh giờ
  if (inTimeSetup) {
    if (code == IR_CODE_OK) {
      autoSelectField = (autoSelectField + 1) % 8;
      drawAutoMenu();
    } else if (code == IR_CODE_UP) {
      switch (autoSelectField) {
        case 0:
          if (workingAutoFrom.hour < 23) workingAutoFrom.hour++;
          else workingAutoFrom.hour = 0;
          break;
        case 1:
          if (workingAutoFrom.minute < 45) workingAutoFrom.minute += 15;
          else workingAutoFrom.minute = 0;
          break;
        case 2:
          if (workingAutoTo.hour < 23) workingAutoTo.hour++;
          else workingAutoTo.hour = 0;
          break;
        case 3:
          if (workingAutoTo.minute < 45) workingAutoTo.minute += 15;
          else workingAutoTo.minute = 0;
          break;
        case 4:
          if (workingDuration.hour < 23) workingDuration.hour++;
          else workingDuration.hour = 0;
          break;
        case 5:
          if (workingDuration.minute < 45) workingDuration.minute += 15;
          else workingDuration.minute = 0;
          break;
        case 6:
          if (workingRest.hour < 23) workingRest.hour++;
          else workingRest.hour = 0;
          break;
        case 7:
          if (workingRest.minute < 45) workingRest.minute += 15;
          else workingRest.minute = 0;
          break;
      }
      drawAutoMenu();
    } else if (code == IR_CODE_DOWN) {
      switch (autoSelectField) {
        case 0:
          if (workingAutoFrom.hour > 0) workingAutoFrom.hour--;
          else workingAutoFrom.hour = 23;
          break;
        case 1:
          if (workingAutoFrom.minute >= 15) workingAutoFrom.minute -= 15;
          else workingAutoFrom.minute = 45;
          break;
        case 2:
          if (workingAutoTo.hour > 0) workingAutoTo.hour--;
          else workingAutoTo.hour = 23;
          break;
        case 3:
          if (workingAutoTo.minute >= 15) workingAutoTo.minute -= 15;
          else workingAutoTo.minute = 45;
          break;
        case 4:
          if (workingDuration.hour > 0) workingDuration.hour--;
          else workingDuration.hour = 23;
          break;
        case 5:
          if (workingDuration.minute >= 15) workingDuration.minute -= 15;
          else workingDuration.minute = 45;
          break;
        case 6:
          if (workingRest.hour > 0) workingRest.hour--;
          else workingRest.hour = 23;
          break;
        case 7:
          if (workingRest.minute >= 15) workingRest.minute -= 15;
          else workingRest.minute = 45;
          break;
      }
      drawAutoMenu();
    } else if (code == IR_CODE_HASH) {
      // Commit all time settings
      savedAutoFrom = workingAutoFrom;
      savedAutoTo   = workingAutoTo;
      savedDuration = workingDuration;
      savedRest     = workingRest;
      saveConfigToPrefs();
      inTimeSetup = false;
      drawAutoMenu();
    } else if (code == IR_CODE_STAR || code == IR_CODE_LEFT) {
      // Revert
      workingAutoFrom = savedAutoFrom;
      workingAutoTo   = savedAutoTo;
      workingDuration = savedDuration;
      workingRest     = savedRest;
      inTimeSetup = false;
      drawAutoMenu();
    }
    return;
  }
  // Xử lý menu chính auto
  uint8_t pageStart = autoMenuPage * autoMenuItemsOnPage;
  uint8_t itemsOnPage = min(autoMenuItemsOnPage, (uint8_t)(autoMenuItems - pageStart));
  if (code == IR_CODE_UP) {
    if (autoMenuSelectedIndex > 0) {
      autoMenuSelectedIndex--;
    } else if (autoMenuPage > 0) {
      autoMenuPage--;
      itemsOnPage = min(autoMenuItemsOnPage, (uint8_t)(autoMenuItems - autoMenuPage * autoMenuItemsOnPage));
      autoMenuSelectedIndex = itemsOnPage - 1;
    }
    drawAutoMenu();
  } else if (code == IR_CODE_DOWN) {
    if (autoMenuSelectedIndex < itemsOnPage - 1) {
      autoMenuSelectedIndex++;
    } else if ((autoMenuPage + 1) * autoMenuItemsOnPage < autoMenuItems) {
      autoMenuPage++;
      autoMenuSelectedIndex = 0;
    }
    drawAutoMenu();
  } else if (code == IR_CODE_OK) {
    uint8_t actualIndex = pageStart + autoMenuSelectedIndex;
    if (actualIndex == 0) {
      workingAutoEnabled = true;
      savedAutoEnabled = true;
      saveConfigToPrefs();
      drawMainMenu();
      currentAppState = STATE_MAIN;
      return;
    } else if (actualIndex == 1) {
      workingAutoEnabled = false;
      savedAutoEnabled = false;
      saveConfigToPrefs();
      drawMainMenu();
      currentAppState = STATE_MAIN;
      return;
    } else if (actualIndex == 2) {
      workingAutoEnabled = false;
      savedAutoEnabled = false;
      workingAutoStartIndex = 1;
      savedAutoStartIndex = 1;
      saveConfigToPrefs();
      drawMainMenu();
      currentAppState = STATE_MAIN;
      return;
    } else if (actualIndex == 3) {
      inSelectValve = true;
      drawAutoMenu();
      return;
    } else if (actualIndex == 4) {
      inTimeSetup = true;
      drawAutoMenu();
      return;
    }
  }
}

// ======================
// === BLYNK CALLBACKS ===
// ======================
BLYNK_WRITE(V0) {
  workingValveState[0] = param.asInt();
  updateOutputsFromWorking();
}
BLYNK_WRITE(V1) {
  workingValveState[1] = param.asInt();
  updateOutputsFromWorking();
}
BLYNK_WRITE(V2) {
  workingValveState[2] = param.asInt();
  updateOutputsFromWorking();
}
BLYNK_WRITE(V3) {
  workingValveState[3] = param.asInt();
  updateOutputsFromWorking();
}
BLYNK_WRITE(V4) {
  workingValveState[4] = param.asInt();
  updateOutputsFromWorking();
}
BLYNK_WRITE(V5) {
  workingValveState[5] = param.asInt();
  updateOutputsFromWorking();
}
BLYNK_WRITE(V6) {
  workingValveState[6] = param.asInt();
  updateOutputsFromWorking();
}
BLYNK_WRITE(V7) {
  workingValveState[7] = param.asInt();
  updateOutputsFromWorking();
}
BLYNK_WRITE(V8) {
  workingValveState[8] = param.asInt();
  updateOutputsFromWorking();
}
BLYNK_WRITE(V9) {
  workingValveState[9] = param.asInt();
  updateOutputsFromWorking();
}
BLYNK_WRITE(V10) {
  workingPumpState[0] = param.asInt();
  updateOutputsFromWorking();
}
BLYNK_WRITE(V11) {
  workingPumpState[1] = param.asInt();
  updateOutputsFromWorking();
}
