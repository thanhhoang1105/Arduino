#include "display.h"
#include "../config.h"
#include <Wire.h>
#include <LiquidCrystal_PCF8574.h>

LiquidCrystal_PCF8574 lcd(LCD_I2C_ADDR);

void initializeDisplay() {
  Wire.begin(LCD_SDA_PIN, LCD_SCL_PIN);
  lcd.begin(20, 4); // 20 columns, 4 rows
  lcd.setBacklight(255); // Max backlight
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("HE THONG TUOI VUON");
  lcd.setCursor(0, 1);
  lcd.print("DANG KHOI DONG...");
}

void displayMessage(const char* line1, const char* line2, const char* line3, const char* line4, int delayMs) {
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print(line1);

  if (strlen(line2) > 0) {
    lcd.setCursor(0, 1);
    lcd.print(line2);
  }

  if (strlen(line3) > 0) {
    lcd.setCursor(0, 2);
    lcd.print(line3);
  }

  if (strlen(line4) > 0) {
    lcd.setCursor(0, 3);
    lcd.print(line4);
  }

  if (delayMs > 0) {
    delay(delayMs);
  }
}