#include "manual_menu.h"
#include "../types.h"
#include "../hardware/display.h"
#include "../hardware/ir_remote.h"
#include "../irrigation/manual_irrigation.h"

void drawManualMenu() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("CHE DO TU DONG");

  lcd.setCursor(0, 1);
  lcd.print("DAT THOI GIAN TUOI");

  lcd.setCursor(0, 2);
  lcd.print("GIO: ");
  if (isEditingManualHour() && blinkState) {
    lcd.print("  ");
  } else {
    if (getManualHour() < 10) lcd.print("0");
    lcd.print(getManualHour());
  }

  lcd.print(" PHUT: ");
  if (!isEditingManualHour() && blinkState) {
    lcd.print("  ");
  } else {
    if (getManualMin() < 10) lcd.print("0");
    lcd.print(getManualMin());
  }

  lcd.setCursor(0, 3);
  if (isManualScheduled()) {
    lcd.print("DA DAT LIC TUOI    *");
  } else {
    lcd.print("* QUAY LAI   # XAC NHAN");
  }
}

void handleManualIR(uint32_t code) {
  if (code == IR_CODE_STAR) {  // BACK
    currentAppState = STATE_MAIN;
    drawMainMenu();
  } else if (code == IR_CODE_OK) {
    // Toggle between hour and minute editing
    setEditingManualHour(!isEditingManualHour());
    drawManualMenu();
  } else if (code == IR_CODE_HASH && !isManualScheduled()) {  // CONFIRM
    if (anySavedValveOpen()) {
      setManualScheduled(true);
      displayMessage("CHE DO THU CONG", "DA CAI DAT", "THOI GIAN BAT DAU:",
                   (String(getManualHour()) + ":" + (getManualMin() < 10 ? "0" : "") + String(getManualMin())).c_str(), 2000);
      drawManualMenu();
    } else {
      // No valves selected for manual run
      displayMessage("LOI!", "CHUA CHON VAN NAO", "HAY VAO CAU HINH", "CHON IT NHAT 1 VAN", 2000);
      drawManualMenu();
    }
  } else if (code >= IR_CODE_0 && code <= IR_CODE_9) {
    uint8_t digit = (code == IR_CODE_0) ? 0 : (code - IR_CODE_1 + 1);

    if (isEditingManualHour()) {
      if (getIrNumericDigits() == 0) {
        setManualHour(digit);
      } else {
        // Two-digit hour but cap at 23
        uint8_t newHour = getIrNumericBuffer() * 10 + digit;
        if (newHour <= 23) {
          setManualHour(newHour);
        }
        resetIRNumericInput();
      }
    } else {
      if (getIrNumericDigits() == 0) {
        setManualMin(digit);
      } else {
        // Two-digit minute but cap at 59
        uint8_t newMin = getIrNumericBuffer() * 10 + digit;
        if (newMin <= 59) {
          setManualMin(newMin);
        }
        resetIRNumericInput();
      }
    }

    drawManualMenu();
  }
}