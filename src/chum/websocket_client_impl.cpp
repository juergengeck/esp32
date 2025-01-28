#include "websocket_client_impl.h"
#include <WiFi.h>
#include <esp_log.h>
#include "config.h"

namespace chum {

static const char* TAG = "WebSocketClientImpl";

WebSocketClientImpl::WebSocketClientImpl() : webSocket_(std::make_unique<WebSocketWrapper>()), isConnected_(false) {}

WebSocketClientImpl::~WebSocketClientImpl() = default;

bool WebSocketClientImpl::connect(const char* ssid, const char* password, const char* wsUrl) {
    if (!connectWiFi(ssid, password)) {
        ESP_LOGE(TAG, "Failed to connect to WiFi");
        return false;
    }

    setupWebSocket(wsUrl);
    return true;
}

void WebSocketClientImpl::disconnect() {
    if (webSocket_) {
        webSocket_->disconnect();
    }
    isConnected_ = false;
    
    // Disconnect WiFi
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
}

bool WebSocketClientImpl::isConnected() const {
    return isConnected_ && webSocket_ && webSocket_->isConnected();
}

bool WebSocketClientImpl::sendMessage(const Message& msg) {
    std::vector<uint8_t> data = MessageSerializer::serializeMessage(msg);
    if (data.empty()) {
        return false;
    }
    return sendBinary(data.data(), data.size());
}

void WebSocketClientImpl::registerMessageHandler(std::function<void(const Message&)> handler) {
    messageHandler_ = std::move(handler);
}

void WebSocketClientImpl::registerConnectionHandler(std::function<void(bool)> handler) {
    connectionHandler_ = std::move(handler);
}

void WebSocketClientImpl::update() {
    // No need to call update explicitly as ESP32 WebSocket client runs in its own task
}

bool WebSocketClientImpl::connectWiFi(const char* ssid, const char* password) {
    ESP_LOGI(TAG, "Connecting to WiFi SSID: %s", ssid);
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    // Wait for connection with timeout
    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - startTime > WIFI_TIMEOUT_MS) {
            ESP_LOGE(TAG, "WiFi connection timeout");
            return false;
        }
        delay(500);
    }

    ESP_LOGI(TAG, "WiFi connected, IP: %s", WiFi.localIP().toString().c_str());
    return true;
}

void WebSocketClientImpl::setupWebSocket(const char* wsUrl) {
    if (!webSocket_) {
        ESP_LOGE(TAG, "WebSocket wrapper not initialized");
        return;
    }

    // Parse URL
    String url(wsUrl);
    String protocol = url.startsWith("wss://") ? "wss://" : "ws://";
    String hostAndPath = url.substring(protocol.length());
    
    // Find the first slash after the host
    int pathPos = hostAndPath.indexOf('/');
    String host = pathPos == -1 ? hostAndPath : hostAndPath.substring(0, pathPos);
    String path = pathPos == -1 ? "/" : hostAndPath.substring(pathPos);
    
    // Default ports
    uint16_t port = protocol == "wss://" ? 443 : 80;
    
    // Check for explicit port
    int colonPos = host.indexOf(':');
    if (colonPos != -1) {
        port = host.substring(colonPos + 1).toInt();
        host = host.substring(0, colonPos);
    }

    ESP_LOGI(TAG, "Connecting to WebSocket - Host: %s, Port: %d, Path: %s", 
             host.c_str(), port, path.c_str());

    // Setup WebSocket event handler
    webSocket_->onEvent([this](WSEventType event, const uint8_t* payload, size_t length) {
        handleWebSocketEvent(event, payload, length);
    });

    // Connect using SSL if wss:// protocol
    if (protocol == "wss://") {
        webSocket_->beginSSL(host.c_str(), port, path.c_str());
    } else {
        webSocket_->begin(host.c_str(), port, path.c_str());
    }
}

void WebSocketClientImpl::handleWebSocketEvent(WSEventType event, const uint8_t* payload, size_t length) {
    switch (event) {
        case WEBSOCKET_EVENT_CONNECTED:
            isConnected_ = true;
            if (connectionHandler_) {
                connectionHandler_(true);
            }
            break;

        case WEBSOCKET_EVENT_DISCONNECTED:
            isConnected_ = false;
            if (connectionHandler_) {
                connectionHandler_(false);
            }
            break;

        case WEBSOCKET_EVENT_DATA:
            if (messageHandler_ && payload && length > 0) {
                Message msg;
                auto maybeMsg = MessageSerializer::deserializeMessage(payload, length);
                if (maybeMsg) {
                    messageHandler_(*maybeMsg);
                } else {
                    ESP_LOGE(TAG, "Failed to deserialize message");
                }
            }
            break;

        case WEBSOCKET_EVENT_ERROR:
            ESP_LOGE(TAG, "WebSocket error occurred");
            break;

        default:
            break;
    }
}

bool WebSocketClientImpl::sendBinary(const uint8_t* data, size_t length) {
    if (!isConnected()) {
        return false;
    }
    return webSocket_->sendBIN(data, length);
}

} // namespace chum 