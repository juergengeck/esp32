#include "types.h"
#include "peer_manager.h"
#include <Arduino.h>
#include <esp_wifi.h>
#include "message_serializer.h"
#include "config.h"
#include <algorithm>
#include <chrono>

namespace chum {

// Static member for ESP-NOW callback context
static PeerManager* instance_ = nullptr;

void BLECallbacks::onWrite(BLECharacteristic* characteristic) {
    std::string value = characteristic->getValue();
    if (value.length() > 0) {
        manager_->handleBluetoothMessage(
            reinterpret_cast<const uint8_t*>(value.data()),
            value.length()
        );
    }
}

PeerManager::PeerManager(std::shared_ptr<TrustedKeysManager> trustManager)
    : trustManager_(trustManager),
      nextSequence_(0),
      isDiscoveryActive_(false),
      isNetworkConnected_(false),
      security_(std::make_unique<Security>()),
      wsClient_(std::make_unique<WebSocketClient>()) {
    instance_ = this;
    bleCallbacks_ = std::make_unique<BLECallbacks>(this);
}

PeerManager::~PeerManager() {
    shutdown();
    instance_ = nullptr;
}

bool PeerManager::initialize() {
    initializeBluetooth();
    
    // Initialize ESP-NOW
    if (esp_now_init() != ESP_OK) {
        return false;
    }
    
    esp_now_register_recv_cb(handleReceivedData);
    esp_now_register_send_cb(handleSentData);
    
    return true;
}

void PeerManager::initializeBluetooth() {
    BLEDevice::init("");
    
    bleServer_ = std::unique_ptr<BLEServer>(BLEDevice::createServer());
    BLEService* pService = bleServer_->createService(BT_SERVICE_UUID);
    
    bleCharacteristic_ = std::unique_ptr<BLECharacteristic>(pService->createCharacteristic(
        BT_CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_WRITE |
        BLECharacteristic::PROPERTY_NOTIFY
    ));
    
    bleCallbacks_ = std::make_unique<BLECallbacks>(this);
    bleCharacteristic_->setCallbacks(bleCallbacks_.get());
    
    pService->start();
    
    bleAdvertising_ = std::unique_ptr<BLEAdvertising>(BLEDevice::getAdvertising());
    bleAdvertising_->addServiceUUID(BT_SERVICE_UUID);
    bleAdvertising_->setScanResponse(true);
}

bool PeerManager::startDiscovery() {
    if (isDiscoveryActive_) {
        return true;
    }
    
    isDiscoveryActive_ = true;
    startBluetoothAdvertising();
    startBluetoothScan();
    
    return true;
}

void PeerManager::startBluetoothAdvertising() {
    bleAdvertising_->start();
}

void PeerManager::startBluetoothScan() {
    bleScan_ = std::unique_ptr<BLEScan>(BLEDevice::getScan());
    bleScan_->setAdvertisedDeviceCallbacks(this);
    bleScan_->setInterval(1349);
    bleScan_->setWindow(449);
    bleScan_->setActiveScan(true);
    bleScan_->start(BLE_SCAN_TIME, false);
}

bool PeerManager::stopDiscovery() {
    if (!isDiscoveryActive_) {
        return true;
    }
    
    isDiscoveryActive_ = false;
    bleAdvertising_->stop();
    if (bleScan_) {
        bleScan_->stop();
    }
    
    return true;
}

void PeerManager::registerMessageHandler(MessageType type, MessageHandler handler) {
    messageHandlers_[type] = std::move(handler);
}

bool PeerManager::sendMessage(const Message& msg) {
    if (msg.recipient.empty()) {
        return broadcastMessage(msg);
    }
    
    auto recipientPeer = getPeerById(msg.recipient);
    if (!recipientPeer) {
        return false;
    }
    
    if (recipientPeer->hasBluetoothSupport) {
        return sendBluetoothMessage(msg, recipientPeer->bluetoothAddress);
    }
    
    // Send via ESP-NOW
    esp_err_t result = esp_now_send(recipientPeer->espNowInfo.peer_addr, 
                                   reinterpret_cast<const uint8_t*>(&msg), 
                                   sizeof(msg));
    
    return result == ESP_OK;
}

bool PeerManager::broadcastMessage(const Message& msg) {
    bool success = true;
    
    for (const auto& [peerId, peer] : peers_) {
        if (peer.isConnected) {
            Message peerMsg = msg;
            peerMsg.recipient = peerId;
            success &= sendMessage(peerMsg);
        }
    }
    
    return success;
}

void PeerManager::handleBluetoothMessage(const uint8_t* data, size_t length) {
    auto msg = MessageSerializer::deserializeMessage(data, length);
    if (msg) {
        processReceivedMessage(*msg);
    }
}

void PeerManager::handleReceivedData(const uint8_t* mac_addr, const uint8_t* data, int len) {
    auto msg = MessageSerializer::deserializeMessage(data, len);
    if (msg) {
        // Note: This is a static callback, so we need to get the instance somehow
        // One way would be to use a static instance pointer, but that's not ideal
        // For now, we'll just process the message directly
        // TODO: Find a better way to handle this
        // instance->processReceivedMessage(*msg);
    }
}

void PeerManager::handleSentData(const uint8_t* mac, esp_now_send_status_t status) {
    // Handle send status if needed
}

void PeerManager::processReceivedMessage(const Message& msg) {
    if (!validateMessage(msg) || isMessageDuplicate(msg)) {
        return;
    }
    
    updatePeerLastSeen(msg.sender);
    
    auto handler = messageHandlers_.find(msg.type);
    if (handler != messageHandlers_.end()) {
        handler->second(msg);
    }
    
    // Store message ID to prevent duplicates
    recentMessages_.insert(msg.sequence);
    
    // Send acknowledgment
    sendAck(msg);
}

bool PeerManager::validateMessage(const Message& msg) const {
    if (msg.sender.empty()) {
        return false;
    }
    
    auto senderPeer = getPeerById(msg.sender);
    if (!senderPeer || !isPeerTrusted(*senderPeer)) {
        return false;
    }
    
    // Convert string key to vector<uint8_t>
    std::vector<uint8_t> key(senderPeer->keys.front().begin(), senderPeer->keys.front().end());
    return security_->verify(msg.payload.data(), msg.payload.size(), 
                           msg.signature.data(), msg.signature.size(),
                           key);
}

bool PeerManager::isMessageDuplicate(const Message& msg) const {
    return recentMessages_.find(msg.sequence) != recentMessages_.end();
}

uint32_t PeerManager::getNextSequence() {
    return nextSequence_++;
}

void PeerManager::updatePeerLastSeen(const std::string& peerId) {
    auto it = peers_.find(peerId);
    if (it != peers_.end()) {
        it->second.lastSeen = static_cast<uint32_t>(std::chrono::system_clock::now().time_since_epoch().count());
    }
}

bool PeerManager::isPeerTrusted(const PeerInfo& peer) const {
    return trustManager_->isTrusted(peer.id);
}

bool PeerManager::addPeer(const PeerInfo& peer) {
    if (peers_.size() >= MAX_PEERS) {
        return false;
    }
    
    peers_[peer.id] = peer;
    return true;
}

bool PeerManager::removePeer(const std::string& peerId) {
    return peers_.erase(peerId) > 0;
}

void PeerManager::cleanupInactivePeers() {
    auto now = static_cast<uint32_t>(std::chrono::system_clock::now().time_since_epoch().count());
    
    for (auto it = peers_.begin(); it != peers_.end();) {
        if (now - it->second.lastSeen > PEER_TIMEOUT) {
            it = peers_.erase(it);
        } else {
            ++it;
        }
    }
}

void PeerManager::shutdown() {
    stopDiscovery();
    disconnectFromNetwork();
    
    if (bleServer_) {
        bleServer_->getAdvertising()->stop();
    }
    
    esp_now_deinit();
}

std::vector<PeerInfo> PeerManager::getActivePeers() const {
    std::vector<PeerInfo> activePeers;
    activePeers.reserve(peers_.size());
    
    for (const auto& [peerId, peer] : peers_) {
        if (peer.isActive) {
            activePeers.push_back(peer);
        }
    }
    
    return activePeers;
}

std::optional<PeerInfo> PeerManager::getPeerById(const std::string& peerId) const {
    auto it = peers_.find(peerId);
    if (it != peers_.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool PeerManager::sendBluetoothMessage(const Message& msg, const std::string& btAddress) {
    auto serializedMsg = MessageSerializer::serializeMessage(msg);
    
    try {
        BLEClient* pClient = BLEDevice::createClient();
        if (!pClient->connect(BLEAddress(btAddress))) {
            return false;
        }
        
        BLERemoteService* pRemoteService = pClient->getService(BT_SERVICE_UUID);
        if (pRemoteService == nullptr) {
            pClient->disconnect();
            return false;
        }
        
        BLERemoteCharacteristic* pRemoteCharacteristic = pRemoteService->getCharacteristic(BT_CHARACTERISTIC_UUID);
        if (pRemoteCharacteristic == nullptr) {
            pClient->disconnect();
            return false;
        }
        
        pRemoteCharacteristic->writeValue(serializedMsg.data(), serializedMsg.size());
        pClient->disconnect();
        return true;
    } catch (...) {
        return false;
    }
}

bool PeerManager::syncProfileWithPeer(const std::string& peerId) {
    auto profile = trustManager_->getLocalProfile();
    if (!profile) {
        return false;
    }
    
    Message msg;
    msg.type = MessageType::ProfileSync;
    msg.sender = profile->id;
    msg.recipient = peerId;
    msg.sequence = getNextSequence();
    msg.timestamp = static_cast<uint64_t>(std::chrono::system_clock::now().time_since_epoch().count());
    msg.payload = MessageSerializer::serializeProfile(*profile);
    msg.signature = security_->sign(msg.payload.data(), msg.payload.size());
    
    return sendMessage(msg);
}

bool PeerManager::syncCertificatesWithPeer(const std::string& peerId) {
    auto certs = trustManager_->getCertificates();
    if (certs.empty()) {
        return false;
    }
    
    Message msg;
    msg.type = MessageType::CertificateSync;
    msg.sender = trustManager_->getLocalProfile()->id;
    msg.recipient = peerId;
    msg.sequence = getNextSequence();
    msg.timestamp = static_cast<uint64_t>(std::chrono::system_clock::now().time_since_epoch().count());
    msg.payload = MessageSerializer::serializeCertificates(certs);
    msg.signature = security_->sign(msg.payload.data(), msg.payload.size());
    
    return sendMessage(msg);
}

void PeerManager::sendAck(const Message& msg) {
    Message ack;
    ack.type = MessageType::Ack;
    ack.sender = trustManager_->getLocalProfile()->id;
    ack.recipient = msg.sender;
    ack.sequence = getNextSequence();
    ack.timestamp = static_cast<uint64_t>(std::chrono::system_clock::now().time_since_epoch().count());
    
    // Include original message sequence in payload for reference
    std::vector<uint8_t> payload(sizeof(msg.sequence));
    memcpy(payload.data(), &msg.sequence, sizeof(msg.sequence));
    ack.payload = std::move(payload);
    
    ack.signature = security_->sign(ack.payload.data(), ack.payload.size());
    
    sendMessage(ack);
}

bool PeerManager::connectToNetwork(const char* ssid, const char* password, const char* wsUrl) {
    if (wsClient_->connect(ssid, password, wsUrl)) {
        isNetworkConnected_ = true;
        wsClient_->registerMessageHandler([this](const Message& msg) {
            this->handleWebSocketMessage(msg);
        });
        wsClient_->registerConnectionHandler([this](bool connected) {
            this->handleWebSocketConnection(connected);
        });
        return true;
    }
    return false;
}

void PeerManager::disconnectFromNetwork() {
    if (wsClient_) {
        wsClient_->disconnect();
    }
    isNetworkConnected_ = false;
}

bool PeerManager::isNetworkConnected() const {
    return isNetworkConnected_;
}

void PeerManager::handleWebSocketMessage(const Message& msg) {
    processReceivedMessage(msg);
}

void PeerManager::handleWebSocketConnection(bool connected) {
    isNetworkConnected_ = connected;
    if (connected) {
        // Perform any necessary initialization after connection
        // For example, sync with other peers
    }
}

void PeerManager::onResult(BLEAdvertisedDevice advertisedDevice) {
    if (advertisedDevice.haveServiceUUID() && 
        advertisedDevice.isAdvertisingService(BLEUUID(BT_SERVICE_UUID))) {
        // Found a potential peer
        PeerInfo peer;
        peer.id = advertisedDevice.getAddress().toString();
        peer.bluetoothAddress = peer.id;
        peer.lastSeen = static_cast<uint32_t>(std::chrono::system_clock::now().time_since_epoch().count());
        peer.isActive = true;
        peer.hasBluetoothSupport = true;
        
        // Add the peer if we trust it
        if (isPeerTrusted(peer)) {
            addPeer(peer);
        }
    }
}

} // namespace chum 