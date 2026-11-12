#include <Arduino.h>

#define BUILTIN_BUTTON 0 //Button default trên Relay x8

const int relayPins[] = {32, 33, 25, 26, 27, 14, 12, 13};
const int numRelays = sizeof(relayPins) / sizeof(relayPins[0]);

#define RELAY_ON   HIGH
#define RELAY_OFF  LOW

/*
kiểm soát các mode 0 -> 4
mode 0: Tắt
mode 1: Bật
mode 2: Blink all
mode 3: Chạy đuổi 2 chiều (1 -> 8 -> 1)
mode 4: Sáng theo chiều 1 -> 8, rồi tắt theo chiều ngược lại
*/
int currentMode = 0;
const int TOTAL_MODES = 5;

// Quản lý nút bấm
int lastButtonState = HIGH;
unsigned long buttonPressTime = 0;
bool isButtonPressed = false;
const unsigned long debounceDelay = 40;
const unsigned long longPressTime = 1000; 

// Quản lý tốc độ Led 
unsigned long previousBlinkMillis = 0;
int blinkSpeed = 250;        

// Biến cho mode 2 
bool syncBlinkState = false;    

// Biến cho mode 3
int chaseCurrentIndex = 0;
int chaseDirection = 1;

// Biến cho mode 4
int fillStep = 0;
bool isFilling = true;     


void setAllRelaysOff() {
  for (int i = 0; i < numRelays; i++) {
    digitalWrite(relayPins[i], RELAY_OFF);
  }
}

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
}

void loop() {
  // Nhận lệnh từ UART
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

  // Xử lý Button
  int reading = digitalRead(BUILTIN_BUTTON);

  // Nhấn nút
  if (reading == LOW && lastButtonState == HIGH) {
    buttonPressTime = millis();
    isButtonPressed = true;
  }

  // Nhả nút
  if (reading == HIGH && lastButtonState == LOW && isButtonPressed) {
    unsigned long pressDuration = millis() - buttonPressTime;
  // Nhấn giữ  thay đổi tốc độ
    if (pressDuration >= longPressTime) {
      if (blinkSpeed == 100) blinkSpeed = 500;
      else if (blinkSpeed == 500) blinkSpeed = 1000;
      else blinkSpeed = 100;
      Serial.printf("[ESP32] -> Nhan giu: Doi toc do thanh %d ms\n", blinkSpeed);
    } 
    else if (pressDuration >= debounceDelay) {
      // Nhấn nhả đổi mode
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

  // Modes
  unsigned long currentMillis = millis();
  // Mode 2
  if (currentMode == 2) {
    if (currentMillis - previousBlinkMillis >= (unsigned long)blinkSpeed) {
      previousBlinkMillis = currentMillis;
      syncBlinkState = !syncBlinkState;
      for (int i = 0; i < numRelays; i++) {
        digitalWrite(relayPins[i], syncBlinkState ? RELAY_ON : RELAY_OFF);
      }
    }
  }

  // Mode 3
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

  // Mode 4
  else if (currentMode == 4) {
    if (currentMillis - previousBlinkMillis >= (unsigned long)blinkSpeed) {
      previousBlinkMillis = currentMillis;

      if (isFilling) {
        
        digitalWrite(relayPins[fillStep], RELAY_ON);
        fillStep++;
        if (fillStep >= numRelays) {
          fillStep = numRelays - 1; 
          isFilling = false;
        }
      } else {
        
        digitalWrite(relayPins[fillStep], RELAY_OFF);
        fillStep--;
        if (fillStep < 0) {
          fillStep = 0;             
          isFilling = true;
        }
      }
    }
  }
}