// Tiền linh kiện tầm 170k
// Kết nối phần cứng:
// - 4x4 Keypad: D2, D3, D4, D5, D6, A0, A1, A2
// - I2C LCD: SDA-A4, SCL-A5
// - RFID: SDA-D10, SCK-D13, MOSI-D11, MISO-D12, RST-D9
// - relay 1-D8, relay 2-7

// project mở cửa bằng mật khẩu hoặc là rfid
// các chức năng: 
//  + mở cửa bằng mật khẩu, thẻ rfid
//  + để đóng cửa nhấn phím A
//  + thay đổi mật khẩu bằng phím B trên bàn phím
//  + thêm/xóa thẻ mới bằng phím C và thẻ matter card
//  + nút * sẽ là clear các ký tự đã nhập
//  + nút # để thoát các chức năng

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <EEPROM.h>
#include <SPI.h>
#include <MFRC522.h>

// RFID Pin
#define SDA_PIN 10
#define SCK_PIN 13
#define MOSI_PIN 11
#define MISO_PIN 12
#define RST_PIN 9

// Số lượng thẻ tối đa & kích thước UID tối đa (có thể là 4 hoặc 7 byte)
#define MAX_CARDS 10
#define MAX_UID_LENGTH 7

// Relay
#define RELAY_1 8
#define RELAY_2 7

// Keypad cấu hình
#define ROWS 4
#define COLS 4
char keys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};
byte rowPins[ROWS] = {2, 3, 4, 5};
byte colPins[COLS] = {6, A0, A1, A2};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// LCD I2C
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Mật khẩu
const int passwordLength = 6;
char storedPassword[passwordLength + 1] = "000000";
char enteredPassword[passwordLength + 1] = "";
char newPassword[passwordLength + 1] = "";
int inputIndex = 0;
bool isChangingPassword = false;
bool isConfirmingNewPassword = false;

// Cửa
bool isDoorOpen = false;

// RFID
MFRC522 rfid(SDA_PIN, RST_PIN);

// Cấu trúc lưu thẻ (UID và kích thước)
struct Card {
  byte uid[MAX_UID_LENGTH];
  byte size;
};

// Thẻ master (ví dụ: 4-byte)
Card masterCard = { {0x99, 0x29, 0xDF, 0x00}, 4 };

// Mảng lưu thẻ và biến đếm
Card storedCards[MAX_CARDS];
int storedCardCount = 0;

// Các cờ thêm/xóa thẻ
bool isAddingCard = false;
bool isDeletingCard = false;

// Biến lưu thông điệp hiện tại của LCD (tối đa 16 ký tự)
char currentPrompt[17] = "NHAP MAT KHAU";

//---------------------------------------------------
// Các hàm hiển thị trên LCD
//---------------------------------------------------

// Hàm hiển thị thông điệp với delay truyền vào (đơn vị mili giây)
void showMessage(const char *message, unsigned long displayTime = 1000) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(message);
  // Cập nhật currentPrompt nếu cần
  strncpy(currentPrompt, message, 16);
  currentPrompt[16] = '\0';
  delay(displayTime); // Delay theo thời gian truyền vào
}


// Hiển thị mật khẩu dưới dạng dấu '*'
void showPassword() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(currentPrompt);
  lcd.setCursor(0, 1);
  for (int i = 0; i < inputIndex; i++) {
    lcd.print('*');
  }
}

// Hiển thị đếm ngược khi kích hoạt relay
void showCountdown(const char *message, int relayPin, int seconds) {
  digitalWrite(relayPin, HIGH);
  for (int i = seconds; i >= 0; i--) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(message);
    lcd.setCursor(0, 1);
    lcd.print(i);
    delay(1000);
  }
  digitalWrite(relayPin, LOW);
  showMessage("NHAP MAT KHAU");
}

//---------------------------------------------------
// Hàm điều khiển cửa, relay
//---------------------------------------------------
void toggleDoor() {
  if (isDoorOpen) {
    activateRelay(RELAY_2, "DANG DONG CUA");
    isDoorOpen = false;
  } else {
    activateRelay(RELAY_1, "DANG MO CUA");
    isDoorOpen = true;
  }
}

void activateRelay(int relayPin, const char *message) {
  showCountdown(message, relayPin, 5);
}

//---------------------------------------------------
// Các hàm xử lý mật khẩu
//---------------------------------------------------
void resetInput() {
  inputIndex = 0;
  enteredPassword[0] = '\0';

  lcd.setCursor(0, 1);
  lcd.print("                ");
  lcd.setCursor(0, 1);
}

