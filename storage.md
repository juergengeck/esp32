# ONE ESP32 Storage Implementation

## Overview
This document outlines the implementation of ONE's storage system on ESP32, adapting the filesystem abstraction from one.core to work with ESP32's SPIFFS filesystem. The implementation follows a layered architecture that provides increasing levels of abstraction and functionality.

## Architecture

### Layer 1: Storage Base Layer
The foundation layer that provides direct SPIFFS operations and basic file management.

#### Key Components:
- Direct SPIFFS operations
- File/directory management
- Error handling
- Path normalization

#### Implementation:
```cpp
namespace one::storage {
    // Core functions
    bool initStorage(const char* instanceIdHash, bool wipeStorage = false);
    StorageResult readUTF8TextFile(const char* filename, String& content);
    StorageResult writeUTF8TextFile(const char* filename, const char* content);
    bool exists(const char* filename);
    size_t fileSize(const char* filename);
}
```

### Layer 2: Storage Streams Layer
Provides streaming abstractions for efficient file operations, especially for large files.

#### Key Components:
- Read/Write stream implementations
- Chunked file operations
- Event-based data handling
- Buffer management

#### Implementation:
```cpp
namespace one::storage {
    class SimpleReadStream {
        virtual size_t read(uint8_t* buffer, size_t length) = 0;
        virtual void close() = 0;
        virtual bool isOpen() const = 0;
        virtual void onData(StreamEventHandler handler);
    };

    class SimpleWriteStream {
        virtual size_t write(const uint8_t* buffer, size_t length) = 0;
        virtual void close() = 0;
        virtual bool isOpen() const = 0;
        virtual void flush() = 0;
        virtual void onDrain(std::function<void()> handler);
    };
}
```

### Layer 3: Filesystem Interface
High-level filesystem abstraction that provides a complete filesystem interface.

#### Key Components:
- Full filesystem operations
- Directory structure management
- File metadata handling
- Space management

#### Implementation:
```cpp
namespace one::storage {
    class IFileSystem {
        // Directory operations
        virtual bool createDir(const char* path, uint16_t mode = 0755) = 0;
        virtual std::vector<DirectoryEntry> readDir(const char* path) = 0;
        
        // File operations
        virtual FileContent readFile(const char* path) = 0;
        virtual bool writeFile(const char* path, const uint8_t* data, size_t length) = 0;
        
        // Space information
        virtual size_t totalSpace() const = 0;
        virtual size_t usedSpace() const = 0;
        virtual size_t freeSpace() const = 0;
    };
}
```

## Integration

### Layer Interaction
The three layers work together seamlessly:
1. Base Layer provides core file operations
2. Streams Layer builds on base operations to provide efficient streaming
3. Filesystem Layer uses both lower layers to implement high-level functionality

### Cross-Layer Operations
All layers can interoperate:
- Files written by one layer can be read by others
- Directory structures are consistent across layers
- Streams can be created for any file regardless of how it was written

### Example Usage:
```cpp
// Initialize storage
initStorage("instance_1", false);

// Create filesystem instance
auto fs = std::make_unique<ESPFileSystem>();

// Write file using base layer
writeUTF8TextFile("data.txt", "Hello ONE!");

// Read using streams
auto stream = streams::createFileReadStream("data.txt");
uint8_t buffer[64];
stream->read(buffer, sizeof(buffer));

// Access via filesystem
fs->createDir("/data");
fs->writeFile("/data/config.txt", buffer, strlen((char*)buffer));
```

## Memory Management

### Buffer Handling
- Fixed-size buffers for predictable memory usage
- Smart pointers for automatic resource cleanup
- Chunked operations for large files
- Stream-based operations to minimize memory footprint

### SPIFFS Constraints
- Maximum filename length: 32 characters
- Maximum file size: Limited by available flash
- No true directory support (emulated)
- No file permissions (emulated)
- No symbolic links

## Error Handling

### Error Categories
- File not found
- Permission denied
- Storage full
- I/O errors
- Memory errors

### Error Reporting
```cpp
struct StorageResult {
    bool success;
    StorageError error;
    String message;
};
```

## Testing

### Unit Tests
Each layer has dedicated unit tests:
- `test_storage_base.cpp`: Base layer tests
- `test_storage_streams.cpp`: Stream layer tests
- `test_filesystem.cpp`: Filesystem layer tests

### Integration Tests
`test_storage_integration.cpp` verifies:
- Cross-layer operations
- Directory hierarchy management
- File consistency across layers
- Resource cleanup

## Performance Considerations

### Optimizations
1. Chunked operations for large files
2. Event-based streaming for efficient I/O
3. Directory caching for faster lookups
4. Minimal memory allocation
5. Buffer reuse where possible

### Benchmarks
Typical performance metrics:
- Small file (<1KB) read/write: <10ms
- Large file (>64KB) streaming: ~1MB/s
- Directory listing: <5ms per 10 entries
- Space calculation: <1ms

## Next Steps
1. Implement file change monitoring
2. Add optional file compression
3. Implement file indexing for faster searches
4. Add support for file attributes
5. Implement file caching system 