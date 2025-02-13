# ESP32 ONE Node Implementation Plan

## Overview
Implementation of a basic ONE node on ESP32 microcontroller, serving as an IoT endpoint in the ONE ecosystem.

## Storage Optimization Plan

### Current Storage Issues
- Crypto library (1.5MB): Too large, using generic mbedTLS
- Full QUIC stack: Would require significant storage
- Complex BLE implementation: Uses considerable flash space

### Optimization Strategy
1. Crypto Optimization:
   - Replace generic mbedTLS with ESP32 hardware crypto
   - Use built-in SHA-256, AES, RNG capabilities
   - Leverage hardware ECC for ECDSA P256

2. Transport Layer Optimization:
   - Replace QUIC with WebSocket over TLS
   - Use HTTP/2 for data sync
   - Minimize protocol overhead

3. BLE Optimization:
   - Implement minimal discovery
   - Basic secure pairing only
   - Credential exchange only
   - No continuous BLE communication

4. Bridge Protocol Optimization:
   - Simple REST/WebSocket API
   - Compact serialization format
   - Minimal protocol overhead

### Implementation Priority
1. Switch to ESP32 hardware crypto
2. Implement minimal BLE for pairing
3. Add basic WebSocket connectivity
4. Create lightweight bridge protocol

## Core Components

### 1. Identity Structure
- Instance ID (UUID) as device's unique identifier
- DID constructed using Instance ID (`did:one:<instance_id>`)
- Integration with ONE identity system
- Owner's public key for signature verification

### 2. Instance Management
- Instance ID generation and persistence
- DID resolution and verification
- Capability-based access control
- Signature verification using owner's public key
- No private key storage on device (keys managed by owner's app)

### 3. Communication
#### Transport Layer
- QUIC as primary transport protocol
- BLE for local discovery and pairing
- WiFi for main network connectivity

#### Security
- ONE-based authentication using DIDs
- Signature verification of owner commands
- End-to-end encryption using ONE protocols
- Perfect forward secrecy

### 4. Storage System
- Encrypted storage using ONE protocols
- Instance-based isolation
- Public key storage
- Atomic operations support

## Implementation Phases

### Phase 0: Core Identity System
- Generate and manage Instance IDs
- Implement DID construction and resolution
- Create C++ library for ONE data types
- Port essential type definitions from one.core
- Leverage ESP32 crypto hardware for verification

### Phase 1: Basic Infrastructure
- Set up ESP32 with necessary communications
- Implement basic storage handling
- Establish instance identity and DID mapping
- Store owner's public key
- Reference and adapt patterns from one.core

### Phase 2: ONE Core Concepts
- Implement basic object storage
- Create simple transaction handling
- Implement Story and Plan objects
- Set up hash-based object referencing

### Phase 3: Communication
- Implement instance discovery using DIDs
- Set up QUIC transport
- Enable data synchronization
- Bridge interface to Node.js ONE implementation

### Phase 4: Integration
- Connect with other ONE nodes using DIDs
- Implement Action handling
- Enable basic user interaction

## Technical Considerations

### Memory Management
1. ESP32 Memory Constraints:
   - DRAM: ~328KB available
   - IRAM: ~128KB for time-critical code
   - Flash: Up to 4MB for program storage
   - PSRAM: Optional external RAM support

2. Memory Optimization Strategies:
   - Use PROGMEM for constant data
   - Implement chunked processing for large objects
   - Utilize stack allocation for temporary buffers
   - Enable PSRAM if available for large datasets
   - Implement circular buffers for streaming data

### Hardware Capabilities
1. Crypto Acceleration:
   - SHA-256 hardware acceleration for signature verification
   - AES-128/192/256 for storage encryption
   - Hardware Random Number Generator

2. Network:
   - WiFi 802.11 b/g/n
   - Bluetooth 4.2
   - Dual-core for network processing

3. Storage:
   - SPI Flash: 4MB minimum recommended
   - Optional SD card support
   - SPIFFS/LittleFS file systems

## Security Model
1. Key Management:
   - Private keys stored and managed by owner's app
   - Device only stores owner's public key
   - All operations requiring signing done by owner's app
   - Device verifies signatures using stored public key

2. Trust Model:
   - Device trusts owner's public key
   - All commands must be signed by owner
   - Device can be reset/replaced without key management concerns
   - Compromise of device doesn't expose private keys

## Initial Focus
1. Instance ID and DID implementation
2. Owner public key storage and verification
3. Secure communication implementation
4. Storage system optimization

## Questions to Address
1. Instance ID persistence strategy
2. DID resolution and verification approach
3. Communication protocol details
4. Memory management approach
5. Storage encryption strategy
6. Adaptation strategy for ONE patterns
7. C++ library architecture
8. Bridge protocol between ESP32 and Node.js implementation 