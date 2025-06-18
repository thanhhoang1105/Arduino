/*
  ESP32 Coffee Garden Irrigation System
  - 10 Valves, 2 Pumps (Relays)
  - IR Remote Control for Menu Navigation
  - LCD 20x4 via I²C (PCF8574) using LiquidCrystal_PCF8574
  - ACS712 Current Sensor for Power Check
  - Blynk Integration (12 Switches: 10 Valves + 2 Pumps)
  - Preferences (NVS) for Saving Configurations
  - Manual (Time-based start) and Automatic (Time-Based) Irrigation Modes
  - WiFi Manager for easy network configuration
*/

// ======================
// === BLYNK DEFINES ===
// ======================
#define BLYNK_TEMPLATE_ID "TMPL6xy4mK0KN"
#define BLYNK_TEMPLATE_NAME "Smart Irrigation Controller"
#define BLYNK_AUTH_TOKEN "J34tRXWg_QTXt8bs0SfbZFSc1ARUpL0o"
#define BLYNK_PRINT Serial // Redirect Blynk logs to Serial

// ======================
// === INCLUDES ===
// ======================
#include <Arduino.h>
#include <Preferences.h>
#include <Wire.h>
#include <LiquidCrystal_PCF8574.h>
#include <IRremote.h>
#include <BlynkSimpleEsp32.h>
#include <time.h>
#include <WiFi.h>
#include <WiFiManager.h>

// ======================
// === HARDWARE PINS ===
// ======================
// Relay pins: 10 Valves, 2 Pumps (Active LOW)
static const uint8_t relayValvePins[10] = {2, 4, 16, 17, 5, 18, 19, 21, 22, 23};
static const uint8_t relayPumpPins[2]   = {26, 25};

// IR Receiver
static const uint8_t IR_RECEIVER_PIN = 15;

// LCD (I2C PCF8574)
static const uint8_t LCD_I2C_ADDR = 0x27;
static const uint8_t LCD_SDA_PIN = 13;
static const uint8_t LCD_SCL_PIN = 14;
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

#define IR_CODE_STAR   0xE916FF00UL  // CANCEL / BACK
#define IR_CODE_HASH   0xF20DFF00UL  // SAVE / CONFIRM

#define IR_CODE_UP     0xE718FF00UL
#define IR_CODE_DOWN   0xAD52FF00UL
#define IR_CODE_RIGHT  0xA55AFF00UL
#define IR_CODE_LEFT   0xF708FF00UL
#define IR_CODE_OK     0xE31CFF00UL

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
// === GLOBAL STATE & CONFIGURATION ===
// ======================
Preferences prefs;

// Relay states (saved in Preferences, working is for current edits)
bool savedValveState[10];
bool workingValveState[10];
bool savedPumpState[2];
bool workingPumpState[2];

// Delay (seconds) for manual irrigation cycle duration and auto mode valve step
uint16_t savedDelaySec;
uint16_t workingDelaySec;

// Power Check (ACS712)
bool savedPowerCheckEnabled;
bool workingPowerCheckEnabled;

// Automatic mode settings
bool savedAutoEnabled;
bool workingAutoEnabled;

uint8_t savedAutoStartIndex;  // 1-10 for display, internally 0-9
uint8_t workingAutoStartIndex;

TimeHM savedAutoFrom, workingAutoFrom;
TimeHM savedAutoTo, workingAutoTo;
TimeHM savedDuration, workingDuration;
TimeHM savedRest, workingRest;

// Manual mode (time-based start for a single cycle using savedValveState)
uint8_t manualHour;
uint8_t manualMin;
bool editingManualHour;
bool manualScheduled;
bool isManualRunning;
unsigned long manualStartMillis;

// Numeric input for IR
uint8_t irNumericBuffer = 0;
uint8_t irNumericDigits = 0;

// Application navigation state
AppState currentAppState = STATE_MAIN;
uint8_t mainMenuIndex = 0;       // 0=Config, 1=Manual, 2=Auto

ConfigMode currentConfigMode = MODE_ONOFF;
uint8_t configMenuPage = 0;      // For ON/OFF mode paging
uint8_t configMenuIndex = 0;     // Index within current page

// Auto mode navigation and operation
uint8_t autoMenuIndex = 0;        // Current selected item in auto menu
uint8_t autoMenuPage = 0;         // Current page in auto menu
bool inSelectValve = false;       // Auto valve selection mode
bool inTimeSetup = false;         // Auto time setup mode
uint8_t autoSelectField = 0;      // Field for time selection (0-7)

// Operational flags
bool autoRunning = false;
unsigned long autoLastActionMillis = 0;
uint8_t autoCurrentValve = 0;     // 0-9 index
bool autoStep = false;            // false = valve on, true = rest period

// Blink state for UI elements
unsigned long lastBlinkMillis = 0;
bool blinkState = false;

// Power check variables
const float POWER_THRESHOLD_AMPS = 0.1;
unsigned long lastPowerCheckMillis = 0;
const unsigned long POWER_CHECK_INTERVAL = 2000; // Check every 2 seconds

// WiFi and Blynk
char blynkAuthToken[] = BLYNK_AUTH_TOKEN;

// NTP settings
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 7 * 3600; // GMT+7 (Vietnam)
const int daylightOffset_sec = 0;

// ======================
// === FUNCTION DECLARATIONS ===
// ======================
void initializeHardware();
void initializeWiFi();
void initializeTime();
void loadConfigFromPrefs();
void saveConfigToPrefs();
void updateOutputsFromWorking();
void drawMainMenu();
void drawConfigMenu();
void drawManualMenu();
void drawAutoMenu();
void handleIR(uint32_t code);
void handleMainIR(uint32_t code);
void handleConfigIR(uint32_t code);
void handleManualIR(uint32_t code);
void handleAutoIR(uint32_t code);
float readACS712Current();
void turnAllOff();
bool isAnyValveOpen();
bool anySavedValveOpen();
bool nowInTimeRange(const TimeHM &from, const TimeHM &to);
TimeHM getCurrentTimeHM();
bool nowMatchesManualTime();
void checkPowerStatus();
void runManualIrrigation();
void runAutoIrrigation();
void resetIRNumericInput();

