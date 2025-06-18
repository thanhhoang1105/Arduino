#include "wifi_manager.h"
#include <WiFi.h>
#include <WiFiManager.h>
#include <time.h>
#include "../config.h"
#include "../types.h"
#include "../hardware/display.h"
#include "blynk_control.h"

void initializeWiFi() {
  Serial.println("[WIFI] Connecting to WiFi...");
  displayMessage("KET NOI WIFI...", "", "", "");

  // Use WiFiManager for easier WiFi setup
  WiFiManager wm;
  wm.setConfigPortalTimeout(180); // Portal stays active for 3 minutes

  if (!wm.autoConnect("IrrigationSystem")) {
    Serial.println("[WIFI] Failed to connect and hit timeout");
    displayMessage("LOI KET NOI WIFI", "Tu dong khoi dong lai", "sau 5 giay...", "", 5000);
    ESP.restart();
  }

  Serial.print("[WIFI] Connected! IP: ");
  Serial.println(WiFi.localIP());

  displayMessage("WIFI OK: ", WiFi.SSID().c_str(), "KET NOI BLYNK...", "");

  // Initialize Blynk
  initializeBlynk();
}

void initializeTime() {
  Serial.println("[TIME] Setting up NTP time...");
  displayMessage("KET NOI WIFI OK", WiFi.SSID().c_str(), "CAI DAT GIO...", "");

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    char timeString[20];
    strftime(timeString, sizeof(timeString), "%H:%M:%S", &timeinfo);
    Serial.print("[TIME] Current time: ");
    Serial.println(timeString);

    displayMessage("KET NOI WIFI OK", WiFi.SSID().c_str(), "GIO HIEN TAI:", timeString, 2000);
  } else {
    Serial.println("[TIME] Failed to obtain time");
    displayMessage("KET NOI WIFI OK", WiFi.SSID().c_str(), "KHONG LAY DUOC GIO", "", 2000);
  }
}