#pragma once

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLEClient.h>
#include <BLEScan.h>
#include <BLE2902.h>
#include "instance.h"

namespace one {
namespace instance {

class BLEDiscovery {
public:
    static BLEDiscovery& getInstance() {
        static BLEDiscovery instance;
        return instance;
    }
    
    bool initialize();
    void startAdvertising();
    void stopAdvertising();
    
    // Add new methods for device-to-device communication
    void startScanning();
    void stopScanning();
    bool connectToDevice(const String& deviceName);
    bool sendMessage(const String& message);
    void disconnect();
    bool isConnectedToDevice() const { return connectedClient_ != nullptr; }
    
private:
    BLEDiscovery() = default;
    ~BLEDiscovery() = default;
    BLEDiscovery(const BLEDiscovery&) = delete;
    BLEDiscovery& operator=(const BLEDiscovery&) = delete;
    
    // Server components
    BLEServer* server_ = nullptr;
    BLEService* service_ = nullptr;
    BLEAdvertising* advertising_ = nullptr;
    BLECharacteristic* notifyChar_ = nullptr;  // Store notify characteristic
    
    // Client components
    BLEClient* connectedClient_ = nullptr;
    BLERemoteCharacteristic* remoteCommandChar_ = nullptr;
    BLERemoteCharacteristic* remoteNotifyChar_ = nullptr;
    BLEScan* scanner_ = nullptr;
    
    bool isInitialized_ = false;
    bool deviceConnected_ = false;
    
    // ONE service and characteristic UUIDs
    static const char* SERVICE_UUID;
    static const char* INSTANCE_ID_UUID;
    static const char* DID_UUID;
    static const char* NAME_UUID;
    static const char* COMMAND_UUID;
    static const char* NOTIFY_UUID;

    // Add callback class for handling commands
    class CommandCallbacks: public BLECharacteristicCallbacks {
        void onWrite(BLECharacteristic* pCharacteristic) override;
    private:
        BLEDiscovery* discovery_;
    };

    // Add callback class for server events
    class ServerCallbacks: public BLEServerCallbacks {
        void onConnect(BLEServer* pServer) override;
        void onDisconnect(BLEServer* pServer) override;
    };

    // Add new callback for notifications from other devices
    class NotifyCallbacks: public BLERemoteCharacteristicCallbacks {
        void onNotify(BLERemoteCharacteristic* pChar, uint8_t* data, size_t length) override;
    };

    // Add scan callback
    class ScanCallbacks: public BLEAdvertisedDeviceCallbacks {
        void onResult(BLEAdvertisedDevice advertisedDevice) override;
    };

    CommandCallbacks commandCallbacks_;
    ServerCallbacks serverCallbacks_;
    NotifyCallbacks notifyCallbacks_;
    ScanCallbacks scanCallbacks_;
    
    // Helper methods
    bool setupScanner();
    bool setupClient();
};

} // namespace instance
} // namespace one 