// ======================
// === BLYNK VIRTUAL PIN HANDLERS ===
// ======================
BLYNK_WRITE(V0) { workingValveState[0] = param.asInt(); updateOutputsFromWorking(); }
BLYNK_WRITE(V1) { workingValveState[1] = param.asInt(); updateOutputsFromWorking(); }
BLYNK_WRITE(V2) { workingValveState[2] = param.asInt(); updateOutputsFromWorking(); }
BLYNK_WRITE(V3) { workingValveState[3] = param.asInt(); updateOutputsFromWorking(); }
BLYNK_WRITE(V4) { workingValveState[4] = param.asInt(); updateOutputsFromWorking(); }
BLYNK_WRITE(V5) { workingValveState[5] = param.asInt(); updateOutputsFromWorking(); }
BLYNK_WRITE(V6) { workingValveState[6] = param.asInt(); updateOutputsFromWorking(); }
BLYNK_WRITE(V7) { workingValveState[7] = param.asInt(); updateOutputsFromWorking(); }
BLYNK_WRITE(V8) { workingValveState[8] = param.asInt(); updateOutputsFromWorking(); }
BLYNK_WRITE(V9) { workingValveState[9] = param.asInt(); updateOutputsFromWorking(); }
BLYNK_WRITE(V10) { workingPumpState[0] = param.asInt(); updateOutputsFromWorking(); }
BLYNK_WRITE(V11) { workingPumpState[1] = param.asInt(); updateOutputsFromWorking(); }

// ======================
// === SETUP ===
// ======================
void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\n[SETUP] He Thong Tuoi Vuon Ca Phe - Khoi Dong...");

  initializeHardware();

  prefs.begin("cfg", false); // Namespace for preferences
  loadConfigFromPrefs(); // Load saved settings

  initializeWiFi();
  initializeTime();

  IrReceiver.begin(IR_RECEIVER_PIN, ENABLE_LED_FEEDBACK); // Start IR receiver

  // Initialize defaults for manual and auto modes
  manualScheduled = false;
  isManualRunning = false;
  editingManualHour = true;
  manualHour = 0;
  manualMin = 0;

  Serial.println("[SETUP] Khoi dong hoan tat.");
  drawMainMenu();
  updateOutputsFromWorking(); // Sync relays and Blynk on startup
}

// ======================
// === MAIN LOOP ===
// ======================
void loop() {
  Blynk.run(); // Process Blynk tasks

  // Process IR remote input
  if (IrReceiver.decode()) {
    uint32_t irCode = IrReceiver.decodedIRData.decodedRawData;
    if (irCode != 0) {
      Serial.print("[IR] Ma nhan duoc: 0x");
      Serial.println(irCode, HEX);
      handleIR(irCode);
    }
    IrReceiver.resume(); // Ready for next IR signal
  }

  // UI Blinking Logic
  if (millis() - lastBlinkMillis > 500) {
    lastBlinkMillis = millis();
    blinkState = !blinkState;

    // Redraw UI if it has blinking elements
    if ((currentAppState == STATE_MANUAL) ||
        (currentAppState == STATE_AUTO && inTimeSetup)) {
      if (currentAppState == STATE_MANUAL) {
        drawManualMenu();
      } else if (currentAppState == STATE_AUTO) {
        drawAutoMenu();
      }
    }
  }

  // Periodic Power Check
  if (workingPowerCheckEnabled && (millis() - lastPowerCheckMillis > POWER_CHECK_INTERVAL)) {
    lastPowerCheckMillis = millis();
    checkPowerStatus();
  }

  // Manual Irrigation Logic
  if (manualScheduled && !isManualRunning) {
    if (nowMatchesManualTime()) {
      runManualIrrigation();
    }
  }

  // Check if manual irrigation is running and needs to be stopped
  if (isManualRunning) {
    if (millis() - manualStartMillis >= savedDelaySec * 1000UL) {
      // Stop irrigation
      for (uint8_t i = 0; i < 10; i++) {
        workingValveState[i] = false;
      }
      updateOutputsFromWorking();
      isManualRunning = false;
      manualScheduled = false;

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("TUOI THU CONG XONG");
      delay(2000);
      currentAppState = STATE_MAIN;
      drawMainMenu();
    }
  }

  // Automatic Irrigation Logic
  if (savedAutoEnabled) {
    runAutoIrrigation();
  }

  delay(20); // Small delay to yield CPU
}

// ======================
// === INITIALIZATION FUNCTIONS ===
// ======================
void initializeHardware() {
  // Initialize relay pins (Active LOW: LOW = ON, HIGH = OFF)
  for (uint8_t i = 0; i < 10; i++) {
    pinMode(relayValvePins[i], OUTPUT);
    digitalWrite(relayValvePins[i], HIGH);  // Default OFF
  }
  for (uint8_t j = 0; j < 2; j++) {
    pinMode(relayPumpPins[j], OUTPUT);
    digitalWrite(relayPumpPins[j], HIGH);  // Default OFF
  }

  // Initialize LCD
  Wire.begin(LCD_SDA_PIN, LCD_SCL_PIN);
  lcd.begin(20, 4); // 20 columns, 4 rows
  lcd.setBacklight(255); // Max backlight
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("HE THONG TUOI VUON");
  lcd.setCursor(0, 1);
  lcd.print("DANG KHOI DONG...");

  Serial.println("[HW] Hardware initialized.");
}

