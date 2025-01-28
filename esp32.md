# ESP32 ONE Node Implementation Plan

## Overview
Implementation of a basic ONE node on ESP32 microcontroller, serving as an IoT endpoint in the ONE ecosystem.

## Development Setup
### Dependencies
- ../one.core - Core ONE functionality reference
- ../one.models - ONE data models reference
- ../one.baypass.replicant - ONE replication reference

### Build Configuration
- Babel configuration for @refinio package addressing:
  ```json
  {
    "plugins": [
      ["module-resolver", {
        "root": ["./src"],
        "alias": {
          "@refinio/one.core": "../one.core",
          "@refinio/one.models": "../one.models",
          "@refinio/one.baypass.replicant": "../one.baypass.replicant"
        }
      }]
    ]
  }
  ```

## Goals
1. Create a minimal ONE node implementation
2. Demonstrate basic ONE principles
3. Establish foundation for future expansion

## Implementation Phases

### Phase 0: ONE Object System
- Create C library (libonecore) for ONE data types
- Implement core object structures and operations
- Port essential type definitions from one.core
- Implement HTML+Microdata serialization
- Leverage ESP32 crypto hardware for object hashing

### Phase 1: Basic Infrastructure
- Set up ESP32 with necessary communications capabilities
- Implement basic storage handling
- Establish basic node identity
- Reference and adapt patterns from one.core where applicable
- Integrate with libonecore

### Phase 2: ONE Core Concepts
- Implement basic HTML+Microdata object storage
- Create simple transaction handling
- Implement basic Story and Plan objects
- Set up hash-based object referencing

### Phase 3: Communication
- Implement node discovery
- Set up basic peer communication
- Enable data synchronization capabilities
- Bridge interface to Node.js ONE implementation

### Phase 4: Integration
- Connect with other ONE nodes
- Implement basic Action handling
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

3. Buffer Management:
   - WebSocket buffer size: 4KB default, configurable
   - JSON document fragmentation: 2KB chunks
   - File system buffer: 512 bytes per operation
   - Stream buffer sizes: 256 bytes to 1KB based on operation

### Hardware Capabilities
1. Crypto Acceleration:
   - SHA-256 hardware acceleration
   - AES-128/192/256
   - RSA up to 4096 bits
   - Hardware Random Number Generator

2. Network:
   - WiFi 802.11 b/g/n
   - Bluetooth 4.2
   - Dual-core for network processing

3. Storage:
   - SPI Flash: 4MB minimum recommended
   - Optional SD card support
   - SPIFFS/LittleFS file systems

## Build Verification

### Pre-build Checks
1. Memory Configuration:
   ```ini
   build_flags =
       -DCONFIG_SPIRAM_SUPPORT
       -DCONFIG_SPIRAM_USE_MALLOC
       -DCONFIG_SPIRAM_TYPE_AUTO
       -DCONFIG_SPIRAM_SIZE=-1
   ```

2. Partition Table:
   - Minimum 1MB for app
   - 1MB for SPIFFS
   - 32KB for NVS

### Build Steps
1. Clean Build:
   ```bash
   pio run --target clean
   pio run
   ```

2. Memory Analysis:
   ```bash
   pio run --target size
   ```

3. Stack Analysis:
   ```bash
   pio check --pattern="src/*"
   ```

### Common Build Issues
1. Memory Overflow:
   - Symptom: Stack overflow or heap fragmentation
   - Solution: Enable PSRAM or adjust buffer sizes

2. Flash Size:
   - Symptom: "Flash full" error
   - Solution: Optimize partition table or reduce program size

3. Compilation Errors:
   - Symptom: C++ version mismatch
   - Solution: Ensure -std=gnu++17 flag is set

4. Link Errors:
   - Symptom: Undefined references
   - Solution: Check library dependencies and versions

## Initial Focus
1. Memory optimization for object system
2. Efficient HTML+Microdata handling
3. Secure communication implementation
4. Storage system optimization

## Questions to Address
1. Storage strategy for HTML+Microdata objects
2. Communication protocol details
3. Memory management approach
4. Security implementation
5. Adaptation strategy for ONE patterns to embedded environment
6. C library architecture and interface design
7. Bridge protocol between ESP32 and Node.js implementation 