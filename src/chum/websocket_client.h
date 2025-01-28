#pragma once

#include <memory>
#include <functional>
#include <vector>
#include <cstdint>
#include <Arduino.h>
#include "types.h"

namespace chum {

// Forward declarations
enum class WStype_t : uint8_t;
class WebSocketWrapper;
class Message;
class WebSocketClientImpl;

class WebSocketClient {
public:
    WebSocketClient();
    ~WebSocketClient();

    // Connection management
    bool connect(const char* ssid, const char* password, const char* wsUrl);
    void disconnect();
    bool isConnected() const;

    // Message handling
    bool sendMessage(const Message& msg);
    void registerMessageHandler(std::function<void(const Message&)> handler);

    // Connection status callback
    void registerConnectionHandler(std::function<void(bool)> handler);

    // Update method - must be called regularly
    void update();

private:
    // Private implementation (PIMPL)
    std::unique_ptr<WebSocketClientImpl> impl_;
};

} // namespace chum

// Include implementation headers after declarations
#include "websocket_types.h"
#include "websocket_wrapper.h"
#include "message_serializer.h" 