#include "main_menu.h"
#include "../types.h"
#include "../hardware/display.h"
#include "../config.h"
#include "../storage/preferences.h"
#include "../irrigation/manual_irrigation.h"
#include "../irrigation/auto_irrigation.h"

extern uint8_t mainMenuIndex;
extern AppState currentAppState;

void drawMainMenu() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("   MENU CHINH");

  const char* menuItems[] = {"CAU HINH VAN/BOM", "CHE DO THU CONG", "CHE DO TU DONG"};

  // Display menu items with selection
  for (uint8_t i = 0; i < 3; i++) {
    lcd.setCursor(0, i + 1);
    if (i == mainMenuIndex) {
      lcd.print("> ");
    } else {
      lcd.print("  ");
    }
    lcd.print(menuItems[i]);
  }
}

void handleMainIR(uint32_t code) {
  switch (code) {
    case IR_CODE_UP:
      if (mainMenuIndex > 0) {
        mainMenuIndex--;
      } else {
        mainMenuIndex = 2;  // Wrap around to bottom
      }
      drawMainMenu();
      break;

    case IR_CODE_DOWN:
      if (mainMenuIndex < 2) {
        mainMenuIndex++;
      } else {
        mainMenuIndex = 0;  // Wrap around to top
      }
      drawMainMenu();
      break;

    case IR_CODE_OK:
      switch (mainMenuIndex) {
        case 0:  // Config
          currentAppState = STATE_CONFIG;
          currentConfigMode = MODE_ONOFF;
          configMenuPage = 0;
          configMenuIndex = 0;
          drawConfigMenu();
          break;

        case 1:  // Manual
          currentAppState = STATE_MANUAL;
          editingManualHour = true;
          drawManualMenu();
          break;

        case 2:  // Auto
          currentAppState = STATE_AUTO;
          autoMenuIndex = 0;
          autoMenuPage = 0;
          inSelectValve = false;
          inTimeSetup = false;
          drawAutoMenu();
          break;
      }
      break;
  }
}