void initializeWiFi() {
  Serial.println("[WIFI] Connecting to WiFi...");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("KET NOI WIFI...");

  // Use WiFiManager for easier WiFi setup
  WiFiManager wm;
  wm.setConfigPortalTimeout(180); // Portal stays active for 3 minutes

  if (!wm.autoConnect("IrrigationSystem")) {
    Serial.println("[WIFI] Failed to connect WiFi, retrying...");
    lcd.setCursor(0, 1);
    lcd.print("KHONG THE KET NOI!");
    delay(2000);
    ESP.restart();
  }

  Serial.print("[WIFI] Connected! IP: ");
  Serial.println(WiFi.localIP());

  lcd.setCursor(0, 1);
  lcd.print("WIFI OK: ");
  lcd.print(WiFi.SSID());

  lcd.setCursor(0, 2);
  lcd.print("KET NOI BLYNK...");

  // Initialize Blynk
  Blynk.config(blynkAuthToken);
  if (Blynk.connect()) {
    Serial.println("[BLYNK] Connected.");
    lcd.print(" OK");
  } else {
    Serial.println("[BLYNK] Connection failed.");
    lcd.print(" LOI");
  }
  delay(1000);
}

void initializeTime() {
  Serial.println("[TIME] Setting up NTP time...");
  lcd.setCursor(0, 3);
  lcd.print("CAI DAT GIO...");

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    Serial.print("[TIME] Current time: ");
    Serial.println(&timeinfo, "%H:%M:%S");
    lcd.print(" OK");
  } else {
    Serial.println("[TIME] Failed to obtain time.");
    lcd.print(" LOI");
  }
  delay(1000);
}

// ======================
// === PREFERENCES (NVS Storage) ===
// ======================
void loadConfigFromPrefs() {
  // Valves
  for (uint8_t i = 0; i < 10; i++) {
    String key = "v" + String(i);
    savedValveState[i] = prefs.getBool(key.c_str(), false);
    workingValveState[i] = savedValveState[i];
  }

  // Pumps
  savedPumpState[0] = prefs.getBool("p0", false);
  savedPumpState[1] = prefs.getBool("p1", false);
  workingPumpState[0] = savedPumpState[0];
  workingPumpState[1] = savedPumpState[1];

  // Delay
  savedDelaySec = prefs.getUInt("delay", 10);  // Default 10s
  workingDelaySec = savedDelaySec;

  // Power Check
  savedPowerCheckEnabled = prefs.getBool("power", false);
  workingPowerCheckEnabled = savedPowerCheckEnabled;

  // Auto mode
  savedAutoEnabled = prefs.getBool("ae", false);
  workingAutoEnabled = savedAutoEnabled;

  savedAutoStartIndex = prefs.getUInt("as", 1);  // 1-10 for display
  workingAutoStartIndex = savedAutoStartIndex;

  savedAutoFrom.hour = prefs.getUInt("afh", 6);    // 6:00 AM default
  savedAutoFrom.minute = prefs.getUInt("afm", 0);
  workingAutoFrom = savedAutoFrom;

  savedAutoTo.hour = prefs.getUInt("ath", 8);      // 8:00 AM default
  savedAutoTo.minute = prefs.getUInt("atm", 0);
  workingAutoTo = savedAutoTo;

  savedDuration.hour = prefs.getUInt("adh", 0);    // 5 minutes default
  savedDuration.minute = prefs.getUInt("adm", 5);
  workingDuration = savedDuration;

  savedRest.hour = prefs.getUInt("arh", 0);        // 1 minute default
  savedRest.minute = prefs.getUInt("arm", 1);
  workingRest = savedRest;

  Serial.println("[PREFS] Configuration loaded.");
}

void saveConfigToPrefs() {
  // Valves
  for (uint8_t i = 0; i < 10; i++) {
    String key = "v" + String(i);
    prefs.putBool(key.c_str(), workingValveState[i]);
    savedValveState[i] = workingValveState[i];
  }

  // Pumps
  prefs.putBool("p0", workingPumpState[0]);
  prefs.putBool("p1", workingPumpState[1]);
  savedPumpState[0] = workingPumpState[0];
  savedPumpState[1] = workingPumpState[1];

  // Delay
  prefs.putUInt("delay", workingDelaySec);
  savedDelaySec = workingDelaySec;

  // Power Check
  prefs.putBool("power", workingPowerCheckEnabled);
  savedPowerCheckEnabled = workingPowerCheckEnabled;

  // Auto mode
  prefs.putBool("ae", workingAutoEnabled);
  savedAutoEnabled = workingAutoEnabled;

  prefs.putUInt("as", workingAutoStartIndex);
  savedAutoStartIndex = workingAutoStartIndex;

  prefs.putUInt("afh", workingAutoFrom.hour);
  prefs.putUInt("afm", workingAutoFrom.minute);
  savedAutoFrom = workingAutoFrom;

  prefs.putUInt("ath", workingAutoTo.hour);
  prefs.putUInt("atm", workingAutoTo.minute);
  savedAutoTo = workingAutoTo;

  prefs.putUInt("adh", workingDuration.hour);
  prefs.putUInt("adm", workingDuration.minute);
  savedDuration = workingDuration;

  prefs.putUInt("arh", workingRest.hour);
  prefs.putUInt("arm", workingRest.minute);
  savedRest = workingRest;

  Serial.println("[PREFS] Configuration saved.");
}

// ======================
// === OUTPUT & SENSOR FUNCTIONS ===
// ======================
void updateOutputsFromWorking() {
  // Valves (Active LOW: LOW = ON, HIGH = OFF)
  for (uint8_t i = 0; i < 10; i++) {
    digitalWrite(relayValvePins[i], workingValveState[i] ? LOW : HIGH);
    if (Blynk.connected()) {
      Blynk.virtualWrite(i, workingValveState[i] ? 1 : 0);
    }
  }

  // Check if any valve is open before allowing pump activation
  bool anyValveOpen = isAnyValveOpen();

  // Pumps (Active LOW: LOW = ON, HIGH = OFF)
  for (uint8_t j = 0; j < 2; j++) {
    // Prevent pumps from running if no valves are open
    if (!anyValveOpen) {
      workingPumpState[j] = false;
    }
    digitalWrite(relayPumpPins[j], workingPumpState[j] ? LOW : HIGH);
    if (Blynk.connected()) {
      Blynk.virtualWrite(10 + j, workingPumpState[j] ? 1 : 0);
    }
  }
}

