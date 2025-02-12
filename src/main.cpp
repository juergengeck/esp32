#include <Arduino.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <mbedtls/sha256.h>  // For SHA256
#include <esp_random.h>      // For hardware RNG
#include <memory>
#include <BluetoothSerial.h>
#include "./storage/filesystem.h"
#include "./storage/storage_base.h"
#include "./chum/websocket_client_impl.h"
#include "./chum/config.h"
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include "esp_chip_info.h"
#include "esp_system.h"
#include "esp_wifi.h"

// Add these new includes
#include "./instance/instance.h"
#include "./instance/keys.h"
#include "./instance/ble_discovery.h"
#include "./display/display_manager.h"

using namespace one::storage;
using namespace chum;
using namespace one::instance;

// ONE Node configuration
#define NODE_NAME "esp32_one_node"
#define NODE_VERSION "0.1.0"
#define LED_PIN 2  // Built-in LED on most ESP32 dev boards

// Global instances
std::unique_ptr<IFileSystem> filesystem;
static std::unique_ptr<WebSocketClientImpl> wsClient;
static std::unique_ptr<Instance> instance;

// Add these to the NodeState struct
struct NodeState {
    bool isInitialized;
    bool storageReady;
    bool instanceReady;
    bool networkReady;
    bool cryptoReady;
    bool displayReady;  // New state for display
    bool btReady;      // New state for Bluetooth
} nodeState = {false, false, false, false, false, false, false};

// LED patterns
void flashError() {
    for(int i = 0; i < 3; i++) {  // Shorter error pattern
        digitalWrite(LED_PIN, HIGH);
        delay(100);
        digitalWrite(LED_PIN, LOW);
        delay(100);
    }
}

void flashSuccess() {
    digitalWrite(LED_PIN, HIGH);
    delay(10000);  // 10 seconds solid ON for success
    digitalWrite(LED_PIN, LOW);
}

void flashProgress() {
    digitalWrite(LED_PIN, HIGH);
    delay(50);
    digitalWrite(LED_PIN, LOW);
}

void heartbeat() {
    // Quick double-flash like a heartbeat
    digitalWrite(LED_PIN, HIGH);
    delay(100);
    digitalWrite(LED_PIN, LOW);
    delay(100);
    digitalWrite(LED_PIN, HIGH);
    delay(100);
    digitalWrite(LED_PIN, LOW);
}

void setLEDStatus(bool success) {
    if (success) {
        flashSuccess();
    } else {
        flashError();
    }
}

void printStorageMetrics() {
    if (filesystem) {
        size_t total = filesystem->totalSpace();
        size_t used = filesystem->usedSpace();
        size_t free = filesystem->freeSpace();
        
        Serial.println("\nStorage Metrics:");
        Serial.printf("Total Space: %u bytes (%.2f KB)\n", total, total/1024.0);
        Serial.printf("Used Space:  %u bytes (%.2f KB)\n", used, used/1024.0);
        Serial.printf("Free Space:  %u bytes (%.2f KB)\n", free, free/1024.0);
        Serial.printf("Usage:       %.1f%%\n", (used * 100.0) / total);
    }
}

// Initialize storage
bool initStorage() {
    if (!SPIFFS.begin(true)) {
        Serial.println("Failed to mount SPIFFS");
        return false;
    }

    filesystem = std::make_unique<ESPFileSystem>();
    if (!filesystem) {
        Serial.println("Failed to initialize filesystem");
        return false;
    }

    return true;
}

// Initialize ONE instance
bool initInstance() {
    instance = std::make_unique<Instance>(NODE_NAME, NODE_VERSION, "esp32_owner", "esp32@local");
    
    if (!instance->initialize()) {
        Serial.println("Failed to initialize instance");
        return false;
    }
    
    Serial.println("Instance initialized:");
    Serial.printf("Name: %s\n", instance->getName().c_str());
    Serial.printf("Owner: %s\n", instance->getOwner().c_str());
    
    return true;
}

// Initialize WebSocket
bool initWebSocket() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi not connected!");
        return false;
    }
    
    wsClient = std::make_unique<WebSocketClientImpl>();
    bool connected = wsClient->connect(config::WIFI_SSID, 
                                     config::WIFI_PASSWORD,
                                     config::WEBSOCKET_URL);
    
    if (!connected) {
        Serial.println("WebSocket connection failed!");
        return false;
    }
    
    Serial.println("WebSocket connected successfully");
    return true;
}

// Test crypto capabilities
bool initCrypto() {
    unsigned char hash[32];
    const char* test_data = "ONE test data";
    mbedtls_sha256_context ctx;
    
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, (const unsigned char*)test_data, strlen(test_data));
    mbedtls_sha256_finish(&ctx, hash);
    
    uint32_t random_number = esp_random();
    
    Serial.println("Crypto subsystem initialized");
    Serial.printf("Random number: %u\n", random_number);
    Serial.print("SHA256 test hash: ");
    for(int i = 0; i < 32; i++) {
        Serial.printf("%02x", hash[i]);
    }
    Serial.println();
    
    return true;
}

