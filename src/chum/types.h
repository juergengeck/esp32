#pragma once

#include <cstdint>
#include <Arduino.h>

namespace chum {

enum class ConnectionState {
    NotConnected,
    Connecting,
    Connected,
    Disconnecting
};

enum class MessageType : uint8_t {
    HELLO = 0,           // Initial connection
    KEY_EXCHANGE = 1,    // Security handshake
    OBJECT_SYNC = 2,     // Object synchronization
    OBJECT_REQUEST = 3,  // Request specific object
    OBJECT_DATA = 4,     // Object data transfer
    HEARTBEAT = 5,       // Connection health check
    STORAGE_METRICS = 6, // Node storage status
    ERROR = 7           // Error reporting
};

struct Message {
    MessageType type;
    uint16_t length;
    uint8_t* data;
};

struct StorageMetrics {
    uint32_t totalBytes;
    uint32_t usedBytes;
    uint32_t freeBytes;
};

struct NodeIdentity {
    String instanceId;
    String nodeType;
    uint32_t capabilities;
    StorageMetrics storage;
    uint32_t uptime;
    uint8_t status;
};

} // namespace chum 