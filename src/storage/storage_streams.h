#pragma once

#include <Arduino.h>
#include "storage_base.h"

namespace one {
namespace storage {

// Forward declarations
class SimpleReadStream;
class SimpleWriteStream;

// Stream event handler type
using StreamEventHandler = std::function<void(const uint8_t*, size_t)>;

/**
 * Base class for all stream events
 */
class StreamEvent {
public:
    virtual ~StreamEvent() = default;
    virtual void emit(StreamEventHandler handler) = 0;
};

/**
 * Data event containing actual bytes
 */
class DataEvent : public StreamEvent {
public:
    DataEvent(const uint8_t* data, size_t length) 
        : data_(data), length_(length) {}
    
    void emit(StreamEventHandler handler) override {
        handler(data_, length_);
    }

private:
    const uint8_t* data_;
    size_t length_;
};

/**
 * Abstract read stream interface
 */
class SimpleReadStream {
public:
    virtual ~SimpleReadStream() = default;

    // Core operations
    virtual size_t read(uint8_t* buffer, size_t length) = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;
    
    // Event handling
    virtual void onData(StreamEventHandler handler) {
        dataHandler_ = handler;
    }

protected:
    StreamEventHandler dataHandler_;
};

/**
 * Abstract write stream interface
 */
class SimpleWriteStream {
public:
    virtual ~SimpleWriteStream() = default;

    // Core operations
    virtual size_t write(const uint8_t* buffer, size_t length) = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;
    virtual void flush() = 0;

    // Event handling
    virtual void onDrain(std::function<void()> handler) {
        drainHandler_ = handler;
    }

protected:
    std::function<void()> drainHandler_;
};

/**
 * Stream factory methods
 */
namespace streams {

// Create a read stream for a file identified by its hash
SimpleReadStream* createFileReadStream(const char* hash);

// Create a write stream for a file
SimpleWriteStream* createFileWriteStream(const char* filename);

// Create a write stream with a specific encoding
SimpleWriteStream* createFileWriteStream(const char* filename, const char* encoding);

// Create a chunked read stream
SimpleReadStream* createChunkedReadStream(const char* filename, size_t chunkSize = 4096);

} // namespace streams

} // namespace storage
} // namespace one 