void resetChangePassword() {
  isChangingPassword = false;
  isConfirmingNewPassword = false;
  newPassword[0] = '\0';
  resetInput();
  showMessage("NHAP MAT KHAU");
}

void startChangePassword() {
  isChangingPassword = true;
  isConfirmingNewPassword = false;
  showMessage("NHAP MK CU");
  resetInput();
}

void processChangePassword() {
  if (!isConfirmingNewPassword) {
    // Nhập mật khẩu cũ
    if (strcmp(enteredPassword, storedPassword) == 0) {
      showMessage("NHAP MK MOI");
      resetInput();
      isConfirmingNewPassword = true;
    } else {
      showMessage("MK CU SAI!", 2000);
      resetChangePassword();
    }
  } else {
    // Nhập mật khẩu mới lần 1 và xác nhận
    if (newPassword[0] == '\0') {
      strcpy(newPassword, enteredPassword);
      showMessage("NHAP LAI MK MOI");
      resetInput();
    } else {
      if (strcmp(newPassword, enteredPassword) == 0) {
        strcpy(storedPassword, newPassword);
        EEPROM.put(0, storedPassword);
        showMessage("MK DA THAY DOI!", 2000);
        resetChangePassword();
      } else {
        showMessage("MK KHONG KHOP!", 2000);
        resetChangePassword();
      }
    }
  }
}

//---------------------------------------------------
// Các hàm xử lý thẻ RFID và EEPROM
//---------------------------------------------------

// Lưu danh sách thẻ vào EEPROM
void saveCardsToEEPROM() {
  int addr = sizeof(storedPassword);
  EEPROM.put(addr, storedCardCount);
  addr += sizeof(storedCardCount);
  for (int i = 0; i < storedCardCount; i++) {
    EEPROM.put(addr, storedCards[i]);
    addr += sizeof(Card);
  }
}

// Kiểm tra thẻ master
bool isAuthorized(byte *uid, byte size) {
  if (size != masterCard.size) return false;
  for (byte i = 0; i < size; i++) {
    if (uid[i] != masterCard.uid[i]) return false;
  }
  return true;
}

// Kiểm tra thẻ đã được lưu chưa
bool isStoredCard(byte *uid, byte size) {
  for (int i = 0; i < storedCardCount; i++) {
    if (storedCards[i].size == size && memcmp(uid, storedCards[i].uid, size) == 0)
      return true;
  }
  return false;
}

// Thêm thẻ mới vào danh sách
void addNewCard(byte *uid, byte size) {
  if (isStoredCard(uid, size)) {
    showMessage("THE DA TON TAI", 2000);
    showMessage("THEM THE MOI");
    return;
  }
  if (storedCardCount >= MAX_CARDS) {
    showMessage("DANH SACH DAY", 2000);
    showMessage("THEM THE MOI");
    return;
  }
  storedCards[storedCardCount].size = size;
  memcpy(storedCards[storedCardCount].uid, uid, size);
  storedCardCount++;
  saveCardsToEEPROM();
  showMessage("THEM THE THANH CONG", 2000);
  showMessage("THEM THE MOI");
}

// Xóa thẻ khỏi danh sách
void deleteCard(byte *uid, byte size) {
  for (int i = 0; i < storedCardCount; i++) {
    if (storedCards[i].size == size && memcmp(uid, storedCards[i].uid, size) == 0) {
      for (int j = i; j < storedCardCount - 1; j++) {
        storedCards[j] = storedCards[j + 1];
      }
      storedCardCount--;
      saveCardsToEEPROM();
      showMessage("DA XOA THE", 2000);
      return;
    }
  }
  showMessage("THE KHONG TON TAI", 2000);
}

// Xóa toàn bộ thẻ
void clearAllCards() {
  int i = 0;
  // Duyệt qua danh sách và xóa các thẻ không phải thẻ master
  while(i < storedCardCount) {
    // Nếu thẻ hiện tại khớp với thẻ master thì bỏ qua
    if (storedCards[i].size == masterCard.size && 
        memcmp(storedCards[i].uid, masterCard.uid, masterCard.size) == 0) {
      i++; // Bỏ qua thẻ master
    } else {
      // Xóa thẻ tại vị trí i bằng cách dịch các phần tử phía sau lên
      for (int j = i; j < storedCardCount - 1; j++) {
        storedCards[j] = storedCards[j + 1];
      }
      storedCardCount--;
      // Không tăng i vì phần tử mới vừa được dịch lên vị trí i cần được kiểm tra
    }
  }
  saveCardsToEEPROM();
  showMessage("XOA TAT CA THE!", 2000);
  isDeletingCard = false;
  showMessage("NHAP MAT KHAU");
}


