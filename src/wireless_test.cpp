#include <Arduino.h>
#include "esp_chip_info.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include <WiFi.h>

void setup() {
    Serial.begin(115200);
    delay(1000);

    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    Serial.println("\nChip Information:");
    Serial.printf("Model: %s\n", ESP.getChipModel());
    Serial.printf("Cores: %d\n", chip_info.cores);
    Serial.printf("Silicon revision: %d\n", chip_info.revision);
    
    Serial.println("\nWiFi Capabilities:");
    // Initialize WiFi to check capabilities
    WiFi.mode(WIFI_MODE_STA);
    WiFi.begin();
    delay(100);
    
    uint8_t protocol;
    esp_wifi_get_protocol(WIFI_IF_STA, &protocol);
    Serial.printf("Protocols supported: ");
    if (protocol & WIFI_PROTOCOL_11B) Serial.print("802.11b ");
    if (protocol & WIFI_PROTOCOL_11G) Serial.print("802.11g ");
    if (protocol & WIFI_PROTOCOL_11N) Serial.print("802.11n ");
    if (protocol & WIFI_PROTOCOL_LR) Serial.print("Long Range ");
    Serial.println();
    
    Serial.println("Security modes: WEP, WPA, WPA2, WPA3");
    Serial.println("Maximum WiFi Speed: 150 Mbps (802.11n)");
    Serial.println("WiFi Frequency: 2.4 GHz");
    
    Serial.println("\nBluetooth Capabilities:");
    if (chip_info.features & CHIP_FEATURE_BT) {
        Serial.println("- Classic Bluetooth: Yes (v4.2)");
        Serial.println("  * BR/EDR (Classic): Supported");
        Serial.println("  * Maximum Speed: 3 Mbps");
    }
    if (chip_info.features & CHIP_FEATURE_BLE) {
        Serial.println("- BLE (Bluetooth Low Energy): Yes (v4.2)");
        Serial.println("  * Maximum Speed: 2 Mbps (PHY 2M)");
        Serial.println("  * Connections: Up to 9 simultaneous");
    }
    
    Serial.println("\nOther Wireless Information:");
    Serial.println("- Thread: Not supported natively");
    Serial.println("- Zigbee: Not supported natively");
    Serial.println("- Matter: Not supported natively");
    Serial.println("RF Output Power: 20dBm (100mW) maximum");
    
    Serial.println("\nMAC Addresses:");
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    Serial.printf("- WiFi Station: %02X:%02X:%02X:%02X:%02X:%02X\n", 
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    Serial.printf("- WiFi SoftAP: %02X:%02X:%02X:%02X:%02X:%02X\n", 
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    esp_read_mac(mac, ESP_MAC_BT);
    Serial.printf("- Bluetooth: %02X:%02X:%02X:%02X:%02X:%02X\n", 
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void loop() {
    delay(1000);
} 