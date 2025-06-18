#include "config_menu.h"
#include "../types.h"
#include "../hardware/display.h"
#include "../storage/preferences.h"
#include "../hardware/hardware.h"
#include "../hardware/ir_remote.h"

void drawConfigMenu() {
  lcd.clear();

  switch (currentConfigMode) {
    case MODE_ONOFF:
      // First page: 5 valves
      // Second page: 5 valves
      // Third page: 2 pumps
      lcd.setCursor(0, 0);
      if (configMenuPage == 0) {
        lcd.print("CAU HINH: VAN 1-5");
        // Display valve states 0-4
        for (uint8_t i = 0; i < 5; i++) {
          lcd.setCursor(0, i + 1);
          if (i == configMenuIndex) {
            lcd.print(">");
          } else {
            lcd.print(" ");
          }
          lcd.print("VAN ");
          lcd.print(i + 1);
          lcd.print(": ");
          lcd.print(getWorkingValveState(i) ? "MO" : "DONG");
        }
      } else if (configMenuPage == 1) {
        lcd.print("CAU HINH: VAN 6-10");
        // Display valve states 5-9
        for (uint8_t i = 0; i < 5; i++) {
          lcd.setCursor(0, i + 1);
          if (i == configMenuIndex) {
            lcd.print(">");
          } else {
            lcd.print(" ");
          }
          lcd.print("VAN ");
          lcd.print(i + 6);
          lcd.print(": ");
          lcd.print(getWorkingValveState(i + 5) ? "MO" : "DONG");
        }
      } else if (configMenuPage == 2) {
        lcd.print("CAU HINH: BOM");
        // Display pump states
        for (uint8_t i = 0; i < 2; i++) {
          lcd.setCursor(0, i + 1);
          if (i == configMenuIndex) {
            lcd.print(">");
          } else {
            lcd.print(" ");
          }
          lcd.print("BOM ");
          lcd.print(i + 1);
          lcd.print(": ");
          lcd.print(getWorkingPumpState(i) ? "MO" : "DONG");
        }

        lcd.setCursor(0, 3);
        lcd.print("* QUAY LAI # LUU");
      }
      break;

    case MODE_DELAY:
      lcd.print("CAU HINH: THOI GIAN");
      lcd.setCursor(0, 1);
      lcd.print("THOI GIAN TUOI: ");
      lcd.setCursor(0, 2);
      lcd.print(getWorkingDelaySec());
      lcd.print(" GIAY");
      lcd.setCursor(0, 3);
      lcd.print("* QUAY LAI # LUU");
      break;

    case MODE_POWERCHECK:
      lcd.print("CAU HINH: KIEM TRA");
      lcd.setCursor(0, 1);
      lcd.print("KIEM TRA DIEN: ");
      lcd.print(isPowerCheckEnabled() ? "BAT" : "TAT");
      lcd.setCursor(0, 3);
      lcd.print("* QUAY LAI # LUU");
      break;
  }
}

void handleConfigIR(uint32_t code) {
  switch (currentConfigMode) {
    case MODE_ONOFF:
      if (code == IR_CODE_STAR) {  // Back
        currentAppState = STATE_MAIN;
        drawMainMenu();
      } else if (code == IR_CODE_HASH) {  // Save
        saveConfigToPrefs();
        displayMessage("DA LUU CAU HINH", "", "", "", 1000);
        currentAppState = STATE_MAIN;
        drawMainMenu();
      } else if (code == IR_CODE_UP) {
        if (configMenuIndex > 0) {
          configMenuIndex--;
        } else {
          if (configMenuPage > 0) {
            configMenuPage--;
            configMenuIndex = (configMenuPage == 2) ? 1 : 4; // Adjust max index based on page
          }
        }
        drawConfigMenu();
      } else if (code == IR_CODE_DOWN) {
        int maxIndex = (configMenuPage == 2) ? 1 : 4;
        if (configMenuIndex < maxIndex) {
          configMenuIndex++;
        } else {
          if ((configMenuPage == 0) || (configMenuPage == 1)) {
            configMenuPage++;
            configMenuIndex = 0;
          }
        }
        drawConfigMenu();
      } else if (code == IR_CODE_OK) {
        // Toggle selected valve or pump
        if (configMenuPage == 0) {
          setWorkingValveState(configMenuIndex, !getWorkingValveState(configMenuIndex));
        } else if (configMenuPage == 1) {
          setWorkingValveState(configMenuIndex + 5, !getWorkingValveState(configMenuIndex + 5));
        } else if (configMenuPage == 2) {
          setWorkingPumpState(configMenuIndex, !getWorkingPumpState(configMenuIndex));
        }
        updateOutputsFromWorking();
        drawConfigMenu();
      }
      break;

    case MODE_DELAY:
      if (code == IR_CODE_STAR) {  // Back
        currentConfigMode = MODE_ONOFF;
        configMenuPage = 0;
        configMenuIndex = 0;
        drawConfigMenu();
      } else if (code == IR_CODE_HASH) {  // Save
        saveConfigToPrefs();
        displayMessage("DA LUU CAU HINH", "", "", "", 1000);
        currentConfigMode = MODE_ONOFF;
        configMenuPage = 0;
        configMenuIndex = 0;
        drawConfigMenu();
      } else if (code >= IR_CODE_0 && code <= IR_CODE_9) {
        uint8_t digit = (code == IR_CODE_0) ? 0 : (code - IR_CODE_1 + 1);

        if (getIrNumericDigits() == 0) {
          setWorkingDelaySec(digit);
        } else {
          setWorkingDelaySec(getWorkingDelaySec() * 10 + digit);
        }

        // Limit to reasonable values (3600 seconds = 1 hour)
        if (getWorkingDelaySec() > 3600) {
          setWorkingDelaySec(3600);
        }

        drawConfigMenu();
      } else if (code == IR_CODE_RIGHT) {
        // Next mode
        currentConfigMode = MODE_POWERCHECK;
        drawConfigMenu();
        resetIRNumericInput();
      }
      break;

    case MODE_POWERCHECK:
      if (code == IR_CODE_STAR) {  // Back
        currentConfigMode = MODE_ONOFF;
        configMenuPage = 0;
        configMenuIndex = 0;
        drawConfigMenu();
      } else if (code == IR_CODE_HASH) {  // Save
        saveConfigToPrefs();
        displayMessage("DA LUU CAU HINH", "", "", "", 1000);
        currentConfigMode = MODE_ONOFF;
        configMenuPage = 0;
        configMenuIndex = 0;
        drawConfigMenu();
      } else if (code == IR_CODE_OK) {
        // Toggle power check state
        setPowerCheckEnabled(!isPowerCheckEnabled());
        drawConfigMenu();
      } else if (code == IR_CODE_LEFT) {
        // Previous mode
        currentConfigMode = MODE_DELAY;
        drawConfigMenu();
      }
      break;
  }
}