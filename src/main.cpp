#include <Arduino.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <mbedtls/sha256.h>  // For SHA256
#include <esp_random.h>      // For hardware RNG
#include <memory>
#include "storage/storage_base.h"
#include "storage/filesystem.h"

using namespace one::storage;

// ONE Node configuration
#define NODE_NAME "esp32_one_node"
#define NODE_VERSION "0.1.0"
#define LED_PIN 2  // Built-in LED on most ESP32 dev boards

// Basic node state
struct NodeState {
  bool isInitialized;
  bool storageReady;
  bool networkReady;
  bool cryptoReady;
} nodeState = {false, false, false, false};

// Global filesystem instance
static std::unique_ptr<IFileSystem> filesystem;

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
  if(!SPIFFS.begin(true)) {
    Serial.println("Storage initialization failed!");
    return false;
  }
  Serial.println("Storage initialized");
  return true;
}

// Initialize network (will be configured later)
bool initNetwork() {
  // For now, just initialize WiFi in station mode
  WiFi.mode(WIFI_STA);
  return true;
}

// Test crypto capabilities
bool initCrypto() {
  // Test SHA256
  unsigned char hash[32];
  const char* test_data = "ONE test data";
  mbedtls_sha256_context ctx;
  
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts(&ctx, 0); // 0 for SHA256, 1 for SHA224
  mbedtls_sha256_update(&ctx, (const unsigned char*)test_data, strlen(test_data));
  mbedtls_sha256_finish(&ctx, hash);
  
  // Test RNG
  uint32_t random_number = esp_random();
  
  Serial.println("Crypto subsystem initialized");
  Serial.print("Random number: ");
  Serial.println(random_number);
  Serial.print("SHA256 test hash: ");
  for(int i= 0; i< 32; i++) {
    char str[3];
    sprintf(str, "%02x", (int)hash[i]);
    Serial.print(str);
  }
  Serial.println();
  
  return true;
}

void setup() {
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);  // Start with LED off
    
    Serial.begin(115200);
    Serial.println("\nInitializing ONE Node...");
    
    // Initialize node components with progress indication
    flashProgress();
    nodeState.storageReady = initStorage();
    
    flashProgress();
    nodeState.networkReady = initNetwork();
    
    flashProgress();
    nodeState.cryptoReady = initCrypto();
    
    if (nodeState.storageReady && nodeState.networkReady && nodeState.cryptoReady) {
        nodeState.isInitialized = true;
        Serial.println("ONE Node initialized successfully");
    } else {
        Serial.println("ONE Node initialization failed!");
        flashError();
        return;
    }

    delay(1000);
    
    Serial.println("\nInitializing Storage System...");
    bool success = true;
    
    flashProgress();
    if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS Mount Failed!");
        success = false;
    }
    
    flashProgress();
    if (success && !initStorage("test_instance", false)) {
        Serial.println("Storage initialization failed!");
        success = false;
    }
    
    if (success) {
        filesystem = std::unique_ptr<IFileSystem>(new ESPFileSystem());
        Serial.println("Storage system initialized successfully!");
        
        flashProgress();
        printStorageMetrics();
        
        Serial.println("\nWriting test files...");
        const char* testData = "This is test data to verify storage system functionality.";
        
        flashProgress();
        StorageResult result = writeUTF8TextFile("test1.txt", testData);
        Serial.printf("Writing test1.txt: %s\n", result.success ? "Success" : "Failed");
        success &= result.success;
        
        if (filesystem->createDir("/data")) {
            flashProgress();
            bool writeOk = filesystem->writeFile("/data/test2.txt", 
                                       (const uint8_t*)testData, 
                                       strlen(testData));
            Serial.printf("Writing /data/test2.txt: %s\n", writeOk ? "Success" : "Failed");
            success &= writeOk;
        } else {
            success = false;
        }
        
        flashProgress();
        Serial.println("\nMetrics after writing test files:");
        printStorageMetrics();
    }
    
    setLEDStatus(success);  // Final status
}

void loop() {
    if (!nodeState.isInitialized) {
        flashError();
        delay(1000);
        return;
    }
    
    // Main node loop
    heartbeat();  // Heartbeat pattern
    printStorageMetrics();
    delay(10000); // Wait for next heartbeat
} 