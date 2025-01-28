#pragma once

#include "types.h"
#include <vector>
#include <map>
#include <set>
#include <memory>
#include <string>
#include <functional>
#include <optional>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEClient.h>
#include <BLEAdvertising.h>
#include <BLEScan.h>
#include <WiFi.h>
#include <esp_now.h>
#include "security.h"
#include "message_serializer.h"
#include "trusted_keys_manager.h"
#include "websocket_client.h"

namespace chum {

// Bluetooth service and characteristic UUIDs
static constexpr char BT_SERVICE_UUID[] = "4fafc201-1fb5-459e-8fcc-c5c9c331914b";
static constexpr char BT_CHARACTERISTIC_UUID[] = "beb5483e-36e1-4688-b7f5-ea07361b26a8";

// Forward declarations
class PeerManager;

class BLECallbacks : public BLECharacteristicCallbacks {
public:
    BLECallbacks(PeerManager* manager) : manager_(manager) {}
    void onWrite(BLECharacteristic* pCharacteristic) override;
private:
    PeerManager* manager_;
};

struct PeerInfo {
    std::string id;
    uint8_t macAddress[6];
    std::vector<std::string> keys;
    uint32_t lastSeen;
    bool isConnected;
    bool hasBluetoothSupport;
    std::string bluetoothAddress;
    esp_now_peer_info_t espNowInfo;
    bool isActive;
};

class PeerManager : public BLEAdvertisedDeviceCallbacks {
public:
    explicit PeerManager(std::shared_ptr<TrustedKeysManager> trustManager);
    ~PeerManager();

    bool initialize();
    bool startDiscovery();
    bool stopDiscovery();
    bool sendMessage(const Message& msg);
    bool broadcastMessage(const Message& msg);
    bool syncProfileWithPeer(const std::string& peerId);
    bool syncCertificatesWithPeer(const std::string& peerId);
    std::vector<PeerInfo> getActivePeers() const;
    std::optional<PeerInfo> getPeerById(const std::string& peerId) const;

    using MessageHandler = std::function<void(const Message&)>;
    void registerMessageHandler(MessageType type, MessageHandler handler);

    void handleBluetoothMessage(const uint8_t* data, size_t length);
    static void handleReceivedData(const uint8_t* mac_addr, const uint8_t* data, int len);
    static void handleSentData(const uint8_t* mac, esp_now_send_status_t status);
    void shutdown();

    // BLEAdvertisedDeviceCallbacks override
    void onResult(BLEAdvertisedDevice advertisedDevice) override;

private:
    void initializeBluetooth();
    void startBluetoothAdvertising();
    void startBluetoothScan();
    bool sendBluetoothMessage(const Message& msg, const std::string& btAddress);
    void processReceivedMessage(const Message& msg);
    bool validateMessage(const Message& msg) const;
    bool isMessageDuplicate(const Message& msg) const;
    uint32_t getNextSequence();
    void updatePeerLastSeen(const std::string& peerId);
    bool isPeerTrusted(const PeerInfo& peer) const;
    bool addPeer(const PeerInfo& peer);
    bool removePeer(const std::string& peerId);
    void cleanupInactivePeers();
    bool connectToNetwork(const char* ssid, const char* password, const char* wsUrl);
    void disconnectFromNetwork();
    bool isNetworkConnected() const;
    void handleWebSocketMessage(const Message& msg);
    void handleWebSocketConnection(bool connected);
    void sendAck(const Message& msg);

    static constexpr uint32_t CLEANUP_INTERVAL = 60000;  // 1 minute
    static constexpr uint32_t PEER_TIMEOUT = 300000;     // 5 minutes
    static constexpr uint32_t DISCOVERY_INTERVAL = 30000; // 30 seconds
    static constexpr uint8_t MAX_PEERS = 20;
    static constexpr uint16_t MAX_MESSAGE_SIZE = 250;
    static constexpr uint32_t BLE_SCAN_TIME = 5;         // 5 seconds

    std::shared_ptr<TrustedKeysManager> trustManager_;
    std::map<std::string, PeerInfo> peers_;
    std::map<MessageType, MessageHandler> messageHandlers_;
    std::set<uint32_t> recentMessages_;
    uint32_t nextSequence_;
    bool isDiscoveryActive_;
    bool isNetworkConnected_;
    std::unique_ptr<Security> security_;

    std::unique_ptr<BLEServer> bleServer_;
    std::unique_ptr<BLEAdvertising> bleAdvertising_;
    std::unique_ptr<BLEScan> bleScan_;
    std::unique_ptr<BLECharacteristic> bleCharacteristic_;
    std::unique_ptr<BLECallbacks> bleCallbacks_;
    std::unique_ptr<WebSocketClient> wsClient_;
};

} // namespace chum 