float readACS712Current() {
  // Read multiple samples for accuracy
  const int samples = 10;
  int total = 0;

  for (int i = 0; i < samples; i++) {
    total += analogRead(ACS712_PIN);
    delay(1);
  }

  int average = total / samples;

  // Convert ADC value to voltage
  float voltage = (average / 4095.0) * 3.3;

  // Convert voltage to current (ACS712 30A model)
  // Sensitivity: 66 mV/A, Offset: 2.5V (when 0A)
  float offset = 2.5;
  float sensitivity = 0.066;
  float current = abs((voltage - offset) / sensitivity);

  return current;
}

void turnAllOff() {
  // Turn off all valves and pumps
  for (uint8_t i = 0; i < 10; i++) {
    workingValveState[i] = false;
  }
  for (uint8_t j = 0; j < 2; j++) {
    workingPumpState[j] = false;
  }
  updateOutputsFromWorking();

  // Reset operational states
  isManualRunning = false;
  manualScheduled = false;
  autoRunning = false;

  Serial.println("[SYSTEM] All relays turned off.");
}

// ======================
// === UTILITY FUNCTIONS ===
// ======================
bool isAnyValveOpen() {
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
  TimeHM now = getCurrentTimeHM();

  // Convert all to minutes for easier comparison
  int nowMinutes = now.hour * 60 + now.minute;
  int fromMinutes = from.hour * 60 + from.minute;
  int toMinutes = to.hour * 60 + to.minute;

  if (fromMinutes <= toMinutes) {
    // Normal time range (e.g., 08:00 to 17:00)
    return (nowMinutes >= fromMinutes && nowMinutes <= toMinutes);
  } else {
    // Time range spans midnight (e.g., 22:00 to 06:00)
    return (nowMinutes >= fromMinutes || nowMinutes <= toMinutes);
  }
}

TimeHM getCurrentTimeHM() {
  TimeHM currentTime = {0, 0};
  struct tm timeinfo;

  if (getLocalTime(&timeinfo)) {
    currentTime.hour = timeinfo.tm_hour;
    currentTime.minute = timeinfo.tm_min;
  }

  return currentTime;
}

bool nowMatchesManualTime() {
  TimeHM now = getCurrentTimeHM();
  return (now.hour == manualHour && now.minute == manualMin);
}

void checkPowerStatus() {
  float current = readACS712Current();
  Serial.print("[POWER] Current reading: ");
  Serial.print(current, 2);
  Serial.println(" A");

  if (current < POWER_THRESHOLD_AMPS) {
    Serial.println("[POWER] CAUTION: Power loss detected!");
    turnAllOff();

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("MAT NGUON!");
    lcd.setCursor(0, 1);
    lcd.print("KIEM TRA NGUON DIEN");
    delay(3000);

    currentAppState = STATE_MAIN;
    drawMainMenu();
  }
}

void resetIRNumericInput() {
  irNumericBuffer = 0;
  irNumericDigits = 0;
}

// ======================
// === IRRIGATION HANDLERS ===
// ======================
void runManualIrrigation() {
  Serial.println("[MANUAL] Starting manual irrigation cycle.");

  // Set status flags
  isManualRunning = true;
  manualStartMillis = millis();

  // Apply saved valve configuration
  for (uint8_t i = 0; i < 10; i++) {
    workingValveState[i] = savedValveState[i];
  }
  updateOutputsFromWorking();

  // Display status
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("TUOI THU CONG");
  lcd.setCursor(0, 1);
  lcd.print("DANG CHAY...");
  lcd.setCursor(0, 2);
  lcd.print("Thoi gian: ");
  lcd.print(savedDelaySec);
  lcd.print("s");
}

void runAutoIrrigation() {
  // Check if within the configured time range
  if (nowInTimeRange(savedAutoFrom, savedAutoTo)) {
    if (!autoRunning) {
      // Start the auto cycle
      autoRunning = true;
      autoCurrentValve = savedAutoStartIndex - 1;  // Convert to 0-9 index
      autoStep = false;  // Start with valve step
      autoLastActionMillis = millis();

      // Display status
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("TU DONG BAT DAU");
      delay(1500);
      drawMainMenu();
    }

    // Process the automatic irrigation sequence
    if (autoRunning) {
      // If in valve step (irrigating)
      if (!autoStep) {
        if (millis() - autoLastActionMillis >= ((uint32_t)savedDuration.hour * 3600UL + savedDuration.minute * 60UL) * 1000UL) {
          // Valve duration complete, move to rest step
          for (uint8_t i = 0; i < 10; i++) {
            workingValveState[i] = false;
          }
          updateOutputsFromWorking();

          autoStep = true;  // Switch to rest step
          autoLastActionMillis = millis();

          // Display status
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("TU DONG: NGHI");
          lcd.setCursor(0, 1);
          lcd.print("Thoi gian: ");
          lcd.print(savedRest.hour > 0 ? String(savedRest.hour) + "h " : "");
          lcd.print(savedRest.minute);
          lcd.print("m");
        }
      }
      // If in rest step
      else {
        if (millis() - autoLastActionMillis >= ((uint32_t)savedRest.hour * 3600UL + savedRest.minute * 60UL) * 1000UL) {
          // Rest complete, move to next valve
          autoCurrentValve = (autoCurrentValve + 1) % 10;  // Cycle through valves 0-9

          // If we've gone full circle, restart from the configured start valve
          if (autoCurrentValve == savedAutoStartIndex - 1) {
            // We've completed a full cycle - check if we're still in the auto time range
            if (!nowInTimeRange(savedAutoFrom, savedAutoTo)) {
              autoRunning = false;
              turnAllOff();

              lcd.clear();
              lcd.setCursor(0, 0);
              lcd.print("TU DONG KET THUC");
              delay(2000);
              drawMainMenu();
              return;
            }
          }

          // Turn on the next valve
          for (uint8_t i = 0; i < 10; i++) {
            workingValveState[i] = (i == autoCurrentValve);
          }
          updateOutputsFromWorking();

          autoStep = false;  // Switch to valve step
          autoLastActionMillis = millis();

          // Display status
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("TU DONG: VAN ");
          lcd.print(autoCurrentValve + 1);  // Display 1-10
          lcd.setCursor(0, 1);
          lcd.print("DANG CHAY...");
          lcd.setCursor(0, 2);
          lcd.print("Thoi gian: ");
          lcd.print(savedDuration.hour > 0 ? String(savedDuration.hour) + "h " : "");
          lcd.print(savedDuration.minute);
          lcd.print("m");
        }
      }
    }
  }
  // Outside auto time range
  else if (autoRunning) {
    autoRunning = false;
    turnAllOff();

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("TU DONG NGOAI GIO");
    lcd.setCursor(0, 1);
    lcd.print("TAT TAT CA");
    delay(2000);
    drawMainMenu();
  }
}

