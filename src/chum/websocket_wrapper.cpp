#include <WiFiClientSecure.h>
#include "websocket_wrapper.h"
#include <esp_log.h>
#include <string.h>
#include <esp_event.h>
#include <esp_crt_bundle.h>

namespace chum {

static const char* TAG = "WebSocketWrapper";

WebSocketWrapper::WebSocketWrapper() : client_(nullptr), isConnected_(false), eventHandler_(nullptr) {
    // Initialize the event loop if not already done
    esp_event_loop_create_default();
    
    memset(&config_, 0, sizeof(config_));
    config_.transport = WEBSOCKET_TRANSPORT_UNKNOWN;
    config_.user_agent = "esp32-websocket-client";
    config_.path = "/";
    config_.ping_interval_sec = 10;
    config_.pingpong_timeout_sec = 10;
    config_.buffer_size = 1024;
    config_.disable_auto_reconnect = false;
    
    uri_buffer[0] = '\0';
}

WebSocketWrapper::~WebSocketWrapper() {
    if (client_) {
        esp_websocket_client_destroy(client_);
    }
}

void WebSocketWrapper::begin(const char* host, uint16_t port, const char* path) {
    snprintf(uri_buffer, sizeof(uri_buffer), "ws://%s:%d%s", host, port, path);
    
    config_.uri = uri_buffer;
    config_.transport = WEBSOCKET_TRANSPORT_OVER_TCP;
    
    if (client_) {
        esp_websocket_client_destroy(client_);
    }
    
    client_ = esp_websocket_client_init(&config_);
    if (client_) {
        esp_websocket_register_events(client_, WEBSOCKET_EVENT_ANY, handleWebSocketEvent, this);
        esp_websocket_client_start(client_);
    } else {
        ESP_LOGE(TAG, "Failed to initialize WebSocket client");
    }
}

void WebSocketWrapper::beginSSL(const char* host, uint16_t port, const char* path) {
    snprintf(uri_buffer, sizeof(uri_buffer), "wss://%s:%d%s", host, port, path);
    
    config_.uri = uri_buffer;
    config_.transport = WEBSOCKET_TRANSPORT_OVER_SSL;
    
    // SSL/TLS Configuration
    config_.skip_cert_common_name_check = true;
    config_.use_global_ca_store = true;
    
    // Initialize the global CA store
    arduino_esp_crt_bundle_attach(NULL);
    
    if (client_) {
        esp_websocket_client_destroy(client_);
    }
    
    client_ = esp_websocket_client_init(&config_);
    if (client_) {
        esp_websocket_register_events(client_, WEBSOCKET_EVENT_ANY, handleWebSocketEvent, this);
        esp_websocket_client_start(client_);
    } else {
        ESP_LOGE(TAG, "Failed to initialize WebSocket client");
    }
}

void WebSocketWrapper::disconnect() {
    if (client_) {
        esp_websocket_client_stop(client_);
        esp_websocket_client_destroy(client_);
        client_ = nullptr;
    }
    isConnected_ = false;
}

bool WebSocketWrapper::sendBIN(const uint8_t* payload, size_t length) {
    if (!client_ || !isConnected_) return false;
    
    esp_err_t err = esp_websocket_client_send_bin(client_, (const char*)payload, length, portMAX_DELAY);
    return err == ESP_OK;
}

bool WebSocketWrapper::sendTXT(const char* payload, size_t length) {
    if (!client_ || !isConnected_) return false;
    
    esp_err_t err = esp_websocket_client_send_text(client_, payload, length, portMAX_DELAY);
    return err == ESP_OK;
}

void WebSocketWrapper::setReconnectInterval(unsigned long interval) {
    if (client_) {
        // Convert ms to seconds
        config_.ping_interval_sec = interval / 1000;
        esp_websocket_client_stop(client_);
        esp_websocket_client_destroy(client_);
        client_ = esp_websocket_client_init(&config_);
        if (client_) {
            esp_websocket_register_events(client_, WEBSOCKET_EVENT_ANY, handleWebSocketEvent, this);
            esp_websocket_client_start(client_);
        }
    }
}

void WebSocketWrapper::enableHeartbeat(uint32_t pingInterval, uint32_t pongTimeout, uint8_t disconnectTimeoutCount) {
    if (client_) {
        config_.ping_interval_sec = pingInterval;
        config_.pingpong_timeout_sec = pongTimeout;
        // Note: ESP32 WebSocket client handles disconnection internally
    }
}

void WebSocketWrapper::onEvent(EventHandler handler) {
    eventHandler_ = std::move(handler);
}

bool WebSocketWrapper::isConnected() const {
    return isConnected_;
}

void WebSocketWrapper::handleWebSocketEvent(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data) {
    WebSocketWrapper* wrapper = static_cast<WebSocketWrapper*>(handler_args);
    esp_websocket_event_data_t* data = (esp_websocket_event_data_t*)event_data;
    
    if (!wrapper || !wrapper->eventHandler_) return;
    
    WSEventType event = static_cast<WSEventType>(event_id);
    
    switch (event) {
        case WEBSOCKET_EVENT_CONNECTED:
            wrapper->isConnected_ = true;
            wrapper->eventHandler_(event, nullptr, 0);
            break;
            
        case WEBSOCKET_EVENT_DISCONNECTED:
            wrapper->isConnected_ = false;
            wrapper->eventHandler_(event, nullptr, 0);
            break;
            
        case WEBSOCKET_EVENT_DATA:
            if (data) {
                wrapper->eventHandler_(event, 
                    reinterpret_cast<const uint8_t*>(data->data_ptr), 
                    data->data_len);
            }
            break;
            
        case WEBSOCKET_EVENT_ERROR:
            wrapper->eventHandler_(event, nullptr, 0);
            break;
            
        default:
            break;
    }
}

} // namespace chum 