#include "ble_discovery.h"

namespace one {
namespace instance {

// Define static UUIDs
const char* BLEDiscovery::SERVICE_UUID = "58ef0ba0-b0f7-11eb-8529-0242ac130003";
const char* BLEDiscovery::INSTANCE_ID_UUID = "58ef0ba1-b0f7-11eb-8529-0242ac130003";
const char* BLEDiscovery::DID_UUID = "58ef0ba2-b0f7-11eb-8529-0242ac130003";
const char* BLEDiscovery::NAME_UUID = "58ef0ba3-b0f7-11eb-8529-0242ac130003";
const char* BLEDiscovery::COMMAND_UUID = "58ef0ba4-b0f7-11eb-8529-0242ac130003";
const char* BLEDiscovery::NOTIFY_UUID = "58ef0ba5-b0f7-11eb-8529-0242ac130003";

// Static variables to store found device address
static BLEAddress* foundDeviceAddress = nullptr;

void BLEDiscovery::CommandCallbacks::onWrite(BLECharacteristic* pCharacteristic) {
    std::string value = pCharacteristic->getValue();
    if (value.length() > 0) {
        Serial.println("Received command: " + String(value.c_str()));
        // Send response via notification characteristic
        if (BLEDiscovery::getInstance().notifyChar_) {
            String response = "Received: " + String(value.c_str());
            BLEDiscovery::getInstance().notifyChar_->setValue(response.c_str());
            BLEDiscovery::getInstance().notifyChar_->notify();
        }
    }
}

void BLEDiscovery::NotifyCallbacks::onNotify(BLERemoteCharacteristic* pChar, uint8_t* data, size_t length) {
    String message((char*)data, length);
    Serial.println("Received notification from peer: " + message);
}

void BLEDiscovery::ScanCallbacks::onResult(BLEAdvertisedDevice advertisedDevice) {
    Serial.printf("Found device: %s\n", advertisedDevice.toString().c_str());
    if (advertisedDevice.haveName()) {
        String name = advertisedDevice.getName().c_str();
        // Store the address if we find our target device
        if (name == "esp32" || name == "esp33") {
            if (foundDeviceAddress != nullptr) {
                delete foundDeviceAddress;
            }
            foundDeviceAddress = new BLEAddress(advertisedDevice.getAddress());
            Serial.printf("Found target device: %s\n", name.c_str());
        }
    }
}

void BLEDiscovery::ServerCallbacks::onConnect(BLEServer* pServer) {
    Serial.println("Client connected");
    deviceConnected_ = true;
    pServer->getAdvertising()->stop();
}

void BLEDiscovery::ServerCallbacks::onDisconnect(BLEServer* pServer) {
    Serial.println("Client disconnected");
    deviceConnected_ = false;
    pServer->getAdvertising()->start();
}

bool BLEDiscovery::setupScanner() {
    scanner_ = BLEDevice::getScan();
    scanner_->setAdvertisedDeviceCallbacks(&scanCallbacks_);
    scanner_->setInterval(1349);
    scanner_->setWindow(449);
    scanner_->setActiveScan(true);
    return true;
}

bool BLEDiscovery::setupClient() {
    if (connectedClient_ != nullptr) {
        connectedClient_->disconnect();
        delete connectedClient_;
    }
    connectedClient_ = BLEDevice::createClient();
    return connectedClient_ != nullptr;
}

void BLEDiscovery::startScanning() {
    if (scanner_ == nullptr) {
        setupScanner();
    }
    scanner_->start(5, false); // Scan for 5 seconds
}

void BLEDiscovery::stopScanning() {
    if (scanner_ != nullptr) {
        scanner_->stop();
    }
}

bool BLEDiscovery::connectToDevice(const String& deviceName) {
    if (!setupClient()) {
        return false;
    }

    startScanning();
    delay(5000); // Wait for scan to complete

    if (foundDeviceAddress == nullptr) {
        Serial.printf("Device %s not found\n", deviceName.c_str());
        return false;
    }

    // Connect to the device
    if (!connectedClient_->connect(*foundDeviceAddress)) {
        Serial.println("Connection failed");
        return false;
    }

    // Get the service
    BLERemoteService* remoteService = connectedClient_->getService(SERVICE_UUID);
    if (remoteService == nullptr) {
        Serial.println("Failed to find service");
        connectedClient_->disconnect();
        return false;
    }

    // Get the command characteristic
    remoteCommandChar_ = remoteService->getCharacteristic(COMMAND_UUID);
    if (remoteCommandChar_ == nullptr) {
        Serial.println("Failed to find command characteristic");
        connectedClient_->disconnect();
        return false;
    }

    // Get the notify characteristic
    remoteNotifyChar_ = remoteService->getCharacteristic(NOTIFY_UUID);
    if (remoteNotifyChar_ == nullptr) {
        Serial.println("Failed to find notify characteristic");
        connectedClient_->disconnect();
        return false;
    }

    // Register for notifications
    if (remoteNotifyChar_->canNotify()) {
        remoteNotifyChar_->registerForNotify(&notifyCallbacks_);
    }

    Serial.printf("Connected to %s\n", deviceName.c_str());
    return true;
}

bool BLEDiscovery::sendMessage(const String& message) {
    if (!isConnectedToDevice() || remoteCommandChar_ == nullptr) {
        Serial.println("Not connected to device");
        return false;
    }

    remoteCommandChar_->writeValue(message.c_str(), message.length());
    Serial.println("Message sent: " + message);
    return true;
}

void BLEDiscovery::disconnect() {
    if (connectedClient_ != nullptr) {
        connectedClient_->disconnect();
    }
}

bool BLEDiscovery::initialize() {
    if (isInitialized_) {
        return true;
    }
    
    // Initialize BLE device with instance name
    auto& instance = Instance::getInstance();
    BLEDevice::init(instance.getName().c_str());
    
    // Set maximum power
    BLEDevice::setPower(ESP_PWR_LVL_P9);
    
    // Create BLE server
    server_ = BLEDevice::createServer();
    if (!server_) {
        Serial.println("Failed to create BLE server");
        return false;
    }
    
    // Set server callbacks
    server_->setCallbacks(&serverCallbacks_);
    
    // Create ONE service
    service_ = server_->createService(SERVICE_UUID);
    if (!service_) {
        Serial.println("Failed to create BLE service");
        return false;
    }
    
    // Create characteristics
    BLECharacteristic* instanceIdChar = service_->createCharacteristic(
        INSTANCE_ID_UUID,
        BLECharacteristic::PROPERTY_READ
    );
    instanceIdChar->setValue(instance.getInstanceId().c_str());
    
    BLECharacteristic* didChar = service_->createCharacteristic(
        DID_UUID,
        BLECharacteristic::PROPERTY_READ
    );
    didChar->setValue(instance.getDid().c_str());
    
    BLECharacteristic* nameChar = service_->createCharacteristic(
        NAME_UUID,
        BLECharacteristic::PROPERTY_READ
    );
    nameChar->setValue(instance.getName().c_str());
    
    // Add command characteristic
    BLECharacteristic* commandChar = service_->createCharacteristic(
        COMMAND_UUID,
        BLECharacteristic::PROPERTY_WRITE
    );
    commandChar->setCallbacks(&commandCallbacks_);
    
    // Add notification characteristic
    notifyChar_ = service_->createCharacteristic(
        NOTIFY_UUID,
        BLECharacteristic::PROPERTY_NOTIFY
    );
    notifyChar_->addDescriptor(new BLEDescriptor(BLEUUID((uint16_t)0x2902)));
    
    // Start the service
    service_->start();
    
    // Get advertising object and configure it
    advertising_ = BLEDevice::getAdvertising();
    advertising_->addServiceUUID(SERVICE_UUID);
    advertising_->setScanResponse(true);
    advertising_->setMinPreferred(0x06);  // functions that help with iPhone connections issue
    advertising_->setMinPreferred(0x12);
    
    isInitialized_ = true;
    Serial.println("BLE Discovery service initialized with command interface");
    return true;
}

void BLEDiscovery::startAdvertising() {
    if (!isInitialized_) {
        return;
    }
    
    BLEDevice::startAdvertising();
    Serial.printf("BLE advertising started with power level: %d\n", esp_ble_tx_power_get(ESP_BLE_PWR_TYPE_ADV));
}

void BLEDiscovery::stopAdvertising() {
    if (!isInitialized_) {
        return;
    }
    
    BLEDevice::stopAdvertising();
    Serial.println("BLE advertising stopped");
}

} // namespace instance
} // namespace one 