// ======================
// === MENU DRAWING ===
// ======================
void drawMainMenu() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("MENU CHINH");

  const char *options[3] = {"CAU HINH", "THU CONG", "TU DONG"};
  for (uint8_t i = 0; i < 3; i++) {
    lcd.setCursor(0, i + 1);
    lcd.print(i == mainMenuIndex ? ">" : " ");
    lcd.print(options[i]);

    // Show auto mode status if on the auto menu option
    if (i == 2) {
      lcd.print(" (");
      lcd.print(savedAutoEnabled ? "ON-" : "OFF-");
      lcd.print(savedAutoStartIndex);
      lcd.print(")");
    }
  }

  currentAppState = STATE_MAIN;
}

void drawConfigMenu() {
  lcd.clear();

  // Declare variables outside switch statement
  uint8_t startIndex;

  switch (currentConfigMode) {
    case MODE_ONOFF:
      lcd.setCursor(0, 0);
      lcd.print("Mode 0: ON/OFF VAN");

      // Show up to 3 valves per page
      startIndex = configMenuPage * 3;
      for (uint8_t row = 0; row < 3; row++) {
        uint8_t valveIndex = startIndex + row;
        if (valveIndex >= 10) break;  // Don't show beyond 10 valves

        lcd.setCursor(0, row + 1);
        lcd.print(row == configMenuIndex ? ">" : " ");
        lcd.print("BEC ");
        lcd.print(valveIndex + 1);  // Display as 1-10
        lcd.print(": [");
        lcd.print(workingValveState[valveIndex] ? "ON " : "OFF");
        lcd.print("]");
      }

      // Show navigation arrows if needed
      if (configMenuPage > 0) {
        lcd.setCursor(18, 1);
        lcd.print("^");
      }
      if ((configMenuPage + 1) * 3 < 10) {
        lcd.setCursor(18, 3);
        lcd.print("v");
      }
      break;

    case MODE_DELAY:
      lcd.setCursor(0, 0);
      lcd.print("Mode 1: DELAY VAN");
      lcd.setCursor(0, 1);
      lcd.print("> DELAY: ");
      lcd.print(workingDelaySec);
      lcd.print(" giay");
      break;

    case MODE_POWERCHECK:
      lcd.setCursor(0, 0);
      lcd.print("Mode 2: POWER CHECK");
      lcd.setCursor(0, 1);
      lcd.print("> POWER CHECK: ");
      lcd.print(workingPowerCheckEnabled ? "ON" : "OFF");
      break;
  }

  // Navigation help
  lcd.setCursor(0, 3);
  lcd.print("#:Luu *:Thoat <>:Mode");

  currentAppState = STATE_CONFIG;
}

void drawManualMenu() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("THU CONG");
  lcd.setCursor(0, 1);

  // Display [HH:MM] with blinking effect on the field being edited
  lcd.print("[");

  // Hours
  if (editingManualHour && blinkState) {
    lcd.print("  ");
  } else {
    if (manualHour < 10) lcd.print("0");
    lcd.print(manualHour);
  }

  lcd.print(":");

  // Minutes
  if (!editingManualHour && blinkState) {
    lcd.print("  ");
  } else {
    if (manualMin < 10) lcd.print("0");
    lcd.print(manualMin);
  }

  lcd.print("]");

  // Show which field is being edited
  lcd.setCursor(0, 2);
  lcd.print("Dang chinh: ");
  lcd.print(editingManualHour ? "GIO" : "PHUT");

  // Instructions
  lcd.setCursor(0, 3);
  lcd.print("#:Luu  *:Thoat  OK:Doi");

  currentAppState = STATE_MANUAL;
}

