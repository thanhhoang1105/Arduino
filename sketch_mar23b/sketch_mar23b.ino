#define BLYNK_TEMPLATE_ID "TMPL6zGEUGQjx"
#define BLYNK_TEMPLATE_NAME "Kitchen Light"
#define BLYNK_AUTH_TOKEN "1pMNb1ZDLbq1I6EdtEsz4g_AsOi0OqNe"

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <BlynkSimpleEsp8266.h>
#include <EEPROM.h>


const char* ssid     = "VU ne";
const char* password = "12341234";

// Cấu hình NTP
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 7 * 3600, 60000);  // Múi giờ +7, cập nhật mỗi 60 giây

// Chân relay (ví dụ sử dụng chân D2)
const int relayPin = D2;

// Định nghĩa kích hoạt relay (chỉnh theo module của bạn)
// Nếu module relay của bạn active high:
#define RELAY_ACTIVE_STATE HIGH
#define RELAY_INACTIVE_STATE LOW
// Nếu active low, bạn có thể đổi thành:
// #define RELAY_ACTIVE_STATE LOW
// #define RELAY_INACTIVE_STATE HIGH

// Các biến cấu hình thời gian bật/tắt relay (default: 21:30 đến 4:00)
volatile int startHour   = 21;
volatile int startMinute = 30;
volatile int endHour     = 4;
volatile int endMinute   = 0;

// EEPROM addresses
#define EEPROM_SIZE 64
#define ADDR_START_HOUR   0
#define ADDR_START_MINUTE 1
#define ADDR_END_HOUR     2
#define ADDR_END_MINUTE   3
#define EEPROM_FLAG_ADDR  4
#define EEPROM_FLAG_VALUE 0xAA

// Biến override từ nút V0 (để bật relay thủ công)
volatile bool manualOverride = false;

// Thời gian kiểm tra lịch trình (mỗi 10 giây)
unsigned long lastCheck = 0;
const unsigned long checkInterval = 10000; // 10 giây

// Lưu trạng thái relay đã gửi lên Blynk (1: ON, 0: OFF)
int lastRelayState = -1;

// Hàm lưu các giá trị cấu hình lên EEPROM
void saveScheduleToEEPROM() {
  EEPROM.write(ADDR_START_HOUR, startHour);
  EEPROM.write(ADDR_START_MINUTE, startMinute);
  EEPROM.write(ADDR_END_HOUR, endHour);
  EEPROM.write(ADDR_END_MINUTE, endMinute);
  EEPROM.write(EEPROM_FLAG_ADDR, EEPROM_FLAG_VALUE); // Đánh dấu đã lưu giá trị hợp lệ
  EEPROM.commit();
  Serial.println("Lưu lịch trình vào EEPROM.");
}

// Hàm load các giá trị cấu hình từ EEPROM (nếu đã lưu trước đó)
void loadScheduleFromEEPROM() {
  if (EEPROM.read(EEPROM_FLAG_ADDR) == EEPROM_FLAG_VALUE) {
    startHour   = EEPROM.read(ADDR_START_HOUR);
    startMinute = EEPROM.read(ADDR_START_MINUTE);
    endHour     = EEPROM.read(ADDR_END_HOUR);
    endMinute   = EEPROM.read(ADDR_END_MINUTE);
    Serial.println("Đã load lịch trình từ EEPROM:");
    Serial.print("Start: ");
    Serial.print(startHour);
    Serial.print(":");
    Serial.println(startMinute);
    Serial.print("End: ");
    Serial.print(endHour);
    Serial.print(":");
    Serial.println(endMinute);
  } else {
    Serial.println("Không có dữ liệu hợp lệ trên EEPROM. Sử dụng giá trị mặc định.");
    saveScheduleToEEPROM();
  }
}

