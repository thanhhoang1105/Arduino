#include <Arduino.h>
#include "config.h"
#include "types.h"
#include "hardware/hardware.h"
#include "networking/wifi_manager.h"
#include "networking/blynk_control.h"
#include "storage/preferences.h"
#include "ui/menu_system.h"
#include "ui/main_menu.h"
#include "irrigation/manual_irrigation.h"
#include "irrigation/auto_irrigation.h"
#include "hardware/ir_remote.h"

// Pin configuration
const uint8_t relayValvePins[10] = {2, 4, 16, 17, 5, 18, 19, 21, 22, 23};
const uint8_t relayPumpPins[2]   = {26, 25};
const uint8_t IR_RECEIVER_PIN = 15;
const uint8_t LCD_I2C_ADDR = 0x27;
const uint8_t LCD_SDA_PIN = 13;
const uint8_t LCD_SCL_PIN = 14;
const uint8_t ACS712_PIN = 34;

// Other constants
const float POWER_THRESHOLD_AMPS = 0.1;
const unsigned long POWER_CHECK_INTERVAL = 2000;
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 7 * 3600; // GMT+7 (Vietnam)
const int daylightOffset_sec = 0;

// Global state variables initialization
AppState currentAppState = STATE_MAIN;
uint8_t mainMenuIndex = 0;
ConfigMode currentConfigMode = MODE_ONOFF;
uint8_t configMenuPage = 0;
uint8_t configMenuIndex = 0;
uint8_t autoMenuIndex = 0;
uint8_t autoMenuPage = 0;
bool inSelectValve = false;
bool inTimeSetup = false;
uint8_t autoSelectField = 0;
unsigned long lastBlinkMillis = 0;
bool blinkState = false;

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\n[SETUP] He Thong Tuoi Vuon Ca Phe - Khoi Dong...");

  initializeHardware();

  initializePreferences();
  loadConfigFromPrefs();

  initializeWiFi();
  initializeTime();
  initializeIR();

  Serial.println("[SETUP] Khoi dong hoan tat.");
  drawMainMenu();
  updateOutputsFromWorking();
}

void loop() {
  runBlynk();

  // Process IR remote input
  handleIRInput();

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
  checkPower();

  // Check manual irrigation
  processManualIrrigation();

  // Check automatic irrigation
  if (isAutoEnabled()) {
    processAutoIrrigation();
  }

  delay(20); // Small delay to yield CPU
}