void drawAutoMenu() {
  lcd.clear();
  lcd.setCursor(0, 0);

  // Show status in title
  lcd.print("TU DONG ");
  lcd.print(savedAutoEnabled ? "(ON-" : "(OFF-");
  lcd.print(savedAutoStartIndex);
  lcd.print(")");

  if (inSelectValve) {
    // Valve selection interface
    lcd.setCursor(0, 1);
    lcd.print("> Chon Van Bat Dau:");
    lcd.setCursor(0, 2);
    lcd.print("  VAN: [");
    lcd.print(workingAutoStartIndex);
    lcd.print("]");

    // Instructions
    lcd.setCursor(0, 3);
    lcd.print("#:Luu  *:Huy");
    return;
  }

  if (inTimeSetup) {
    // Time setup interface - FROM-TO on first row
    lcd.setCursor(0, 1);
    lcd.print("F[");

    // FROM hour
    if (autoSelectField == 0 && blinkState) {
      lcd.print("  ");
    } else {
      if (workingAutoFrom.hour < 10) lcd.print("0");
      lcd.print(workingAutoFrom.hour);
    }

    lcd.print(":");

    // FROM minute
    if (autoSelectField == 1 && blinkState) {
      lcd.print("  ");
    } else {
      if (workingAutoFrom.minute < 10) lcd.print("0");
      lcd.print(workingAutoFrom.minute);
    }

    lcd.print("]-T[");

    // TO hour
    if (autoSelectField == 2 && blinkState) {
      lcd.print("  ");
    } else {
      if (workingAutoTo.hour < 10) lcd.print("0");
      lcd.print(workingAutoTo.hour);
    }

    lcd.print(":");

    // TO minute
    if (autoSelectField == 3 && blinkState) {
      lcd.print("  ");
    } else {
      if (workingAutoTo.minute < 10) lcd.print("0");
      lcd.print(workingAutoTo.minute);
    }

    lcd.print("]");

    // Duration-Rest on second row
    lcd.setCursor(0, 2);
    lcd.print("D[");

    // DURATION hour
    if (autoSelectField == 4 && blinkState) {
      lcd.print("  ");
    } else {
      if (workingDuration.hour < 10) lcd.print("0");
      lcd.print(workingDuration.hour);
    }

    lcd.print(":");

    // DURATION minute
    if (autoSelectField == 5 && blinkState) {
      lcd.print("  ");
    } else {
      if (workingDuration.minute < 10) lcd.print("0");
      lcd.print(workingDuration.minute);
    }

    lcd.print("]-R[");

    // REST hour
    if (autoSelectField == 6 && blinkState) {
      lcd.print("  ");
    } else {
      if (workingRest.hour < 10) lcd.print("0");
      lcd.print(workingRest.hour);
    }

    lcd.print(":");

    // REST minute
    if (autoSelectField == 7 && blinkState) {
      lcd.print("  ");
    } else {
      if (workingRest.minute < 10) lcd.print("0");
      lcd.print(workingRest.minute);
    }

    lcd.print("]");

    // Instructions
    lcd.setCursor(0, 3);
    lcd.print("#:Luu *:Huy OK:Next");
    return;
  }

  // Main auto menu options
  const char *options[5] = {
    "1. BAT TU DONG",
    "2. TAT TU DONG",
    "3. RESET",
    "4. CHON VAN BAT DAU",
    "5. CAI DAT THOI GIAN"
  };

  // Calculate which page of options to show
  uint8_t itemsPerPage = 3;
  uint8_t totalPages = (5 + itemsPerPage - 1) / itemsPerPage;
  uint8_t startItem = autoMenuPage * itemsPerPage;

  // Fix the min() function type mismatch by casting
  uint8_t itemsToDisplay = 5 - startItem;
  uint8_t itemsOnThisPage = (itemsPerPage < itemsToDisplay) ? itemsPerPage : itemsToDisplay;

  // Display the current page of menu items
  for (uint8_t i = 0; i < itemsOnThisPage; i++) {
    uint8_t itemIndex = startItem + i;
    lcd.setCursor(0, i + 1);
    lcd.print(itemIndex == autoMenuIndex ? ">" : " ");
    lcd.print(options[itemIndex]);
  }

  // Show navigation arrows if needed
  if (autoMenuPage > 0) {
    lcd.setCursor(18, 1);
    lcd.print("^");
  }
  if (autoMenuPage < totalPages - 1) {
    lcd.setCursor(18, 3);
    lcd.print("v");
  }

  currentAppState = STATE_AUTO;
}

// ======================
// === IR INPUT HANDLING ===
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
    mainMenuIndex = (mainMenuIndex == 0) ? 2 : mainMenuIndex - 1;
    drawMainMenu();
  }
  else if (code == IR_CODE_DOWN) {
    mainMenuIndex = (mainMenuIndex + 1) % 3;
    drawMainMenu();
  }
  else if (code == IR_CODE_OK) {
    switch (mainMenuIndex) {
      case 0:  // Config
        currentConfigMode = MODE_ONOFF;
        configMenuPage = 0;
        configMenuIndex = 0;
        drawConfigMenu();
        break;

      case 1:  // Manual
        editingManualHour = true;
        manualHour = 0;
        manualMin = 0;
        drawManualMenu();
        break;

      case 2:  // Auto
        autoMenuIndex = 0;
        autoMenuPage = 0;
        inSelectValve = false;
        inTimeSetup = false;
        drawAutoMenu();
        break;
    }
  }
}

