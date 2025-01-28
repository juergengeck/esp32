#pragma once

#include <cstdint>
#include "esp_websocket_client.h"
#include "esp_event.h"

namespace chum {

// Forward declarations
class WebSocketWrapper;

// We'll use ESP32's native WebSocket event types directly
using WSEventType = esp_websocket_event_id_t;
using WSOpCode = esp_websocket_transport_t;

} // namespace chum 