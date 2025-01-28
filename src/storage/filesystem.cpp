#include "filesystem.h"
#include <SPIFFS.h>
#include <time.h>
#include <memory>

namespace one {
namespace storage {

namespace {
    constexpr size_t MAX_PATH_LENGTH = 32;  // SPIFFS filename limit
    constexpr char PATH_SEPARATOR = '/';
}

ESPFileSystem::ESPFileSystem() {
    // SPIFFS should already be initialized by storage_base
}

ESPFileSystem::~ESPFileSystem() {
    // Nothing to clean up
}

bool ESPFileSystem::createDir(const char* path, uint16_t mode) {
    // SPIFFS doesn't support real directories, but we can create an empty file
    // with a special suffix to mark it as a directory
    if (!validatePath(path)) return false;
    
    String dirPath = normalizePath(path);
    dirPath += "/.dir";
    
    ::fs::File file = SPIFFS.open(dirPath.c_str(), "w");
    if (!file) return false;
    
    file.close();
    return true;
}

bool ESPFileSystem::exists(const char* path) {
    if (!validatePath(path)) return false;
    return SPIFFS.exists(normalizePath(path).c_str());
}

FileDescription ESPFileSystem::stat(const char* path) {
    FileDescription desc = {0};
    if (!validatePath(path)) return desc;

    String normalizedPath = normalizePath(path);
    ::fs::File file = SPIFFS.open(normalizedPath.c_str(), "r");
    if (!file) return desc;

    desc.size = file.size();
    desc.isDirectory = isDirectory(normalizedPath.c_str());
    desc.isSymlink = false;  // SPIFFS doesn't support symlinks
    
    // Set default mode (we don't have real permissions in SPIFFS)
    desc.mode = desc.isDirectory ? 0755 : 0644;
    
    // Get file times (SPIFFS doesn't track these, so we use current time)
    desc.atime = desc.mtime = desc.ctime = time(nullptr);
    
    file.close();
    return desc;
}

std::vector<DirectoryEntry> ESPFileSystem::readDir(const char* path) {
    std::vector<DirectoryEntry> entries;
    if (!validatePath(path)) return entries;

    String dirPath = normalizePath(path);
    ::fs::File root = SPIFFS.open(dirPath.c_str());
    if (!root || !root.isDirectory()) return entries;

    ::fs::File file = root.openNextFile();
    while (file) {
        // Skip special directory marker files
        if (!String(file.name()).endsWith(".dir")) {
            DirectoryEntry entry;
            entry.name = file.name();
            
            // Remove the directory prefix from the name
            if (entry.name.startsWith(dirPath)) {
                entry.name = entry.name.substring(dirPath.length());
                if (entry.name.startsWith("/")) {
                    entry.name = entry.name.substring(1);
                }
            }
            
            // Get file description
            entry.desc = stat(file.name());
            entries.push_back(entry);
        }
        file = root.openNextFile();
    }

    return entries;
}

bool ESPFileSystem::removeDir(const char* path) {
    if (!validatePath(path)) return false;
    
    String dirPath = normalizePath(path);
    
    // Remove all files in directory
    std::vector<DirectoryEntry> entries = readDir(path);
    for (const auto& entry : entries) {
        String fullPath = dirPath + PATH_SEPARATOR + entry.name;
        if (!removeFile(fullPath.c_str())) return false;
    }
    
    // Remove directory marker
    String markerPath = dirPath + "/.dir";
    return SPIFFS.remove(markerPath.c_str());
}

FileContent ESPFileSystem::readFile(const char* path) {
    FileContent content = {0};
    if (!validatePath(path)) return content;

    ::fs::File file = SPIFFS.open(normalizePath(path).c_str(), "r");
    if (!file) return content;

    content.size = file.size();
    content.data = std::unique_ptr<uint8_t[]>(new uint8_t[content.size]);

    if (content.data) {
        file.read(content.data.get(), content.size);
    } else {
        content.size = 0;
    }
    
    file.close();
    return content;
}

FileContent ESPFileSystem::readFileChunk(const char* path, size_t offset, size_t length) {
    FileContent content = {0};
    if (!validatePath(path)) return content;

    ::fs::File file = SPIFFS.open(normalizePath(path).c_str(), "r");
    if (!file) return content;

    if (offset >= file.size()) {
        file.close();
        return content;
    }

    // Adjust length if it would exceed file size
    size_t remainingBytes = file.size() - offset;
    length = std::min(length, remainingBytes);

    content.size = length;
    content.data = std::unique_ptr<uint8_t[]>(new uint8_t[length]);

    file.seek(offset);
    file.read(content.data.get(), length);
    
    file.close();
    return content;
}

bool ESPFileSystem::writeFile(const char* path, const uint8_t* data, size_t length, uint16_t mode) {
    if (!validatePath(path)) return false;

    ::fs::File file = SPIFFS.open(normalizePath(path).c_str(), "w");
    if (!file) return false;

    size_t written = file.write(data, length);
    file.close();

    return written == length;
}

bool ESPFileSystem::appendFile(const char* path, const uint8_t* data, size_t length) {
    if (!validatePath(path)) return false;

    ::fs::File file = SPIFFS.open(normalizePath(path).c_str(), "a");
    if (!file) return false;

    size_t written = file.write(data, length);
    file.close();

    return written == length;
}

bool ESPFileSystem::removeFile(const char* path) {
    if (!validatePath(path)) return false;
    return SPIFFS.remove(normalizePath(path).c_str());
}

bool ESPFileSystem::rename(const char* oldPath, const char* newPath) {
    if (!validatePath(oldPath) || !validatePath(newPath)) return false;
    return SPIFFS.rename(normalizePath(oldPath).c_str(), normalizePath(newPath).c_str());
}

bool ESPFileSystem::createSymlink(const char* target, const char* linkpath) {
    // SPIFFS doesn't support symlinks
    return false;
}

String ESPFileSystem::readSymlink(const char* path) {
    // SPIFFS doesn't support symlinks
    return String();
}

std::unique_ptr<SimpleReadStream> ESPFileSystem::createReadStream(const char* path) {
    if (!validatePath(path)) return nullptr;
    return std::unique_ptr<SimpleReadStream>(streams::createFileReadStream(normalizePath(path).c_str()));
}

std::unique_ptr<SimpleWriteStream> ESPFileSystem::createWriteStream(const char* path) {
    if (!validatePath(path)) return nullptr;
    return std::unique_ptr<SimpleWriteStream>(streams::createFileWriteStream(normalizePath(path).c_str()));
}

bool ESPFileSystem::truncate(const char* path, size_t size) {
    if (!validatePath(path)) return false;

    // SPIFFS doesn't support truncate directly, so we need to recreate the file
    String normalizedPath = normalizePath(path);
    ::fs::File file = SPIFFS.open(normalizedPath.c_str(), "r");
    if (!file) return false;

    // If requested size is larger than current size, pad with zeros
    size_t currentSize = file.size();
    if (size > currentSize) {
        file.close();
        ::fs::File writeFile = SPIFFS.open(normalizedPath.c_str(), "a");
        if (!writeFile) return false;
        
        size_t padding = size - currentSize;
        uint8_t zero = 0;
        while (padding--) {
            writeFile.write(&zero, 1);
        }
        writeFile.close();
        return true;
    }

    // If requested size is smaller, create new file with truncated content
    std::unique_ptr<uint8_t[]> buffer(new uint8_t[size]);
    file.read(buffer.get(), size);
    file.close();

    return writeFile(path, buffer.get(), size);
}

bool ESPFileSystem::chmod(const char* path, uint16_t mode) {
    // SPIFFS doesn't support file permissions
    return exists(path);
}

bool ESPFileSystem::touch(const char* path) {
    if (!validatePath(path)) return false;

    if (!exists(path)) {
        // Create empty file if it doesn't exist
        ::fs::File file = SPIFFS.open(normalizePath(path).c_str(), "w");
        if (!file) return false;
        file.close();
    }

    // Update timestamps
    updateFileTime(path, time(nullptr));
    return true;
}

size_t ESPFileSystem::totalSpace() const {
    return SPIFFS.totalBytes();
}

size_t ESPFileSystem::usedSpace() const {
    return SPIFFS.usedBytes();
}

size_t ESPFileSystem::freeSpace() const {
    return SPIFFS.totalBytes() - SPIFFS.usedBytes();
}

bool ESPFileSystem::validatePath(const char* path) const {
    if (!path) return false;
    size_t len = strlen(path);
    return len > 0 && len <= MAX_PATH_LENGTH;
}

String ESPFileSystem::normalizePath(const char* path) const {
    String normalized = path;
    // Ensure path starts with /
    if (!normalized.startsWith("/")) {
        normalized = "/" + normalized;
    }
    return normalized;
}

bool ESPFileSystem::isDirectory(const char* path) const {
    String dirMarker = String(path) + "/.dir";
    return SPIFFS.exists(dirMarker.c_str());
}

void ESPFileSystem::updateFileTime(const char* path, uint32_t time) {
    // SPIFFS doesn't support file timestamps
    // This is a no-op, but kept for interface compatibility
}

} // namespace storage
} // namespace one 