void handleConfigIR(uint32_t code) {
  if (currentConfigMode == MODE_ONOFF) {
    uint8_t valvesPerPage = 3;
    uint8_t totalValves = 10;
    uint8_t totalPages = (totalValves + valvesPerPage - 1) / valvesPerPage;
    uint8_t currentValveIndex = configMenuPage * valvesPerPage + configMenuIndex;

    if (code == IR_CODE_UP) {
      if (configMenuIndex > 0) {
        configMenuIndex--;
      } else if (configMenuPage > 0) {
        configMenuPage--;
        configMenuIndex = min(valvesPerPage - 1, totalValves - configMenuPage * valvesPerPage - 1);
      }
      drawConfigMenu();
    }
    else if (code == IR_CODE_DOWN) {
      if (configMenuIndex < min(valvesPerPage - 1, totalValves - configMenuPage * valvesPerPage - 1)) {
        configMenuIndex++;
      } else if (configMenuPage < totalPages - 1) {
        configMenuPage++;
        configMenuIndex = 0;
      }
      drawConfigMenu();
    }
    else if (code == IR_CODE_LEFT) {
      currentConfigMode = MODE_POWERCHECK;
      drawConfigMenu();
    }
    else if (code == IR_CODE_RIGHT) {
      currentConfigMode = MODE_DELAY;
      drawConfigMenu();
    }
    else if (code == IR_CODE_OK) {
      if (currentValveIndex < totalValves) {
        workingValveState[currentValveIndex] = !workingValveState[currentValveIndex];
        drawConfigMenu();
      }
    }
  }
  else if (currentConfigMode == MODE_DELAY) {
    if (code == IR_CODE_UP) {
      if (workingDelaySec < 10) {
        workingDelaySec++;
      } else if (workingDelaySec < 60) {
        workingDelaySec += 5;
      } else if (workingDelaySec < 300) {
        workingDelaySec += 30;
      } else {
        workingDelaySec += 60;
      }
      if (workingDelaySec > 3600) workingDelaySec = 3600;  // Max 1 hour
      drawConfigMenu();
    }
    else if (code == IR_CODE_DOWN) {
      if (workingDelaySec > 300) {
        workingDelaySec -= 60;
      } else if (workingDelaySec > 60) {
        workingDelaySec -= 30;
      } else if (workingDelaySec > 10) {
        workingDelaySec -= 5;
      } else if (workingDelaySec > 1) {
        workingDelaySec--;
      }
      drawConfigMenu();
    }
    else if (code == IR_CODE_LEFT) {
      currentConfigMode = MODE_ONOFF;
      configMenuPage = 0;
      configMenuIndex = 0;
      drawConfigMenu();
    }
    else if (code == IR_CODE_RIGHT) {
      currentConfigMode = MODE_POWERCHECK;
      drawConfigMenu();
    }
  }
  else if (currentConfigMode == MODE_POWERCHECK) {
    if (code == IR_CODE_OK) {
      workingPowerCheckEnabled = !workingPowerCheckEnabled;
      drawConfigMenu();
    }
    else if (code == IR_CODE_LEFT) {
      currentConfigMode = MODE_DELAY;
      drawConfigMenu();
    }
    else if (code == IR_CODE_RIGHT) {
      currentConfigMode = MODE_ONOFF;
      configMenuPage = 0;
      configMenuIndex = 0;
      drawConfigMenu();
    }
  }

  // Common actions for all config modes
  if (code == IR_CODE_HASH) {
    // Save configuration
    for (uint8_t i = 0; i < 10; i++) {
      savedValveState[i] = workingValveState[i];
    }
    savedDelaySec = workingDelaySec;
    savedPowerCheckEnabled = workingPowerCheckEnabled;
    saveConfigToPrefs();
    updateOutputsFromWorking();

    // Return to main menu
    currentAppState = STATE_MAIN;
    drawMainMenu();
  }
  else if (code == IR_CODE_STAR) {
    // Cancel changes
    for (uint8_t i = 0; i < 10; i++) {
      workingValveState[i] = savedValveState[i];
    }
    workingDelaySec = savedDelaySec;
    workingPowerCheckEnabled = savedPowerCheckEnabled;
    updateOutputsFromWorking();

    // Return to main menu
    currentAppState = STATE_MAIN;
    drawMainMenu();
  }
}

void handleManualIR(uint32_t code) {
  // Handle direct numeric input
  if (code >= IR_CODE_0 && code <= IR_CODE_9) {
    uint8_t num = 0;
    if (code == IR_CODE_0) num = 0;
    else if (code == IR_CODE_1) num = 1;
    else if (code == IR_CODE_2) num = 2;
    else if (code == IR_CODE_3) num = 3;
    else if (code == IR_CODE_4) num = 4;
    else if (code == IR_CODE_5) num = 5;
    else if (code == IR_CODE_6) num = 6;
    else if (code == IR_CODE_7) num = 7;
    else if (code == IR_CODE_8) num = 8;
    else if (code == IR_CODE_9) num = 9;

    if (irNumericDigits == 0) {
      irNumericBuffer = num;
      irNumericDigits = 1;
    } else {
      irNumericBuffer = irNumericBuffer * 10 + num;
      irNumericDigits = 2;
    }

    if (editingManualHour) {
      if (irNumericBuffer > 23) {
        irNumericBuffer = (irNumericDigits == 1) ? num : 23;
      }
      manualHour = irNumericBuffer;
    } else {
      if (irNumericBuffer > 59) {
        irNumericBuffer = (irNumericDigits == 1) ? num : 59;
      }
      manualMin = irNumericBuffer;
    }

    if (irNumericDigits == 2) {
      resetIRNumericInput();
    }

    drawManualMenu();
    return;
  }

  // Reset numeric input on other keys
  resetIRNumericInput();

  // Handle other navigation and control keys
  if (code == IR_CODE_UP) {
    if (editingManualHour) {
      manualHour = (manualHour + 1) % 24;
    } else {
      manualMin = (manualMin < 45) ? manualMin + 15 : 0;
    }
    drawManualMenu();
  }
  else if (code == IR_CODE_DOWN) {
    if (editingManualHour) {
      manualHour = (manualHour > 0) ? manualHour - 1 : 23;
    } else {
      manualMin = (manualMin >= 15) ? manualMin - 15 : 45;
    }
    drawManualMenu();
  }
  else if (code == IR_CODE_OK) {
    editingManualHour = !editingManualHour;
    drawManualMenu();
  }
  else if (code == IR_CODE_HASH) {
    // Schedule manual irrigation
    if (anySavedValveOpen()) {
      manualScheduled = true;
      isManualRunning = false;

      // Display confirmation
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("DA HEN GIO TU DONG");
      lcd.setCursor(0, 1);
      lcd.print("VAO LUC: ");
      if (manualHour < 10) lcd.print("0");
      lcd.print(manualHour);
      lcd.print(":");
      if (manualMin < 10) lcd.print("0");
      lcd.print(manualMin);

      // Show which valves will be activated
      lcd.setCursor(0, 2);
      lcd.print("Van tuoi: ");
      String valveList = "";
      for (uint8_t i = 0; i < 10; i++) {
        if (savedValveState[i]) {
          valveList += String(i+1) + " ";
        }
      }
      lcd.setCursor(0, 3);
      lcd.print(valveList);

      delay(3000);
      currentAppState = STATE_MAIN;
      drawMainMenu();
    } else {
      // Error: No valves configured to ON
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("LOI: KHONG CO VAN");
      lcd.setCursor(0, 1);
      lcd.print("NAO DUOC BAT");
      lcd.setCursor(0, 3);
      lcd.print("Vao CAU HINH...");
      delay(3000);

      currentAppState = STATE_CONFIG;
      currentConfigMode = MODE_ONOFF;
      configMenuPage = 0;
      configMenuIndex = 0;
      drawConfigMenu();
    }
  }
  else if (code == IR_CODE_STAR) {
    // Cancel
    currentAppState = STATE_MAIN;
    drawMainMenu();
  }
}

