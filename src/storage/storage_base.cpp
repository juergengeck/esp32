#include "storage_base.h"
#include <SPIFFS.h>

namespace one {
namespace storage {

namespace {
    // Module-level variables
    String gInstancePath;
    String gStorageDirs[6];  // One for each StorageDir enum value
    bool gIsInitialized = false;

    // Convert StorageDir enum to string
    const char* getDirName(StorageDir type) {
        switch(type) {
            case StorageDir::OBJECTS: return "objects";
            case StorageDir::TMP: return "tmp";
            case StorageDir::RMAPS: return "rmaps";
            case StorageDir::VMAPS: return "vmaps";
            case StorageDir::ACACHE: return "acache";
            case StorageDir::PRIVATE: return "private";
            default: return "";
        }
    }
}

bool initStorage(const char* instanceIdHash, bool wipeStorage) {
    if (!instanceIdHash) {
        return false;
    }

    // Initialize SPIFFS if not already done
    if (!SPIFFS.begin(true)) {
        return false;
    }

    // Set up instance path
    gInstancePath = internal::joinPath(internal::DEFAULT_BASE_PATH, instanceIdHash);
    
    // Initialize storage directory paths
    for (int i = 0; i < 6; i++) {
        StorageDir type = static_cast<StorageDir>(i);
        gStorageDirs[i] = internal::joinPath(gInstancePath.c_str(), getDirName(type));
    }

    // Check if instance exists
    bool instanceExists = SPIFFS.exists(gInstancePath.c_str());

    // Handle wipeStorage flag
    if (instanceExists && wipeStorage) {
        // Remove all files in instance directory
        File root = SPIFFS.open(gInstancePath.c_str());
        if (root && root.isDirectory()) {
            File file = root.openNextFile();
            while (file) {
                SPIFFS.remove(file.path());
                file = root.openNextFile();
            }
        }
        instanceExists = false;
    }

    // Create directory structure if needed
    if (!instanceExists || wipeStorage) {
        if (!internal::createStorageDirs()) {
            return false;
        }
    }

    gIsInitialized = true;
    return true;
}

void closeStorage() {
    gInstancePath = "";
    for (int i = 0; i < 6; i++) {
        gStorageDirs[i] = "";
    }
    gIsInitialized = false;
}

bool doesStorageExist(const char* instanceIdHash) {
    if (!instanceIdHash) {
        return false;
    }
    String path = internal::joinPath(internal::DEFAULT_BASE_PATH, instanceIdHash);
    return SPIFFS.exists(path.c_str());
}

StorageResult readUTF8TextFile(const char* filename, String& content) {
    StorageResult result = {false, StorageError::NONE, ""};
    
    if (!gIsInitialized) {
        result.error = StorageError::IO_ERROR;
        result.message = "Storage not initialized";
        return result;
    }

    String path = normalizeFilename(filename);
    if (!SPIFFS.exists(path.c_str())) {
        result.error = StorageError::FILE_NOT_FOUND;
        result.message = "File not found: " + path;
        return result;
    }

    File file = SPIFFS.open(path.c_str(), "r");
    if (!file) {
        result.error = StorageError::IO_ERROR;
        result.message = "Failed to open file: " + path;
        return result;
    }

    content = file.readString();
    file.close();
    
    result.success = true;
    return result;
}

StorageResult writeUTF8TextFile(const char* filename, const char* content) {
    StorageResult result = {false, StorageError::NONE, ""};
    
    if (!gIsInitialized) {
        result.error = StorageError::IO_ERROR;
        result.message = "Storage not initialized";
        return result;
    }

    String path = normalizeFilename(filename);
    File file = SPIFFS.open(path.c_str(), "w");
    if (!file) {
        result.error = StorageError::IO_ERROR;
        result.message = "Failed to create file: " + path;
        return result;
    }

    size_t written = file.print(content);
    file.close();

    if (written == 0) {
        result.error = StorageError::IO_ERROR;
        result.message = "Failed to write to file: " + path;
        return result;
    }

    result.success = true;
    return result;
}

bool exists(const char* filename) {
    if (!gIsInitialized || !filename) {
        return false;
    }
    String path = normalizeFilename(filename);
    return SPIFFS.exists(path.c_str());
}

size_t fileSize(const char* filename) {
    if (!gIsInitialized || !filename) {
        return 0;
    }
    String path = normalizeFilename(filename);
    File file = SPIFFS.open(path.c_str(), "r");
    if (!file) {
        return 0;
    }
    size_t size = file.size();
    file.close();
    return size;
}

String normalizeFilename(const char* filename, StorageDir type) {
    if (!gIsInitialized || !filename) {
        return "";
    }
    return internal::joinPath(gStorageDirs[static_cast<int>(type)].c_str(), filename);
}

String getStorageDirForFileType(StorageDir type) {
    if (!gIsInitialized) {
        return "";
    }
    return gStorageDirs[static_cast<int>(type)];
}

namespace internal {

bool createStorageDirs() {
    // Create base instance directory
    File root = SPIFFS.open(gInstancePath.c_str(), "w");
    if (!root) {
        return false;
    }
    root.close();

    // Create all storage subdirectories
    for (int i = 0; i < 6; i++) {
        File dir = SPIFFS.open(gStorageDirs[i].c_str(), "w");
        if (!dir) {
            return false;
        }
        dir.close();
    }
    return true;
}

bool validatePath(const char* path) {
    if (!path) {
        return false;
    }
    size_t len = strlen(path);
    return len > 0 && len <= MAX_PATH_LENGTH;
}

String joinPath(const char* base, const char* filename) {
    String result = base;
    if (!result.endsWith("/")) {
        result += "/";
    }
    result += filename;
    return result;
}

} // namespace internal

} // namespace storage
} // namespace one 