// Hàm đồng bộ trạng thái relay với widget V0 trên Blynk chỉ khi trạng thái thay đổi
void syncRelayState(int currentState) {
  if (currentState != lastRelayState) {
    Blynk.virtualWrite(V0, currentState);
    Serial.print("Sync Relay State to Blynk V0: ");
    Serial.println(currentState);
    lastRelayState = currentState;
  }
}

// Hàm cập nhật giá trị từ Blynk cho nút override (Virtual Pin V0)
BLYNK_WRITE(V0) {
  int value = param.asInt();
  manualOverride = (value == 1);
  Serial.print("Manual override: ");
  Serial.println(manualOverride ? "ON" : "OFF");
}

// Hàm cập nhật giá trị từ Blynk cho thời gian bật/tắt
BLYNK_WRITE(V1) {
  startHour = param.asInt();
  Serial.print("Start Hour updated: ");
  Serial.println(startHour);
  saveScheduleToEEPROM();
}

BLYNK_WRITE(V2) {
  startMinute = param.asInt();
  Serial.print("Start Minute updated: ");
  Serial.println(startMinute);
  saveScheduleToEEPROM();
}

BLYNK_WRITE(V3) {
  endHour = param.asInt();
  Serial.print("End Hour updated: ");
  Serial.println(endHour);
  saveScheduleToEEPROM();
}

BLYNK_WRITE(V4) {
  endMinute = param.asInt();
  Serial.print("End Minute updated: ");
  Serial.println(endMinute);
  saveScheduleToEEPROM();
}

void setup() {
  Serial.begin(19200);
  EEPROM.begin(EEPROM_SIZE);
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, RELAY_INACTIVE_STATE);  // Tắt relay ban đầu

  // Load lịch trình từ EEPROM
  loadScheduleFromEEPROM();

  // Kết nối WiFi và Blynk
  Serial.println("Đang kết nối WiFi và Blynk...");
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, password);
  Serial.println("WiFi và Blynk đã kết nối.");
  Serial.print("Địa chỉ IP: ");
  Serial.println(WiFi.localIP());

  // Khởi tạo NTPClient
  timeClient.begin();
}

void loop() {
  Blynk.run();  // Xử lý sự kiện của Blynk
  timeClient.update();

  if (millis() - lastCheck >= checkInterval) {
    lastCheck = millis();
    bool relayOn = false;

    // Nếu manual override đang bật thì dùng giá trị override
    if (manualOverride) {
      relayOn = true;
      Serial.println("Relay: ON (manual override)");
    } else {
      // Lấy thời gian hiện tại từ NTP
      unsigned long epochTime = timeClient.getEpochTime();
      int currentHour = (epochTime % 86400L) / 3600;  // (0-23)
      int currentMinute = (epochTime % 3600) / 60;      // (0-59)

      Serial.print("Thời gian hiện tại: ");
      Serial.print(currentHour);
      Serial.print(":");
      Serial.println(currentMinute);

      // Chuyển đổi thời gian thành số phút kể từ nửa đêm
      int currentTimeMinutes = currentHour * 60 + currentMinute;
      int startTimeMinutes = startHour * 60 + startMinute;
      int endTimeMinutes   = endHour * 60 + endMinute;

      // Xét khoảng thời gian bật relay
      if (startTimeMinutes < endTimeMinutes) {
        if (currentTimeMinutes >= startTimeMinutes && currentTimeMinutes < endTimeMinutes) {
          relayOn = true;
        }
      } else {
        if (currentTimeMinutes >= startTimeMinutes || currentTimeMinutes < endTimeMinutes) {
          relayOn = true;
        }
      }
      Serial.print("Relay: ");
      Serial.println(relayOn ? "ON (schedule)" : "OFF (schedule)");
    }

    int newRelayState = relayOn ? RELAY_ACTIVE_STATE : RELAY_INACTIVE_STATE;
    digitalWrite(relayPin, newRelayState);
    syncRelayState(newRelayState == RELAY_ACTIVE_STATE ? 1 : 0);
  }
}