void handleAutoIR(uint32_t code) {
  // Handle valve selection interface
  if (inSelectValve) {
    if (code == IR_CODE_UP) {
      if (workingAutoStartIndex < 10) {
        workingAutoStartIndex++;
      } else {
        workingAutoStartIndex = 1;
      }
      drawAutoMenu();
    }
    else if (code == IR_CODE_DOWN) {
      if (workingAutoStartIndex > 1) {
        workingAutoStartIndex--;
      } else {
        workingAutoStartIndex = 10;
      }
      drawAutoMenu();
    }
    else if (code == IR_CODE_HASH) {
      savedAutoStartIndex = workingAutoStartIndex;
      saveConfigToPrefs();
      inSelectValve = false;
      drawAutoMenu();
    }
    else if (code == IR_CODE_STAR) {
      workingAutoStartIndex = savedAutoStartIndex;
      inSelectValve = false;
      drawAutoMenu();
    }
    return;
  }

  // Handle time setup interface
  if (inTimeSetup) {
    if (code == IR_CODE_OK) {
      autoSelectField = (autoSelectField + 1) % 8;
      drawAutoMenu();
      return;
    }
    else if (code == IR_CODE_UP || code == IR_CODE_DOWN) {
      int8_t change = (code == IR_CODE_UP) ? 1 : -1;

      switch (autoSelectField) {
        case 0:  // FROM hour
          workingAutoFrom.hour = (workingAutoFrom.hour + change + 24) % 24;
          break;
        case 1:  // FROM minute
          workingAutoFrom.minute = (workingAutoFrom.minute + change + 60) % 60;
          break;
        case 2:  // TO hour
          workingAutoTo.hour = (workingAutoTo.hour + change + 24) % 24;
          break;
        case 3:  // TO minute
          workingAutoTo.minute = (workingAutoTo.minute + change + 60) % 60;
          break;
        case 4:  // DURATION hour
          workingDuration.hour = (workingDuration.hour + change + 24) % 24;
          break;
        case 5:  // DURATION minute
          workingDuration.minute = (workingDuration.minute + change + 60) % 60;
          break;
        case 6:  // REST hour
          workingRest.hour = (workingRest.hour + change + 24) % 24;
          break;
        case 7:  // REST minute
          workingRest.minute = (workingRest.minute + change + 60) % 60;
          break;
      }

      drawAutoMenu();
      return;
    }
    else if (code == IR_CODE_HASH) {
      // Save time settings
      savedAutoFrom = workingAutoFrom;
      savedAutoTo = workingAutoTo;
      savedDuration = workingDuration;
      savedRest = workingRest;
      saveConfigToPrefs();

      inTimeSetup = false;
      drawAutoMenu();
      return;
    }
    else if (code == IR_CODE_STAR) {
      // Cancel changes
      workingAutoFrom = savedAutoFrom;
      workingAutoTo = savedAutoTo;
      workingDuration = savedDuration;
      workingRest = savedRest;

      inTimeSetup = false;
      drawAutoMenu();
      return;
    }
  }

  // Handle main auto menu
  uint8_t itemsPerPage = 3;
  uint8_t totalItems = 5;
  uint8_t totalPages = (totalItems + itemsPerPage - 1) / itemsPerPage;
  uint8_t currentItemIndex = autoMenuPage * itemsPerPage + autoMenuIndex;

  if (code == IR_CODE_UP) {
    if (autoMenuIndex > 0) {
      autoMenuIndex--;
    } else if (autoMenuPage > 0) {
      autoMenuPage--;
      autoMenuIndex = min(itemsPerPage - 1, totalItems - autoMenuPage * itemsPerPage - 1);
    }
    drawAutoMenu();
  }
  else if (code == IR_CODE_DOWN) {
    if (autoMenuIndex < min(itemsPerPage - 1, totalItems - autoMenuPage * itemsPerPage - 1)) {
      autoMenuIndex++;
    } else if (autoMenuPage < totalPages - 1) {
      autoMenuPage++;
      autoMenuIndex = 0;
    }
    drawAutoMenu();
  }
  else if (code == IR_CODE_OK) {
    switch (currentItemIndex) {
      case 0:  // Turn Auto ON
        workingAutoEnabled = true;
        savedAutoEnabled = true;
        saveConfigToPrefs();

        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("TU DONG: BAT");
        lcd.setCursor(0, 1);
        lcd.print("Van dau: ");
        lcd.print(savedAutoStartIndex);
        delay(2000);

        currentAppState = STATE_MAIN;
        drawMainMenu();
        break;

      case 1:  // Turn Auto OFF
        workingAutoEnabled = false;
        savedAutoEnabled = false;
        saveConfigToPrefs();

        // Stop any running auto operations
        if (autoRunning) {
          autoRunning = false;
          turnAllOff();
        }

        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("TU DONG: TAT");
        delay(2000);

        currentAppState = STATE_MAIN;
        drawMainMenu();
        break;

      case 2:  // Reset
        workingAutoEnabled = false;
        savedAutoEnabled = false;
        workingAutoStartIndex = 1;
        savedAutoStartIndex = 1;
        saveConfigToPrefs();

        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("TU DONG: RESET");
        delay(2000);

        currentAppState = STATE_MAIN;
        drawMainMenu();
        break;

      case 3:  // Select Start Valve
        inSelectValve = true;
        drawAutoMenu();
        break;

      case 4:  // Time Setup
        inTimeSetup = true;
        autoSelectField = 0;  // Start with the first field (FROM hour)
        drawAutoMenu();
        break;
    }
  }
  else if (code == IR_CODE_STAR) {
    currentAppState = STATE_MAIN;
    drawMainMenu();
  }
}
