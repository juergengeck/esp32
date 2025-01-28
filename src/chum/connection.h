#pragma once

#include <WebSocketsClient.h>
#include <queue>
#include <memory>
#include "types.h"

namespace chum {

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

private:
    void handleWebSocketEvent(WStype_t type, uint8_t* payload, size_t length);
    void setState(ConnectionState newState);
    void startHeartbeat();
    void stopHeartbeat();
    void processMessageQueue();

    WebSocketsServer* server_;
    WebSocketsClient* client_;
    ConnectionState state_;
    uint32_t lastHeartbeat_;
    MessageCallback messageCallback_;
    StateCallback stateCallback_;
    std::queue<Message> messageQueue_;
    static constexpr uint32_t HEARTBEAT_INTERVAL = 10000; // 10 seconds
    static constexpr size_t MAX_QUEUE_SIZE = 100;
};

} // namespace chum 