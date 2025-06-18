#include "auto_menu.h"
#include "../types.h"
#include "../hardware/display.h"
#include "../storage/preferences.h"
#include "../hardware/ir_remote.h"
#include "../irrigation/auto_irrigation.h"

void drawAutoMenu() {
  lcd.clear();

  if (inSelectValve) {
    // Valve selection screen
    lcd.setCursor(0, 0);
    lcd.print("CHON VAN BAT DAU: ");
    lcd.print(workingAutoStartIndex);

    for (int i = 0; i < 10; i++) {
      int row = (i < 5) ? 1 : 2;
      int col = (i < 5) ? i * 4 : (i - 5) * 4;
      lcd.setCursor(col, row);
      lcd.print(i + 1);
      if (i + 1 == workingAutoStartIndex) {
        lcd.print("*");
      } else {
        lcd.print(" ");
      }
    }

    lcd.setCursor(0, 3);
    lcd.print("* QUAY LAI   # XAC NHAN");
  }
  else if (inTimeSetup) {
    // Time setup screen
    lcd.setCursor(0, 0);

    // Field labels
    const char* labels[] = {"TU:", "DEN:", "TGIAN:", "NGHI:"};

    // Current field values
    TimeHM values[] = {
      workingAutoFrom,
      workingAutoTo,
      workingDuration,
      workingRest
    };

    // Display label and time for current field
    lcd.print(labels[autoSelectField / 2]);

    TimeHM time = values[autoSelectField / 2];
    if (autoSelectField % 2 == 0) {
      // Hour editing
      if (blinkState) {
        lcd.print("  :");
      } else {
        lcd.print(time.hour < 10 ? "0" : "");
        lcd.print(time.hour);
        lcd.print(":");
      }

      lcd.print(time.minute < 10 ? "0" : "");
      lcd.print(time.minute);
    } else {
      // Minute editing
      lcd.print(time.hour < 10 ? "0" : "");
      lcd.print(time.hour);
      lcd.print(":");

      if (blinkState) {
        lcd.print("  ");
      } else {
        lcd.print(time.minute < 10 ? "0" : "");
        lcd.print(time.minute);
      }
    }

    // Show navigation hints
    lcd.setCursor(0, 3);
    lcd.print("* QUAY LAI  < > # LUU");
  }
  else {
    // Main auto menu
    lcd.setCursor(0, 0);
    lcd.print("CHE DO TU DONG");

    // Items on first page
    const char* items[] = {
      "CHO PHEP:",
      "VAN BAT DAU:",
      "THOI GIAN:",
      "TRANG THAI:"
    };

    // Display menu items with selection
    for (uint8_t i = 0; i < 3; i++) {
      lcd.setCursor(0, i + 1);
      lcd.print(i == autoMenuIndex ? ">" : " ");
      lcd.print(items[i]);

      // Show values for each item
      switch (i) {
        case 0:  // Enable/Disable
          lcd.setCursor(11, 1);
          lcd.print(workingAutoEnabled ? "BAT" : "TAT");
          break;

        case 1:  // Starting valve
          lcd.setCursor(14, 2);
          lcd.print(workingAutoStartIndex);
          break;

        case 2:  // Time settings
          lcd.setCursor(12, 3);
          if (isAutoRunning()) {
            lcd.print("DANG CHAY");
          } else if (workingAutoEnabled) {
            lcd.print("SAN SANG");
          } else {
            lcd.print("TAT");
          }
          break;
      }
    }
  }
}

void handleAutoIR(uint32_t code) {
  if (inSelectValve) {
    // Handle valve selection
    if (code == IR_CODE_STAR) {
      inSelectValve = false;
      drawAutoMenu();
    } else if (code == IR_CODE_HASH) {
      inSelectValve = false;
      drawAutoMenu();
    } else if (code >= IR_CODE_1 && code <= IR_CODE_9) {
      workingAutoStartIndex = code - IR_CODE_1 + 1;
      drawAutoMenu();
    } else if (code == IR_CODE_0) {
      workingAutoStartIndex = 10;
      drawAutoMenu();
    }
  }
  else if (inTimeSetup) {
    // Handle time setup
    if (code == IR_CODE_STAR) {
      inTimeSetup = false;
      drawAutoMenu();
    } else if (code == IR_CODE_HASH) {
      inTimeSetup = false;
      saveConfigToPrefs();
      displayMessage("DA LUU CAI DAT", "THOI GIAN", "", "", 1000);
      drawAutoMenu();
    } else if (code == IR_CODE_LEFT) {
      if (autoSelectField > 0) {
        autoSelectField--;
      } else {
        autoSelectField = 7;  // Wrap around
      }
      drawAutoMenu();
    } else if (code == IR_CODE_RIGHT) {
      if (autoSelectField < 7) {
        autoSelectField++;
      } else {
        autoSelectField = 0;  // Wrap around
      }
      drawAutoMenu();
    } else if (code >= IR_CODE_0 && code <= IR_CODE_9) {
      uint8_t digit = (code == IR_CODE_0) ? 0 : (code - IR_CODE_1 + 1);
      TimeHM* time = nullptr;

      // Determine which time struct we're editing
      switch (autoSelectField / 2) {
        case 0: time = &workingAutoFrom; break;
        case 1: time = &workingAutoTo; break;
        case 2: time = &workingDuration; break;
        case 3: time = &workingRest; break;
      }

      if (time != nullptr) {
        if (autoSelectField % 2 == 0) {
          // Hour editing
          if (getIrNumericDigits() == 0) {
            time->hour = digit;
          } else {
            uint8_t newHour = getIrNumericBuffer() * 10 + digit;
            if (newHour <= 23) {
              time->hour = newHour;
            }
            resetIRNumericInput();
          }
        } else {
          // Minute editing
          if (getIrNumericDigits() == 0) {
            time->minute = digit;
          } else {
            uint8_t newMinute = getIrNumericBuffer() * 10 + digit;
            if (newMinute <= 59) {
              time->minute = newMinute;
            }
            resetIRNumericInput();
          }
        }
      }

      drawAutoMenu();
    }
  }
  else {
    // Main auto menu handling
    if (code == IR_CODE_STAR) {
      currentAppState = STATE_MAIN;
      drawMainMenu();
    } else if (code == IR_CODE_UP) {
      if (autoMenuIndex > 0) {
        autoMenuIndex--;
      } else {
        autoMenuIndex = 2;  // Wrap around
      }
      drawAutoMenu();
    } else if (code == IR_CODE_DOWN) {
      if (autoMenuIndex < 2) {
        autoMenuIndex++;
      } else {
        autoMenuIndex = 0;  // Wrap around
      }
      drawAutoMenu();
    } else if (code == IR_CODE_OK) {
      switch (autoMenuIndex) {
        case 0:  // Toggle enabled
          workingAutoEnabled = !workingAutoEnabled;
          saveConfigToPrefs();
          drawAutoMenu();
          break;

        case 1:  // Valve selection
          inSelectValve = true;
          drawAutoMenu();
          break;

        case 2:  // Time setup
          inTimeSetup = true;
          autoSelectField = 0;
          drawAutoMenu();
          break;
      }
    }
  }
}