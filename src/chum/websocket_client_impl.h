#pragma once

#include <memory>
#include <functional>
#include <vector>
#include <cstdint>
#include <Arduino.h>
#include "websocket_types.h"
#include "websocket_wrapper.h"
#include "message_serializer.h"
#include "types.h"

namespace chum {

// Forward declarations
class Message;
class MessageSerializer;

class WebSocketClientImpl {
public:
    WebSocketClientImpl();
    ~WebSocketClientImpl();

    // Connection management
    bool connect(const char* ssid, const char* password, const char* wsUrl);
    void disconnect();
    bool isConnected() const;

    // Message handling
    bool sendMessage(const Message& msg);
    bool sendBinary(const uint8_t* data, size_t length);
    void registerMessageHandler(std::function<void(const Message&)> handler);

    // Connection status callback
    void registerConnectionHandler(std::function<void(bool)> handler);

    // Update method - must be called regularly
    void update();

    static constexpr uint32_t WIFI_TIMEOUT_MS = 30000;  // Increase timeout to 30 seconds

private:
    // WiFi connection
    bool connectWiFi(const char* ssid, const char* password);
    void setupWebSocket(const char* wsUrl);
    void handleWebSocketEvent(WSEventType event, const uint8_t* payload, size_t length);

    // Internal state
    std::unique_ptr<WebSocketWrapper> webSocket_;
    bool isConnected_;
    std::function<void(const Message&)> messageHandler_;
    std::function<void(bool)> connectionHandler_;

    // Constants
    static constexpr uint32_t WEBSOCKET_PING_INTERVAL = 5000;  // 5 seconds
};

} // namespace chum 