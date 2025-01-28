#pragma once

#include <Arduino.h>
#include <vector>
#include <memory>
#include "storage_base.h"
#include "storage_streams.h"

namespace one {
namespace storage {

/**
 * File description structure
 */
struct FileDescription {
    uint16_t mode;      // File mode/permissions
    size_t size;        // File size in bytes
    uint32_t atime;     // Last access time
    uint32_t mtime;     // Last modification time
    uint32_t ctime;     // Creation time
    bool isDirectory;   // Is this a directory?
    bool isSymlink;     // Is this a symlink?
};

/**
 * Directory entry structure
 */
struct DirectoryEntry {
    String name;
    FileDescription desc;
};

/**
 * File content structure
 */
struct FileContent {
    std::unique_ptr<uint8_t[]> data;
    size_t size;
};

/**
 * Abstract filesystem interface
 */
class IFileSystem {
public:
    virtual ~IFileSystem() = default;

    // Directory operations
    virtual bool createDir(const char* path, uint16_t mode = 0755) = 0;
    virtual bool exists(const char* path) = 0;
    virtual FileDescription stat(const char* path) = 0;
    virtual std::vector<DirectoryEntry> readDir(const char* path) = 0;
    virtual bool removeDir(const char* path) = 0;

    // File operations
    virtual FileContent readFile(const char* path) = 0;
    virtual FileContent readFileChunk(const char* path, size_t offset, size_t length) = 0;
    virtual bool writeFile(const char* path, const uint8_t* data, size_t length, uint16_t mode = 0644) = 0;
    virtual bool appendFile(const char* path, const uint8_t* data, size_t length) = 0;
    virtual bool removeFile(const char* path) = 0;
    virtual bool rename(const char* oldPath, const char* newPath) = 0;

    // Symlink operations
    virtual bool createSymlink(const char* target, const char* linkpath) = 0;
    virtual String readSymlink(const char* path) = 0;

    // Stream operations
    virtual std::unique_ptr<SimpleReadStream> createReadStream(const char* path) = 0;
    virtual std::unique_ptr<SimpleWriteStream> createWriteStream(const char* path) = 0;

    // Utility operations
    virtual bool truncate(const char* path, size_t size) = 0;
    virtual bool chmod(const char* path, uint16_t mode) = 0;
    virtual bool touch(const char* path) = 0;

    // Space information
    virtual size_t totalSpace() const = 0;
    virtual size_t usedSpace() const = 0;
    virtual size_t freeSpace() const = 0;
};

/**
 * SPIFFS-based filesystem implementation
 */
class ESPFileSystem : public IFileSystem {
public:
    ESPFileSystem();
    ~ESPFileSystem() override;

    // Directory operations
    bool createDir(const char* path, uint16_t mode = 0755) override;
    bool exists(const char* path) override;
    FileDescription stat(const char* path) override;
    std::vector<DirectoryEntry> readDir(const char* path) override;
    bool removeDir(const char* path) override;

    // File operations
    FileContent readFile(const char* path) override;
    FileContent readFileChunk(const char* path, size_t offset, size_t length) override;
    bool writeFile(const char* path, const uint8_t* data, size_t length, uint16_t mode = 0644) override;
    bool appendFile(const char* path, const uint8_t* data, size_t length) override;
    bool removeFile(const char* path) override;
    bool rename(const char* oldPath, const char* newPath) override;

    // Symlink operations
    bool createSymlink(const char* target, const char* linkpath) override;
    String readSymlink(const char* path) override;

    // Stream operations
    std::unique_ptr<SimpleReadStream> createReadStream(const char* path) override;
    std::unique_ptr<SimpleWriteStream> createWriteStream(const char* path) override;

    // Utility operations
    bool truncate(const char* path, size_t size) override;
    bool chmod(const char* path, uint16_t mode) override;
    bool touch(const char* path) override;

    // Space information
    size_t totalSpace() const override;
    size_t usedSpace() const override;
    size_t freeSpace() const override;

private:
    bool validatePath(const char* path) const;
    String normalizePath(const char* path) const;
    bool isDirectory(const char* path) const;
    void updateFileTime(const char* path, uint32_t time);
};

} // namespace storage
} // namespace one 