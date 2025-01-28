#pragma once

#include <Arduino.h>
#include <queue>
#include <memory>
#include <functional>
#include "types.h"
#include "security.h"
#include "websocket_types.h"

// Forward declarations
class WebSocketsServer;
class WebSocketsClient;
enum WStype_t : uint8_t;

namespace chum {

struct InternalMessage {
    MessageType type;
    const uint8_t* data;
    size_t length;
};

class Connection {
public:
    using MessageCallback = std::function<void(const Message&)>;
    using StateCallback = std::function<void(ConnectionState)>;

    Connection();
    ~Connection();

    // Server operations
    bool initServer(uint16_t port = 81);
    void stopServer();

    // Client operations
    bool connectToPeer(const char* host, uint16_t port = 81);
    void disconnect();

    // Message handling
    bool sendMessage(MessageType type, const uint8_t* data, size_t length);
    void setMessageCallback(MessageCallback callback);
    void setStateCallback(StateCallback callback);

    // Connection management
    void update(); // Call in loop to handle WebSocket events
    bool isConnected() const;
    ConnectionState getState() const;

    // Security operations
    bool initSecurity();
    const KeyPair& getKeyPair() const;

private:
    void handleWebSocketEvent(WSEventType type, const uint8_t* payload, size_t length);
    void setState(ConnectionState newState);
    void startHeartbeat();
    void stopHeartbeat();
    void processMessageQueue();
    
    // Security helpers
    bool handleKeyExchange(const uint8_t* data, size_t length);
    bool verifyMessage(const Message& msg);

    struct WebSocketImpl;
    std::unique_ptr<WebSocketImpl> impl_;
    ConnectionState state_;
    uint32_t lastHeartbeat_;
    MessageCallback messageCallback_;
    StateCallback stateCallback_;
    std::unique_ptr<Security> security_;
    bool securityInitialized_;
    std::vector<uint8_t> peerPublicKey_;
    std::queue<InternalMessage> messageQueue_;

    static constexpr uint32_t HEARTBEAT_INTERVAL = 5000; // 5 seconds
};

} // namespace chum 