//---------------------------------------------------
// Các hàm xử lý quét RFID và Keypad
//---------------------------------------------------

// Xử lý đầu vào từ Keypad
void handleInput(char key) {
  if (inputIndex < passwordLength) {
    enteredPassword[inputIndex++] = key;
    enteredPassword[inputIndex] = '\0';
  }
  showPassword();

  if (inputIndex == passwordLength) {
    if (isChangingPassword) {
      processChangePassword();
    } else {
      if (strcmp(enteredPassword, storedPassword) == 0) {
        toggleDoor();
      } else {
        showMessage("SAI MAT KHAU", 2000);
        showMessage("NHAP MAT KHAU");
      }
      resetInput();
    }
  }
}

// Quét RFID và xử lý theo từng chế độ
void checkRFID() {
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) return;

  // Đọc danh sách thẻ từ EEPROM
  int addr = sizeof(storedPassword);
  EEPROM.get(addr, storedCardCount);
  addr += sizeof(storedCardCount);
  for (int i = 0; i < storedCardCount; i++) {
    EEPROM.get(addr, storedCards[i]);
    addr += sizeof(Card);
  }

  // Debug in danh sách thẻ
  Serial.println("DANH SACH THE TRONG EEPROM:");
  for (int i = 0; i < storedCardCount; i++) {
    Serial.print("The ");
    Serial.print(i + 1);
    Serial.print(": ");
    for (byte j = 0; j < storedCards[i].size; j++) {
      Serial.print(storedCards[i].uid[j], HEX);
      Serial.print(" ");
    }
    Serial.println();
  }
  Serial.print("RFID UID: ");
  for (byte i = 0; i < rfid.uid.size; i++) {
    Serial.print(rfid.uid.uidByte[i], HEX);
    Serial.print(" ");
  }
  Serial.println();

  // Xử lý thẻ tùy theo chế độ
  if (isAddingCard) {
    addNewCard(rfid.uid.uidByte, rfid.uid.size);
  } else if (isDeletingCard) {
    deleteCard(rfid.uid.uidByte, rfid.uid.size);
  } else if (isAuthorized(rfid.uid.uidByte, rfid.uid.size) || isStoredCard(rfid.uid.uidByte, rfid.uid.size)) {
    toggleDoor();
  } else {
    showMessage("THE KHONG HOP LE", 2000);
    showMessage("NHAP MAT KHAU");
  }
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}

// Chế độ thêm thẻ
void startAddingCard() {
  showMessage("QUET THE MASTER");
  bool isMasterCardScanned = false;
  bool shouldExit = false;

  while (!isMasterCardScanned && !shouldExit) {
    char key = keypad.getKey();
    if (key == '#') {
      shouldExit = true;
      break;
    }
    while (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
      key = keypad.getKey();
      if (key == '#') {
        shouldExit = true;
        break;
      }
    }
    if (shouldExit) break;
    if (isAuthorized(rfid.uid.uidByte, rfid.uid.size)) {
      isAddingCard = true;
      showMessage("XAC NHAN MASTER", 2000);
      showMessage("THEM THE MOI");
      isMasterCardScanned = true;
    } else {
      showMessage("THE MASTER SAI", 2000);
      showMessage("QUET THE MASTER");
    }
    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
  }
  if (shouldExit) {
    isAddingCard = false;
    showMessage("NHAP MAT KHAU");
  }
}

