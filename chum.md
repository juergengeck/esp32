# ONE Chum Protocol Implementation Plan

## Overview
The ONE chum protocol is a peer-to-peer communication protocol for ESP32 nodes in the ONE ecosystem, enabling secure object exchange and node discovery.

## Core Components

### 1. Node Identity
- Unique node identifier based on hardware ID and instance hash
- Node capabilities advertisement
- Node state management
- Storage metrics and health status

### 2. Discovery Layer
- UDP broadcast/multicast for local network discovery
- mDNS service advertisement
- Node presence announcements
- Network topology mapping
- Peer list management

### 3. Communication Layer
#### WebSocket Configuration
```cpp
// WebSocket configuration constants
struct WSConfig {
    static constexpr size_t DEFAULT_RX_BUFFER_SIZE = 4096;
    static constexpr size_t DEFAULT_TX_BUFFER_SIZE = 4096;
    static constexpr uint32_t PING_INTERVAL_MS = 5000;
    static constexpr uint32_t PONG_TIMEOUT_MS = 2000;
    static constexpr uint32_t RECONNECT_INTERVAL_MS = 5000;
    static constexpr uint8_t MAX_RECONNECT_ATTEMPTS = 5;
    static constexpr size_t MAX_FRAME_SIZE = 8192;
};

// WebSocket event types for type-safe event handling
enum class WStype_t {
    DISCONNECTED,
    CONNECTED,
    TEXT,
    BIN,
    ERROR,
    FRAGMENT_TEXT_START,
    FRAGMENT_BIN_START,
    FRAGMENT,
    FRAGMENT_FIN,
    PING,
    PONG
};

// WebSocket client wrapper for reliable communication
class WebSocketClient {
public:
    // Connection management
    bool connect(const char* ssid, const char* password, const char* wsUrl);
    void disconnect();
    bool isConnected() const;

    // Message handling with automatic retries
    bool sendMessage(const Message& msg);
    void registerMessageHandler(std::function<void(const Message&)> handler);
    void registerConnectionHandler(std::function<void(bool)> handler);

    // Must be called regularly to handle events and retries
    void update();

private:
    // Message queue for reliability
    struct QueuedMessage {
        Message msg;
        uint32_t retryCount;
        uint32_t lastTryTime;
    };
    std::vector<QueuedMessage> messageQueue_;

    // Constants
    static constexpr uint32_t WIFI_TIMEOUT_MS = 10000;      // 10 seconds
    static constexpr uint32_t WEBSOCKET_PING_INTERVAL = 5000; // 5 seconds
    static constexpr uint32_t MESSAGE_RETRY_INTERVAL = 1000;  // 1 second
    static constexpr uint32_t MAX_RETRY_COUNT = 3;
};
```

### Memory Management
1. Buffer Configuration:
   ```cpp
   // In platformio.ini
   build_flags =
       -DWS_RX_BUFFER_SIZE=4096
       -DWS_TX_BUFFER_SIZE=4096
       -DWS_MAX_QUEUED_MESSAGES=50
   ```

2. Memory Optimization:
   - Use of circular buffers for message queuing
   - Fragmentation of large messages
   - PSRAM utilization when available
   - Stack-based allocation for temporary buffers

3. Resource Cleanup:
   ```cpp
   class ResourceGuard {
       std::function<void()> cleanup_;
   public:
       ResourceGuard(std::function<void()> cleanup) : cleanup_(cleanup) {}
       ~ResourceGuard() { if(cleanup_) cleanup_(); }
   };
   ```

### Error Handling and Recovery
1. Connection Issues:
   ```cpp
   enum class WSError {
       NONE,
       CONNECTION_FAILED,
       AUTHENTICATION_FAILED,
       TIMEOUT,
       BUFFER_OVERFLOW,
       PROTOCOL_ERROR
   };
   ```

2. Recovery Mechanisms:
   - Automatic reconnection with exponential backoff
   - Message queue persistence
   - Session state recovery
   - Connection health monitoring

## Implementation Phases

### Phase 1: WebSocket Communication Layer ✓
- Reliable message delivery
- Automatic reconnection
- Buffer management
- Error handling

### Phase 2: Node Identity & Discovery (In Progress)
- Unique node identification
- Network presence management
- Peer discovery
- Service advertisement

### Phase 3: Object Exchange Protocol (Pending)
- Object synchronization
- Chunked transfer
- Progress tracking
- Integrity verification

### Phase 4: Security Implementation (Pending)
- TLS/SSL integration
- Key exchange
- Message encryption
- Trust verification

## Troubleshooting Guide

### Common Issues

1. Connection Problems
   - Symptom: Frequent disconnections
   - Check: Network stability, buffer sizes
   - Solution: Adjust reconnection parameters, increase buffer sizes

2. Memory Issues
   - Symptom: Heap fragmentation, crashes
   - Check: Memory usage patterns, buffer configurations
   - Solution: Enable PSRAM, optimize buffer sizes

3. Message Delivery
   - Symptom: Lost or duplicate messages
   - Check: Queue size, retry configuration
   - Solution: Adjust retry parameters, implement deduplication

4. Performance
   - Symptom: Slow message processing
   - Check: Buffer sizes, message fragmentation
   - Solution: Optimize message size, adjust buffer configuration

### Debugging Tools

1. Memory Analysis
   ```cpp
   void printMemoryStats() {
       Serial.printf("Free heap: %d\n", ESP.getFreeHeap());
       Serial.printf("Max alloc heap: %d\n", ESP.getMaxAllocHeap());
       Serial.printf("PSRAM: %d\n", ESP.getPsramSize());
   }
   ```

2. Connection Monitoring
   ```cpp
   void monitorConnection() {
       Serial.printf("WiFi RSSI: %d\n", WiFi.RSSI());
       Serial.printf("Connection state: %d\n", client.getState());
       Serial.printf("Queued messages: %d\n", client.queueSize());
   }
   ```

3. Performance Metrics
   ```cpp
   struct Metrics {
       uint32_t messagesSent;
       uint32_t messagesReceived;
       uint32_t retries;
       uint32_t reconnections;
       uint32_t avgProcessingTime;
   };
   ```

## Testing Strategy

### Unit Tests
1. WebSocket Layer Tests
   - Event handling
   - Message formatting
   - Connection management
   - Buffer handling
   - Retry mechanism

2. Protocol Tests
   - Message types
   - Binary formatting
   - Validation
   - Error handling
   - Version compatibility

### Integration Tests
1. End-to-End Tests
   - Connection establishment
   - Message exchange
   - Error recovery
   - Performance metrics

2. Load Tests
   - Multiple connections
   - Large messages
   - Network stress
   - Memory usage

## Next Steps
1. Complete Node Identity implementation
2. Implement secure message exchange
3. Optimize memory usage
4. Add comprehensive monitoring
5. Implement full recovery mechanisms 