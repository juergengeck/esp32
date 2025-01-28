#pragma once

#include <functional>
#include <memory>
#include <cstdint>
#include "esp_websocket_client.h"
#include "esp_event.h"
#include "websocket_types.h"

namespace chum {

// Forward declarations
class Message;

class WebSocketWrapper {
public:
    // Event handler type definition
    using EventHandler = std::function<void(WSEventType event, const uint8_t* payload, size_t length)>;

    WebSocketWrapper();
    ~WebSocketWrapper();

    void begin(const char* host, uint16_t port, const char* path);
    void beginSSL(const char* host, uint16_t port, const char* path);
    void disconnect();
    
    bool sendBIN(const uint8_t* payload, size_t length);
    bool sendTXT(const char* payload, size_t length);
    
    void setReconnectInterval(unsigned long interval);
    void enableHeartbeat(uint32_t pingInterval, uint32_t pongTimeout, uint8_t disconnectTimeoutCount);
    
    void onEvent(EventHandler handler);
    bool isConnected() const;

private:
    static void handleWebSocketEvent(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data);
    
    esp_websocket_client_handle_t client_;
    esp_websocket_client_config_t config_;
    EventHandler eventHandler_;
    bool isConnected_;
    char uri_buffer[512];  // Large enough buffer for URIs
};

} // namespace chum 