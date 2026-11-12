#include <Arduino.h>

// Nút bấm tích hợp trên bo ESP32 Relay x8 (GPIO 0)
#define BUILTIN_BUTTON 0

// Danh sách các chân kích 8 Relay trên bo
const int relayPins[] = {32, 33, 25, 26, 27, 14, 12, 13};
const int numRelays = sizeof(relayPins) / sizeof(relayPins[0]);

// Mức kích logic của bo (Active HIGH: 1 = Bật, 0 = Tắt)
#define RELAY_ON   HIGH
#define RELAY_OFF  LOW

// Quản lý chế độ (0 -> 4)
int currentMode = 0;
const int TOTAL_MODES = 5;

// Biến quản lý nút bấm & chống dội phím (Debounce + Phân biệt Click / Long Press)
int lastButtonState = HIGH;
unsigned long buttonPressTime = 0;
bool isButtonPressed = false;
const unsigned long debounceDelay = 40;
const unsigned long longPressTime = 1000; // Nhấn giữ > 1s để đổi tốc độ

// Biến quản lý thời gian nháy (Non-blocking với millis)
unsigned long previousBlinkMillis = 0;
int blinkSpeed = 250;          // Tốc độ nháy mặc định: 250ms
bool syncBlinkState = false;    // Trạng thái cho Mode 2

// Biến chạy đuổi cho Mode 3 (1 -> 8 -> 1)
int chaseCurrentIndex = 0;
int chaseDirection = 1;

// Biến điều khiển hiệu ứng Fill-up cho Mode 4
int fillStep = 0;
bool isFilling = true;         // true: đang sáng dồn (1->8), false: đang tắt dồn (8->1)

// Tắt toàn bộ 8 Relay
void setAllRelaysOff() {
  for (int i = 0; i < numRelays; i++) {
    digitalWrite(relayPins[i], RELAY_OFF);
  }
}

// Bật toàn bộ 8 Relay
void setAllRelaysOn() {
  for (int i = 0; i < numRelays; i++) {
    digitalWrite(relayPins[i], RELAY_ON);
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(BUILTIN_BUTTON, INPUT_PULLUP);

  for (int i = 0; i < numRelays; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], RELAY_OFF);
  }

  Serial.println("\n=== ESP32 RELAY X8: FILL-UP (SANG 1->8, TAT 8->1) ===");
  Serial.println("  Mode 0: TAT HET");
  Serial.println("  Mode 1: BAT HET");
  Serial.println("  Mode 2: BLINK DONG LOAT");
  Serial.println("  Mode 3: BLINK CHAY DUOI 2 CHIEU");
  Serial.println("  Mode 4: SANG DON (1->8) & TAT DON NGUOC (8->1)");
  Serial.println("Lenh UART:");
  Serial.println("  - S <ms>    : Doi toc do (VD: S 100, S 350)");
  Serial.println("  - MODE <0-4>: Chuyen thang den Mode chi dinh");
  Serial.println("=====================================================");
}