// Initialize display
bool initDisplay() {
    auto& display = one::display::DisplayManager::getInstance();
    if (!display.initialize()) {
        Serial.println("Display initialization failed!");
        return false;
    }
    
    display.showBootScreen(NODE_VERSION);
    Serial.println("Display initialized successfully");
    return true;
}

// Add BLE scanning callback
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
        Serial.printf("Device found: %s\n", advertisedDevice.toString().c_str());
        if (advertisedDevice.haveName()) {
            String name = advertisedDevice.getName().c_str();
            if (name.indexOf("esp32_") >= 0) {
                Serial.printf("  -> Potential ONE node found: %s\n", name.c_str());
            }
        }
    }
};

// Initialize Bluetooth
bool initBluetooth() {
    auto& bleDiscovery = one::instance::BLEDiscovery::getInstance();
    if (!bleDiscovery.initialize()) {
        Serial.println("Failed to initialize BLE Discovery");
        return false;
    }
    bleDiscovery.startAdvertising();
    Serial.println("BLE initialized successfully");
    Serial.printf("Device name: %s\n", NODE_NAME);
    return true;
}

// Scan for BLE devices
void scanBluetoothDevices() {
    Serial.println("Starting BLE scan...");
    
    BLEScan* pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pBLEScan->setActiveScan(true);
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);
    
    BLEScanResults foundDevices = pBLEScan->start(5, false);
    Serial.printf("Scan complete! Found %d devices\n", foundDevices.getCount());
    pBLEScan->clearResults();
}

// UART for Ox64 connection
#define OX64_TX_PIN 17  // ESP32's TX pin to Ox64's RX
#define OX64_RX_PIN 16  // ESP32's RX pin to Ox64's TX
#define OX64_BAUD 230400  // Baud rate for Ox64 (using macOS safe value)

// Buffer size for UART communication
#define BUFFER_SIZE 256

void printWiFiCapabilities() {
    Serial.println("\nWiFi Capabilities:");
    
    // Get WiFi capabilities
    wifi_mode_t mode;
    esp_wifi_get_mode(&mode);
    
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
}

void printBLECapabilities() {
    Serial.println("\nBluetooth Capabilities:");
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    
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
}

void printOtherWirelessCapabilities() {
    Serial.println("\nOther Wireless Information:");
    Serial.println("- Thread: Not supported natively");
    Serial.println("- Zigbee: Not supported natively");
    Serial.println("- Matter: Not supported natively");
    Serial.println("RF Output Power: 20dBm (100mW) maximum");
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    // Initialize LED
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    Serial.println("\nChip Information:");
    Serial.printf("Model: %s\n", ESP.getChipModel());
    Serial.printf("Cores: %d\n", chip_info.cores);
    Serial.printf("Silicon revision: %d\n", chip_info.revision);
    Serial.printf("Flash size: %dMB %s\n", spi_flash_get_chip_size() / (1024 * 1024),
        (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");
    Serial.printf("Free heap: %d\n", ESP.getFreeHeap());
    
    // Initialize WiFi for capability checking
    WiFi.mode(WIFI_MODE_STA);
    WiFi.begin();
    delay(100);
    
    // Print all capabilities
    printWiFiCapabilities();
    printBLECapabilities();
    printOtherWirelessCapabilities();
    
    // Initialize core components
    nodeState.storageReady = initStorage();
    nodeState.cryptoReady = initCrypto();
    
    // Initialize ONE instance
    auto& instance = Instance::getInstance();
    nodeState.instanceReady = instance.initialize();
    
    // Initialize BLE discovery
    auto& bleDiscovery = BLEDiscovery::getInstance();
    if (bleDiscovery.initialize()) {
        bleDiscovery.startAdvertising();
        nodeState.btReady = true;
        Serial.println("BLE advertising started with name: " + instance.getName());
    } else {
        Serial.println("Failed to initialize BLE");
    }

    // Print MAC addresses
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
    
    // Do an initial BLE scan
    scanBluetoothDevices();

    // Print final status
    Serial.println("\nInitialization complete:");
    Serial.printf("Storage:   %s\n", nodeState.storageReady ? "OK" : "FAIL");
    Serial.printf("Crypto:    %s\n", nodeState.cryptoReady ? "OK" : "FAIL");
    Serial.printf("Instance:  %s\n", nodeState.instanceReady ? "OK" : "FAIL");
    Serial.printf("BLE:       %s\n", nodeState.btReady ? "OK" : "FAIL");

    nodeState.isInitialized = nodeState.storageReady && 
                             nodeState.cryptoReady && 
                             nodeState.instanceReady;

    if (nodeState.isInitialized) {
        flashSuccess();
    } else {
        flashError();
    }
}

void loop() {
    if (!nodeState.isInitialized) {
        flashError();
        delay(1000);
        return;
    }

    // Handle WebSocket messages
    if (wsClient) {
        wsClient->update();
    }

    // Update display
    if (nodeState.displayReady) {
        auto& display = one::display::DisplayManager::getInstance();
        display.update();
    }

    // Show heartbeat
    heartbeat();
    delay(1000);
} 