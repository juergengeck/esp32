#include <Arduino.h>

// UART for Ox64 connection
#define OX64_TX_PIN 17  // ESP32's TX pin to Ox64's RX
#define OX64_RX_PIN 16  // ESP32's RX pin to Ox64's TX
#define OX64_BAUD 115200  // Common bootloader speed

// Boot0 UART binary
const uint8_t boot0_uart[] = {
  // We'll add the binary content here
  0x00  // Placeholder
};

// Buffer size for UART communication
#define BUFFER_SIZE 1024

bool transferMode = false;

void setup() {
  // Initialize USB Serial (UART0)
  Serial.begin(115200);
  
  // Initialize UART2 for Ox64
  Serial2.begin(OX64_BAUD, SERIAL_8N1, OX64_RX_PIN, OX64_TX_PIN);
  
  Serial.println("ESP32 UART Bridge initialized");
  Serial.printf("USB Serial: 115200 baud\n");
  Serial.printf("Ox64 UART: %d baud\n", OX64_BAUD);
  Serial.printf("Ox64 TX: GPIO%d, RX: GPIO%d\n", OX64_TX_PIN, OX64_RX_PIN);
  Serial.println("Commands:");
  Serial.println("t - Enter transfer mode");
  Serial.println("x - Exit transfer mode");
  Serial.println("r - Reset Ox64 (toggle RTS)");
}

void sendBoot0() {
  Serial.println("Sending boot0_uart.bin...");
  Serial2.write(boot0_uart, sizeof(boot0_uart));
  Serial.println("Done sending boot0_uart.bin");
}

void resetOx64() {
  // Toggle RTS to reset the Ox64
  Serial2.end();
  delay(100);
  Serial2.begin(OX64_BAUD, SERIAL_8N1, OX64_RX_PIN, OX64_TX_PIN);
  Serial.println("Ox64 reset requested");
}

void loop() {
  static uint8_t buf[BUFFER_SIZE];
  
  // Check for commands or data from USB Serial
  if (Serial.available()) {
    if (!transferMode) {
      char cmd = Serial.read();
      switch(cmd) {
        case 't':
          transferMode = true;
          Serial.println("Transfer mode enabled - send binary data");
          break;
        case 'r':
          resetOx64();
          break;
        default:
          Serial2.write(cmd);
      }
    } else {
      // In transfer mode - forward all data directly
      while (Serial.available() && Serial2.availableForWrite()) {
        int len = Serial.readBytes(buf, min(Serial.available(), BUFFER_SIZE));
        if (len == 1 && buf[0] == 'x') {
          transferMode = false;
          Serial.println("Transfer mode disabled");
          break;
        }
        Serial2.write(buf, len);
        Serial2.flush(); // Wait for all data to be sent
      }
    }
  }
  
  // Forward Ox64 to USB Serial with hex printing
  if (Serial2.available()) {
    int len = Serial2.readBytes(buf, min(Serial2.available(), BUFFER_SIZE));
    // Print hex values
    for(int i = 0; i < len; i++) {
      Serial.printf("%02X ", buf[i]);
    }
    Serial.println();
  }
} 