void loop() {
  // ----------------------------------------------------
  // 1. NHẬN LỆNH ĐIỀU CHỈNH TỪ CỔNG UART
  // ----------------------------------------------------
  if (Serial.available() > 0) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toUpperCase();

    if (cmd.startsWith("S ")) {
      int val = cmd.substring(2).toInt();
      if (val >= 20 && val <= 5000) {
        blinkSpeed = val;
        Serial.printf("[ESP32] -> Toc do nhay cap nhat: %d ms\n", blinkSpeed);
      } else {
        Serial.println("[ESP32 Error] -> Toc do phai tu 20ms den 5000ms!");
      }
    } 
    else if (cmd.startsWith("MODE ")) {
      int newMode = cmd.substring(5).toInt();
      if (newMode >= 0 && newMode < TOTAL_MODES) {
        currentMode = newMode;
        if (currentMode == 0) setAllRelaysOff();
        else if (currentMode == 1) setAllRelaysOn();
        else if (currentMode == 4) {
          setAllRelaysOff();
          fillStep = 0;
          isFilling = true;
        }
        Serial.printf("[ESP32] -> Chuyen sang Mode %d\n", currentMode);
      }
    }
  }

  // ----------------------------------------------------
  // 2. QUÉT VÀ XỬ LÝ NÚT NHẤN (CLICK / LONG PRESS)
  // ----------------------------------------------------
  int reading = digitalRead(BUILTIN_BUTTON);

  // Nhấn nút (cạnh xuống)
  if (reading == LOW && lastButtonState == HIGH) {
    buttonPressTime = millis();
    isButtonPressed = true;
  }

  // Nhả nút (cạnh lên)
  if (reading == HIGH && lastButtonState == LOW && isButtonPressed) {
    unsigned long pressDuration = millis() - buttonPressTime;

    if (pressDuration >= longPressTime) {
      // Nhấn giữ > 1 giây: Đổi nhanh tốc độ
      if (blinkSpeed == 100) blinkSpeed = 500;
      else if (blinkSpeed == 500) blinkSpeed = 1000;
      else blinkSpeed = 100;
      Serial.printf("[ESP32] -> Nhan giu: Doi toc do thanh %d ms\n", blinkSpeed);
    } 
    else if (pressDuration >= debounceDelay) {
      // Nhấn nhả nhanh: Chuyển mode vòng tròn (0 -> 4)
      currentMode = (currentMode + 1) % TOTAL_MODES;
      chaseCurrentIndex = 0;
      chaseDirection = 1;
      syncBlinkState = false;
      fillStep = 0;
      isFilling = true;

      Serial.printf("[ESP32] -> Chuyen sang Mode %d: ", currentMode);
      switch (currentMode) {
        case 0:
          setAllRelaysOff();
          Serial.println("TAT TOAN BO");
          break;
        case 1:
          setAllRelaysOn();
          Serial.println("BAT TOAN BO");
          break;
        case 2:
          Serial.printf("BLINK DONG LOAT (%d ms)\n", blinkSpeed);
          break;
        case 3:
          setAllRelaysOff();
          Serial.printf("BLINK CHAY DUOI 2 CHIEU (%d ms)\n", blinkSpeed);
          break;
        case 4:
          setAllRelaysOff();
          Serial.printf("FILL-UP (SANG 1->8, TAT 8->1) (%d ms)\n", blinkSpeed);
          break;
      }
    }
    isButtonPressed = false;
  }
  lastButtonState = reading;

  // ----------------------------------------------------
  // 3. THỰC THI HIỆU ỨNG THEO CHẾ ĐỘ HIỆN TẠI
  // ----------------------------------------------------
  unsigned long currentMillis = millis();

  // Mode 2: Nhấp nháy đồng loạt cả 8 kênh
  if (currentMode == 2) {
    if (currentMillis - previousBlinkMillis >= (unsigned long)blinkSpeed) {
      previousBlinkMillis = currentMillis;
      syncBlinkState = !syncBlinkState;
      for (int i = 0; i < numRelays; i++) {
        digitalWrite(relayPins[i], syncBlinkState ? RELAY_ON : RELAY_OFF);
      }
    }
  }

  // Mode 3: Chạy đuổi 2 chiều (1 -> 8 -> 1)
  else if (currentMode == 3) {
    if (currentMillis - previousBlinkMillis >= (unsigned long)blinkSpeed) {
      previousBlinkMillis = currentMillis;

      for (int i = 0; i < numRelays; i++) {
        digitalWrite(relayPins[i], RELAY_OFF);
      }
      digitalWrite(relayPins[chaseCurrentIndex], RELAY_ON);

      chaseCurrentIndex += chaseDirection;
      if (chaseCurrentIndex >= numRelays - 1) {
        chaseCurrentIndex = numRelays - 1;
        chaseDirection = -1;
      } else if (chaseCurrentIndex <= 0) {
        chaseCurrentIndex = 0;
        chaseDirection = 1;
      }
    }
  }

  // Mode 4: Sáng dồn từ 1->8 và tắt dần ngược chiều từ 8->1
  else if (currentMode == 4) {
    if (currentMillis - previousBlinkMillis >= (unsigned long)blinkSpeed) {
      previousBlinkMillis = currentMillis;

      if (isFilling) {
        // Bật dần: 0 -> 1 -> ... -> 7
        digitalWrite(relayPins[fillStep], RELAY_ON);
        fillStep++;
        if (fillStep >= numRelays) {
          fillStep = numRelays - 1; // Đổi mốc bắt đầu tắt từ 7
          isFilling = false;
        }
      } else {
        // Tắt dần ngược lại: 7 -> 6 -> ... -> 0
        digitalWrite(relayPins[fillStep], RELAY_OFF);
        fillStep--;
        if (fillStep < 0) {
          fillStep = 0;             // Reset về 0 để sáng dồn lại
          isFilling = true;
        }
      }
    }
  }
}