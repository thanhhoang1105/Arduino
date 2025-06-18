#pragma once

// Blynk configuration
#define BLYNK_TEMPLATE_ID "TMPL6xy4mK0KN"
#define BLYNK_TEMPLATE_NAME "Smart Irrigation Controller"
#define BLYNK_AUTH_TOKEN "J34tRXWg_QTXt8bs0SfbZFSc1ARUpL0o"
#define BLYNK_PRINT Serial

// Hardware pins
// Relay pins
extern const uint8_t relayValvePins[10];
extern const uint8_t relayPumpPins[2];

// IR Receiver
extern const uint8_t IR_RECEIVER_PIN;

// LCD (I2C PCF8574)
extern const uint8_t LCD_I2C_ADDR;
extern const uint8_t LCD_SDA_PIN;
extern const uint8_t LCD_SCL_PIN;

// ACS712 (Analog Current Sensor)
extern const uint8_t ACS712_PIN;

// IR Remote codes
#define IR_CODE_0      0xE619FF00UL
#define IR_CODE_1      0xBA45FF00UL
#define IR_CODE_2      0xB946FF00UL
#define IR_CODE_3      0xB847FF00UL
#define IR_CODE_4      0xBB44FF00UL
#define IR_CODE_5      0xBF40FF00UL
#define IR_CODE_6      0xBC43FF00UL
#define IR_CODE_7      0xF807FF00UL
#define IR_CODE_8      0xEA15FF00UL
#define IR_CODE_9      0xF609FF00UL

#define IR_CODE_STAR   0xE916FF00UL  // CANCEL / BACK
#define IR_CODE_HASH   0xF20DFF00UL  // SAVE / CONFIRM

#define IR_CODE_UP     0xE718FF00UL
#define IR_CODE_DOWN   0xAD52FF00UL
#define IR_CODE_RIGHT  0xA55AFF00UL
#define IR_CODE_LEFT   0xF708FF00UL
#define IR_CODE_OK     0xE31CFF00UL

// Power check constants
extern const float POWER_THRESHOLD_AMPS;
extern const unsigned long POWER_CHECK_INTERVAL;

// NTP settings
extern const char* ntpServer;
extern const long gmtOffset_sec;
extern const int daylightOffset_sec;