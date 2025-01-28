#include "websocket_client.h"
#include "websocket_client_impl.h"
#include <WiFi.h>
#include <Arduino.h>
#include "websocket_types.h"

// Constants
#define WIFI_TIMEOUT_MS 10000
#define WEBSOCKET_PING_INTERVAL 25000

namespace chum {

WebSocketClient::WebSocketClient() : impl_(std::make_unique<WebSocketClientImpl>()) {}

WebSocketClient::~WebSocketClient() = default;

bool WebSocketClient::connect(const char* ssid, const char* password, const char* wsUrl) {
    if (!impl_) return false;
    return impl_->connect(ssid, password, wsUrl);
}

void WebSocketClient::disconnect() {
    if (impl_) impl_->disconnect();
}

bool WebSocketClient::isConnected() const {
    return impl_ && impl_->isConnected();
}

bool WebSocketClient::sendMessage(const Message& msg) {
    if (!impl_) return false;
    return impl_->sendMessage(msg);
}

void WebSocketClient::registerMessageHandler(std::function<void(const Message&)> handler) {
    if (impl_) impl_->registerMessageHandler(std::move(handler));
}

void WebSocketClient::registerConnectionHandler(std::function<void(bool)> handler) {
    if (impl_) impl_->registerConnectionHandler(std::move(handler));
}

void WebSocketClient::update() {
    if (impl_) impl_->update();
}

} // namespace chum 