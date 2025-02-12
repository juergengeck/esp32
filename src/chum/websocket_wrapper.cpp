#include <WiFiClientSecure.h>
#include "websocket_wrapper.h"
#include <esp_log.h>
#include <string.h>
#include <esp_event.h>
#include <esp_tls.h>

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
    config_.buffer_size = 4096;
    config_.disable_auto_reconnect = false;
    config_.task_prio = 5;  // Higher priority for WebSocket task
    config_.task_stack = 8192;  // 8KB stack should be sufficient
    
    uri_buffer[0] = '\0';

    // Set log levels for debugging - using ESP_LOG_VERBOSE for maximum detail
    esp_log_level_set("*", ESP_LOG_VERBOSE);
    esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
    esp_log_level_set("esp-tls-mbedtls", ESP_LOG_VERBOSE);
    esp_log_level_set("WEBSOCKET", ESP_LOG_VERBOSE);
    esp_log_level_set("WEBSOCKET_CLIENT", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_BASE", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_WS", ESP_LOG_VERBOSE);
    
    ESP_LOGI(TAG, "WebSocket wrapper initialized with configuration:");
    ESP_LOGI(TAG, "- Task priority: %d", config_.task_prio);
    ESP_LOGI(TAG, "- Stack size: %d bytes", config_.task_stack);
    ESP_LOGI(TAG, "- Buffer size: %d bytes", config_.buffer_size);
    ESP_LOGI(TAG, "- Ping interval: %d sec", config_.ping_interval_sec);
    ESP_LOGI(TAG, "- Ping timeout: %d sec", config_.pingpong_timeout_sec);
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
    ESP_LOGI(TAG, "Starting SSL connection to %s:%d%s", host, port, path);
    
    snprintf(uri_buffer, sizeof(uri_buffer), "wss://%s:%d%s", host, port, path);
    ESP_LOGI(TAG, "WebSocket URI: %s", uri_buffer);
    
    // SSL/TLS settings with detailed logging
    ESP_LOGI(TAG, "Configuring SSL/TLS settings...");
    // Use the root CA certificate
    extern const uint8_t root_ca_pem_start[] asm("_binary_src_certs_root_ca_pem_start");
    config_.cert_pem = reinterpret_cast<const char*>(root_ca_pem_start);
    config_.cert_len = strlen(config_.cert_pem);
    config_.client_cert = nullptr;
    config_.client_key = nullptr;
    config_.disable_auto_reconnect = false;
    
    // Task and buffer configuration
    config_.task_stack = 8192;  // 8KB stack
    config_.buffer_size = 4096;  // 4KB buffer
    config_.task_prio = 3;   // Lower than WiFi task but higher than idle
    config_.pingpong_timeout_sec = 10;  // 10 second timeout
    
    ESP_LOGI(TAG, "WebSocket Configuration:");
    ESP_LOGI(TAG, "- Certificate length: %d bytes", config_.cert_len);
    ESP_LOGI(TAG, "- Stack size: %d bytes", config_.task_stack);
    ESP_LOGI(TAG, "- Buffer size: %d bytes", config_.buffer_size);
    ESP_LOGI(TAG, "- Task priority: %d", config_.task_prio);
    ESP_LOGI(TAG, "- Ping/Pong timeout: %d sec", config_.pingpong_timeout_sec);
    ESP_LOGI(TAG, "- Auto reconnect: %s", config_.disable_auto_reconnect ? "disabled" : "enabled");
    
    if (client_) {
        ESP_LOGI(TAG, "Cleaning up existing client connection...");
        esp_websocket_client_destroy(client_);
        client_ = nullptr;
        ESP_LOGI(TAG, "Existing client destroyed");
    }
    
    ESP_LOGI(TAG, "Initializing WebSocket client with SSL...");
    client_ = esp_websocket_client_init(&config_);
    if (!client_) {
        ESP_LOGE(TAG, "Failed to initialize WebSocket client - check memory allocation");
        ESP_LOGE(TAG, "Current free heap: %u bytes", esp_get_free_heap_size());
        return;
    }
    
    ESP_LOGI(TAG, "Client initialized successfully, registering event handler...");
    esp_websocket_register_events(client_, WEBSOCKET_EVENT_ANY, handleWebSocketEvent, this);
    
    ESP_LOGI(TAG, "Starting WebSocket client...");
    esp_err_t err = esp_websocket_client_start(client_);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start client: %s (0x%x)", esp_err_to_name(err), err);
        ESP_LOGE(TAG, "Common issues:");
        ESP_LOGE(TAG, "- ESP_ERR_INVALID_ARG (0x102): Check URI format");
        ESP_LOGE(TAG, "- ESP_ERR_NO_MEM (0x101): Increase stack/buffer size");
        ESP_LOGE(TAG, "- ESP_FAIL (0x1): Check network connection");
        ESP_LOGE(TAG, "Current free heap: %u bytes", esp_get_free_heap_size());
    } else {
        ESP_LOGI(TAG, "Client started successfully");
        ESP_LOGI(TAG, "Current free heap: %u bytes", esp_get_free_heap_size());
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
            ESP_LOGI(TAG, "WEBSOCKET_EVENT_CONNECTED - Connection established successfully");
            wrapper->isConnected_ = true;
            wrapper->eventHandler_(event, nullptr, 0);
            break;
            
        case WEBSOCKET_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "WEBSOCKET_EVENT_DISCONNECTED");
            if (data && data->data_ptr) {
                ESP_LOGI(TAG, "Disconnect reason: %.*s", data->data_len, data->data_ptr);
            }
            wrapper->isConnected_ = false;
            wrapper->eventHandler_(event, nullptr, 0);
            break;
            
        case WEBSOCKET_EVENT_DATA:
            if (data) {
                ESP_LOGI(TAG, "WEBSOCKET_EVENT_DATA:");
                ESP_LOGI(TAG, "- Length: %d bytes", data->data_len);
                ESP_LOGI(TAG, "- Type: %s", data->op_code == 0x01 ? "text" : 
                                          data->op_code == 0x02 ? "binary" : 
                                          data->op_code == 0x08 ? "close" : 
                                          data->op_code == 0x09 ? "ping" : 
                                          data->op_code == 0x0A ? "pong" : "unknown");
                if (data->data_len > 0) {
                    ESP_LOGI(TAG, "- First 32 bytes: ");
                    for (int i = 0; i < std::min(32, data->data_len); i++) {
                        printf("%02x ", (uint8_t)data->data_ptr[i]);
                    }
                    printf("\n");
                }
                wrapper->eventHandler_(event, 
                    reinterpret_cast<const uint8_t*>(data->data_ptr), 
                    data->data_len);
            }
            break;
            
        case WEBSOCKET_EVENT_ERROR:
            ESP_LOGE(TAG, "WEBSOCKET_EVENT_ERROR");
            if (data && data->data_ptr) {
                ESP_LOGE(TAG, "Error message: %.*s", data->data_len, data->data_ptr);
            }
            wrapper->eventHandler_(event, nullptr, 0);
            break;
            
        default:
            ESP_LOGI(TAG, "Unknown WebSocket event: %d", event_id);
            break;
    }
}

} // namespace chum 