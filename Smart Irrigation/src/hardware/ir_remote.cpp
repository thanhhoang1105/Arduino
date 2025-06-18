#include "ir_remote.h"
#include <IRremote.h>
#include "../config.h"
#include "../types.h"
#include "../ui/menu_system.h"

uint8_t irNumericBuffer = 0;
uint8_t irNumericDigits = 0;

void initializeIR() {
  IrReceiver.begin(IR_RECEIVER_PIN, ENABLE_LED_FEEDBACK);
  Serial.println("[IR] IR receiver initialized.");
}

void handleIRInput() {
  if (IrReceiver.decode()) {
    uint32_t code = IrReceiver.decodedIRData.decodedRawData;
    if (code != 0) {
      Serial.print("[IR] Received code: 0x");
      Serial.println(code, HEX);
      handleIR(code);
    }
    IrReceiver.resume();
  }
}

uint8_t getIrNumericBuffer() {
  return irNumericBuffer;
}

uint8_t getIrNumericDigits() {
  return irNumericDigits;
}

void resetIRNumericInput() {
  irNumericBuffer = 0;
  irNumericDigits = 0;
}

// Các hàm xử lý IR khác sẽ được chuyển sang menu_system.cpp