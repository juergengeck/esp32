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
#### WebSocket Infrastructure
- Promise-based WebSocket client/server implementation
- Binary message support using ArrayBuffer
- Connection state management (NotListening, Starting, Listening, ShuttingDown)
- Heartbeat mechanism (10-second intervals)
- Automatic reconnection handling

#### Message Protocol
```cpp
enum MessageType {
    HELLO,              // Initial connection
    KEY_EXCHANGE,       // Security handshake
    OBJECT_SYNC,        // Object synchronization
    OBJECT_REQUEST,     // Request specific object
    OBJECT_DATA,        // Object data transfer
    HEARTBEAT,          // Connection health check
    STORAGE_METRICS,    // Node storage status
    ERROR              // Error reporting
};
```

#### Connection Management
- Connection pooling and lifecycle management
- Message queuing system
- Error handling and recovery
- Connection state monitoring
- Graceful shutdown procedures

### 4. Object Exchange
- Object synchronization protocol
- Chunked data transfer
- Progress tracking
- Integrity verification
- Bandwidth management

### 5. Security Layer
- TLS for transport security
- Node authentication
- Message encryption using public/private keys
- Key exchange mechanism
- Trust establishment

## Implementation Phases

### Phase 1: Node Identity & Basic Discovery
1. Node Identity Implementation
   ```cpp
   struct NodeIdentity {
       String instanceId;
       String nodeType;
       uint32_t capabilities;
       StorageMetrics storage;
       uint32_t uptime;
       uint8_t status;
   };
   ```

2. Local Network Discovery
   ```cpp
   class NodeDiscovery {
       void startBroadcast();
       void handleDiscoveryMessage();
       void updatePeerList();
       void advertiseMDNS();
   };
   ```

### Phase 2: Communication Infrastructure
1. WebSocket Implementation
   ```cpp
   class ChumConnection {
   private:
       WebSocket webSocket;
       MessageQueue messageQueue;
       ConnectionState state;
       uint32_t lastHeartbeat;
       
   public:
       void initServer();
       void connectToPeer();
       Promise<void> sendMessage(MessageType type, const uint8_t* data, size_t length);
       void handleMessage(const uint8_t* data, size_t length);
       void maintainConnection();
       void startHeartbeat();
       bool isConnected() const;
   };
   ```

2. Message Protocol
   ```cpp
   class MessageHandler {
   private:
       ChumConnection& connection;
       
   public:
       Promise<void> sendHello();
       Promise<void> exchangeKeys();
       Promise<void> syncObjects();
       Promise<void> requestObject(const String& hash);
       Promise<void> sendObject(const String& hash, const uint8_t* data, size_t length);
       void handleIncomingMessage(MessageType type, const uint8_t* data, size_t length);
   };
   ```

### Phase 3: Object Exchange Protocol
1. Object Synchronization
   ```cpp
   class ObjectSync {
       void announceObjects();
       void requestObject();
       void sendObject();
       void receiveObject();
   };
   ```

2. Transfer Management
   ```cpp
   class TransferManager {
       void startTransfer();
       void handleChunk();
       void verifyIntegrity();
       void resumeTransfer();
   };
   ```

### Phase 4: Security Implementation
1. Security Layer
   ```cpp
   class ChumSecurity {
       void initTLS();
       void authenticatePeer();
       void encryptMessage();
       void verifyMessage();
   };
   ```

2. Trust Management
   ```cpp
   class TrustManager {
       void establishTrust();
       void verifyTrust();
       void updateTrustLevel();
   };
   ```

## Testing Strategy

### Unit Tests
1. Node Identity Tests
   - Identity generation
   - Capability reporting
   - State management

2. WebSocket Tests
   - Connection establishment
   - Message serialization/deserialization
   - Heartbeat functionality
   - Reconnection handling
   - Error recovery

3. Communication Tests
   - Message protocol compliance
   - Queue management
   - Connection pool management
   - Binary message handling

4. Object Exchange Tests
   - Sync protocol
   - Transfer reliability
   - Integrity checks

5. Security Tests
   - Authentication
   - Encryption
   - Trust verification

### Integration Tests
1. Multi-Node Tests
   - Node discovery
   - Connection establishment
   - Object synchronization

2. Network Tests
   - Different network conditions
   - Connection recovery
   - Bandwidth adaptation

3. Load Tests
   - Multiple concurrent transfers
   - Large object handling
   - Resource management

## Performance Considerations

### Memory Management
- Connection pool size limits
- Object chunk size optimization
- Buffer management
- Resource cleanup

### Network Optimization
- Message batching
- Compression
- Bandwidth throttling
- Connection pooling

### Power Efficiency
- Sleep mode during inactivity
- Efficient polling intervals
- Batch processing
- Radio power management

## Error Handling

### Network Errors
- Connection loss recovery
- Timeout handling
- Retry mechanisms
- Circuit breaker implementation

### Protocol Errors
- Message validation
- State recovery
- Protocol version mismatch
- Malformed data handling

### Security Errors
- Authentication failures
- Trust violations
- Encryption errors
- Certificate validation

## Next Steps
1. Implement WebSocket infrastructure
2. Set up message protocol
3. Add connection management
4. Implement object synchronization
5. Integrate security layer
6. Comprehensive testing
7. Performance optimization
8. Documentation and examples 