// Chế độ xóa thẻ
void startDeletingCard() {
  showMessage("QUET THE MASTER");
  bool isMasterCardScanned = false;
  bool shouldExit = false;
  
  // Quét thẻ master
  while (!isMasterCardScanned && !shouldExit) {
    char key = keypad.getKey();
    if (key == '#') {
      shouldExit = true;
      break;
    }

    while (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
      key = keypad.getKey();
      if (key == '#') {
        shouldExit = true;
        break;
      }
    }
    if (shouldExit) break;
    if (isAuthorized(rfid.uid.uidByte, rfid.uid.size)) {
      isMasterCardScanned = true;
      showMessage("XAC NHAN MASTER", 2000);
      // Sau khi quét master, chuyển sang chế độ xóa thẻ
      isDeletingCard = true;
      showMessage("QUET THE CAN XOA");
    } else {
      showMessage("THE MASTER SAI", 2000);
      showMessage("QUET THE MASTER");
    }
    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
  }
  
  if (shouldExit) {
    isDeletingCard = false;
    showMessage("NHAP MAT KHAU");
    return;
  }
  
  // Biến tạm để tích lũy mã xóa từ bàn phím (4 ký tự)
  static char deletionInput[5] = "";  // 4 ký tự + '\0'
  static int deletionIndex = 0;
  
  // Vòng lặp xử lý xóa thẻ
  while (!shouldExit) {
    char key = keypad.getKey();
    if (key == '#') {
      shouldExit = true;
      break;
    }
    
    // Nếu có phím bàn phím được nhập
    if (key != NO_KEY) {
      // Chỉ xử lý các phím số
      if (key >= '0' && key <= '9') {
        deletionInput[deletionIndex++] = key;
        deletionInput[deletionIndex] = '\0';
      }
      // Khi đã nhập đủ 4 ký tự
      if (deletionIndex == 4) {
        if (strcmp(deletionInput, "0000") == 0) {
          clearAllCards();
          // Reset biến nhập để chờ thêm (nếu cần)
          deletionIndex = 0;
          deletionInput[0] = '\0';
          // Sau khi xóa toàn bộ, ta có thể thoát khỏi chế độ xóa
          shouldExit = true;
          break;
        } else {
          showMessage("SAI MA XOA", 2000);
          showMessage("QUET THE CAN XOA");
          // Reset biến nhập để cho phép nhập lại
          deletionIndex = 0;
          deletionInput[0] = '\0';
        }
      }
    }
    
    // Đồng thời, nếu có thẻ RFID được quét để xóa riêng lẻ
    if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
      if (isStoredCard(rfid.uid.uidByte, rfid.uid.size)) {
        deleteCard(rfid.uid.uidByte, rfid.uid.size);
        showMessage("QUET THE CAN XOA");
      } else {
        showMessage("THE KHONG TON TAI", 2000);
        showMessage("QUET THE CAN XOA");
      }
      rfid.PICC_HaltA();
      rfid.PCD_StopCrypto1();
    }
  }
  
  // Khi thoát chế độ xóa
  isDeletingCard = false;
  // Reset biến nhập bàn phím xóa
  deletionIndex = 0;
  deletionInput[0] = '\0';
  showMessage("NHAP MAT KHAU");
}

// Dừng chế độ thêm/xóa thẻ
void stopAddingAndDeletingCard() {
  isAddingCard = false;
  isDeletingCard = false;
  isChangingPassword = false;
  isConfirmingNewPassword = false;
  resetInput();
  showMessage("NHAP MAT KHAU");
}

//---------------------------------------------------
// Hàm setup và loop
//---------------------------------------------------
void setup() {
  Serial.begin(9600);
  SPI.begin();
  rfid.PCD_Init();

  lcd.begin();
  lcd.backlight();

  pinMode(RELAY_1, OUTPUT);
  pinMode(RELAY_2, OUTPUT);
  digitalWrite(RELAY_1, LOW);
  digitalWrite(RELAY_2, LOW);

  // Cấu hình chân A3 cho nút nhấn
  pinMode(A3, INPUT_PULLUP);

  // Lấy mật khẩu từ EEPROM và kiểm tra hợp lệ
  EEPROM.get(0, storedPassword);
  bool isInvalid = false;
  for (int i = 0; i < passwordLength; i++) {
    if (storedPassword[i] < '0' || storedPassword[i] > '9') {
      isInvalid = true;
      break;
    }
  }
  if (isInvalid) {
    strcpy(storedPassword, "000000");
    EEPROM.put(0, storedPassword);
  }
  Serial.print("STORED PASSWORD: ");
  Serial.println(storedPassword);
  showMessage("NHAP MAT KHAU");
}

void loop() {
  char key = keypad.getKey();
  if(key) {
    Serial.print("PHIM NHAN: ");
    Serial.println(key);
  }

  if (key && key >= '0' && key <= '9') {
    handleInput(key);
  } else if (key == 'B') {
    startChangePassword();
  } else if (key == '*') {
    resetInput();
  } else if (key == 'A') {
    activateRelay(RELAY_2, "DANG DONG CUA");
  } else if (key == 'C') {
    startAddingCard();
  } else if (key == 'D') {
    startDeletingCard();
  } else if (key == '#') {
    stopAddingAndDeletingCard();
  } else if (digitalRead(A3) == HIGH) {
    toggleDoor();
  }
  checkRFID(); // Quét RFID liên tục
}
