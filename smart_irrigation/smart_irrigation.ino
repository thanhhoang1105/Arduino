#define BLYNK_TEMPLATE_ID "TMPL6xy4mK0KN"
#define BLYNK_TEMPLATE_NAME "Smart Irrigation Controller"
#define BLYNK_AUTH_TOKEN "J34tRXWg_QTXt8bs0SfbZFSc1ARUpL0o"
#define BLYNK_PRINT Serial

#include "BlynkCredentials.h"
#include <Arduino.h>
#include "Hardware.h"
#include "Config.h"
#include "TimeBlynk.h"
#include "Display.h"
#include "IRHandler.h"
#include "Irrigation.h"
#include "Utils.h"

void setup()
{
    Serial.begin(115200);
    delay(100);
    Hardware::begin();
    Config::load();
    TimeBlynk::begin();
    Display::drawMainMenu(0, Config::getAutoEnabled(), Config::getAutoStartIndex());
    IRHandler::begin(15);
}

void loop()
{
    TimeBlynk::runBlynk(); // Thay vì gọi Blynk.run() trực tiếp
    IRHandler::process();
    Irrigation::update();
    Display::updateBlink();
    Utils::periodicTasks();
}