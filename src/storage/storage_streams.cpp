#include "storage_streams.h"
#include <SPIFFS.h>

namespace one {
namespace storage {

namespace {

/**
 * SPIFFS-based file read stream implementation
 */
class SPIFFSReadStream : public SimpleReadStream {
public:
    explicit SPIFFSReadStream(const char* filename) {
        file_ = SPIFFS.open(filename, "r");
    }

    ~SPIFFSReadStream() override {
        close();
    }

    size_t read(uint8_t* buffer, size_t length) override {
        if (!isOpen()) return 0;
        
        size_t bytesRead = file_.read(buffer, length);
        if (bytesRead > 0 && dataHandler_) {
            DataEvent event(buffer, bytesRead);
            event.emit(dataHandler_);
        }
        return bytesRead;
    }

    void close() override {
        if (file_) {
            file_.close();
        }
    }

    bool isOpen() const override {
        return file_ && file_.size() > 0;
    }

protected:
    ::fs::File file_;
    StreamEventHandler dataHandler_;
};

/**
 * SPIFFS-based file write stream implementation
 */
class SPIFFSWriteStream : public SimpleWriteStream {
public:
    explicit SPIFFSWriteStream(const char* filename) {
        file_ = SPIFFS.open(filename, "w");
    }

    ~SPIFFSWriteStream() override {
        close();
    }

    size_t write(const uint8_t* buffer, size_t length) override {
        if (!isOpen()) return 0;
        
        size_t written = file_.write(buffer, length);
        if (written == length && drainHandler_) {
            drainHandler_();
        }
        return written;
    }

    void close() override {
        if (file_) {
            flush();
            file_.close();
        }
    }

    bool isOpen() const override {
        return file_;
    }

    void flush() override {
        if (file_) {
            file_.flush();
        }
    }

private:
    ::fs::File file_;
};

/**
 * Chunked read stream implementation
 */
class ChunkedReadStream : public SimpleReadStream {
public:
    ChunkedReadStream(const char* filename, size_t chunkSize = 1024)
        : chunkSize_(chunkSize) {
        file_ = SPIFFS.open(filename, "r");
    }

    ~ChunkedReadStream() override {
        close();
    }

    size_t read(uint8_t* buffer, size_t length) override {
        if (!isOpen()) return 0;
        
        size_t remaining = file_.size() - file_.position();
        size_t toRead = std::min(std::min(length, chunkSize_), remaining);
        
        size_t bytesRead = file_.read(buffer, toRead);
        if (bytesRead > 0 && dataHandler_) {
            DataEvent event(buffer, bytesRead);
            event.emit(dataHandler_);
        }
        return bytesRead;
    }

    void close() override {
        if (file_) {
            file_.close();
        }
    }

    bool isOpen() const override {
        return file_ && file_.size() > 0;
    }

protected:
    ::fs::File file_;
    size_t chunkSize_;
    StreamEventHandler dataHandler_;
};

} // anonymous namespace

namespace streams {

SimpleReadStream* createFileReadStream(const char* filename) {
    return new SPIFFSReadStream(filename);
}

SimpleWriteStream* createFileWriteStream(const char* filename) {
    if (!filename) return nullptr;
    return new SPIFFSWriteStream(filename);
}

SimpleWriteStream* createFileWriteStream(const char* filename, const char* encoding) {
    // For now, we ignore encoding as we're dealing with raw bytes
    // In the future, we could add encoding conversion here
    return createFileWriteStream(filename);
}

SimpleReadStream* createChunkedReadStream(const char* filename, size_t chunkSize) {
    if (!filename) return nullptr;
    
    String path = normalizeFilename(filename);
    return new ChunkedReadStream(path.c_str(), chunkSize);
}

} // namespace streams

} // namespace storage
} // namespace one 