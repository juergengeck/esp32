#pragma once

#include <Arduino.h>
#include <SPIFFS.h>

namespace one {
namespace storage {

// Storage directories enum
enum class StorageDir {
    OBJECTS,    // ONE objects
    TMP,        // Temporary files
    RMAPS,      // Reverse maps
    VMAPS,      // Version maps
    ACACHE,     // Action cache
    PRIVATE     // Private data
};

// Error handling
enum class StorageError {
    NONE,
    FILE_NOT_FOUND,
    PERMISSION_DENIED,
    STORAGE_FULL,
    IO_ERROR,
    MEMORY_ERROR
};

struct StorageResult {
    bool success;
    StorageError error;
    String message;
};

// Core storage functions
bool initStorage(const char* instanceIdHash, bool wipeStorage = false);
void closeStorage();
bool doesStorageExist(const char* instanceIdHash);

// File operations
StorageResult readUTF8TextFile(const char* filename, String& content);
StorageResult writeUTF8TextFile(const char* filename, const char* content);
bool exists(const char* filename);
size_t fileSize(const char* filename);

// Path management
String normalizeFilename(const char* filename, StorageDir type = StorageDir::OBJECTS);
String getStorageDirForFileType(StorageDir type);

// Internal constants and helpers
namespace internal {
    constexpr const char* DEFAULT_BASE_PATH = "/one";
    constexpr size_t MAX_FILENAME_LENGTH = 32;  // SPIFFS filename limit
    constexpr size_t MAX_PATH_LENGTH = 128;     // Total path length limit
    
    bool createStorageDirs();
    bool validatePath(const char* path);
    String joinPath(const char* base, const char* filename);
}

} // namespace storage
} // namespace one 