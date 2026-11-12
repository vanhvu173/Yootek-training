#include <Arduino.h>

// Danh sách chân GPIO của 8 Relay (tương ứng 8 đèn LED relay trên bo)
const int relayPins[] = {32, 33, 25, 26, 27, 14, 12, 13};
const int numRelays = sizeof(relayPins) / sizeof(relayPins[0]);

// Chân User LED rời trên bo mạch (nếu muốn nháy riêng LED này)
const int USER_LED_PIN = 23;

// Đa số mạch ESP32 8 relay 5-30V dùng kích mức CAO (HIGH = Bật LED/Đóng Relay)
#define RELAY_ON   HIGH
#define RELAY_OFF  LOW

// Các biến trạng thái điều khiển qua UART
bool isBlinking = false;       // Trạng thái cho phép nháy hay không
int blinkDelay = 200;          // Tốc độ nháy (ms)
unsigned long previousMillis = 0;
int currentStep = 0;

void turnAllOff() {
  for (int i = 0; i < numRelays; i++) {
    digitalWrite(relayPins[i], RELAY_OFF);
  }
  digitalWrite(USER_LED_PIN, LOW);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Cấu hình chân Output
  for (int i = 0; i < numRelays; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], RELAY_OFF);
  }
  pinMode(USER_LED_PIN, OUTPUT);
  digitalWrite(USER_LED_PIN, LOW);

  Serial.println("=== DIEU KHIEN BLINK LED RELAY X8 QUA UART ===");
  Serial.println("Cac lenh dieu khien:");
  Serial.println("  START      : Bat dau che do Blink chay duoi");
  Serial.println("  STOP       : Dung blink va tat het den");
  Serial.println("  SPEED <ms> : Doi toc do blink (Vi du: SPEED 100 hoac SPEED 500)");
  Serial.println("===============================================");
}

void loop() {
  // 1. Lắng nghe và xử lý lệnh từ cổng UART
  if (Serial.available() > 0) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toUpperCase();

    if (cmd == "G") {
      isBlinking = true;
      currentStep = 0;
      previousMillis = millis();
      Serial.println("[ESP32] -> Da BAT che do Blink.");
    } 
    else if (cmd == "S") {
      isBlinking = false;
      turnAllOff();
      Serial.println("[ESP32] -> Da DUNG Blink va TAT het LED.");
    } 
    else if (cmd.startsWith("P ")) {
      int newSpeed = cmd.substring(2).toInt();
      if (newSpeed >= 20 && newSpeed <= 5000) {
        blinkDelay = newSpeed;
        Serial.printf("[ESP32] -> Toc do blink moi: %d ms\n", blinkDelay);
      } else {
        Serial.println("[ESP32] -> Toc do khong hop le (20 - 5000 ms).");
      }
    } 
    else if (cmd.length() > 0) {
      Serial.println("[ESP32] -> Lenh khong xac dinh: " + cmd);
    }
  }

  // 2. Chạy hiệu ứng Blink không dùng delay() (Non-blocking) để UART luôn phản hồi tức thì
  if (isBlinking) {
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= blinkDelay) {
      previousMillis = currentMillis;

      // Tắt tất cả các đèn
      for (int j = 0; j < numRelays; j++) {
        digitalWrite(relayPins[j], RELAY_OFF);
      }

      // Bật duy nhất LED ở vị trí hiện tại
      digitalWrite(relayPins[currentStep], RELAY_ON);

      // Nhấp nháy kèm User LED ở chân 23 để theo dõi
      digitalWrite(USER_LED_PIN, (currentStep % 2 == 0) ? HIGH : LOW);

      // Chuyển sang LED tiếp theo
      currentStep++;
      if (currentStep >= numRelays) {
        currentStep = 0;
      }
    }
  }
}