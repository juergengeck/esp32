#include <Arduino.h>

// UART for Ox64 connection
#define OX64_TX_PIN 17  // ESP32's TX pin to Ox64's RX
#define OX64_RX_PIN 16  // ESP32's RX pin to Ox64's TX
#define OX64_BAUD 230400  // Baud rate for Ox64 (using macOS safe value)

// Buffer size for UART communication
#define BUFFER_SIZE 256

void setup() {
  // Initialize USB Serial (UART0)
  Serial.begin(115200);
  
  // Initialize UART2 for Ox64
  Serial2.begin(OX64_BAUD, SERIAL_8N1, OX64_RX_PIN, OX64_TX_PIN);
  
  Serial.println("ESP32 UART Bridge initialized");
  Serial.printf("USB Serial: 115200 baud\n");
  Serial.printf("Ox64 UART: %d baud\n", OX64_BAUD);
  Serial.printf("Ox64 TX: GPIO%d, RX: GPIO%d\n", OX64_TX_PIN, OX64_RX_PIN);
}

void loop() {
  uint8_t buf[BUFFER_SIZE];
  
  // Forward USB Serial to Ox64
  if (Serial.available()) {
    int len = Serial.readBytes(buf, BUFFER_SIZE);
    Serial2.write(buf, len);
  }
  
  // Forward Ox64 to USB Serial
  if (Serial2.available()) {
    int len = Serial2.readBytes(buf, BUFFER_SIZE);
    Serial.write(buf, len);
  }
} 