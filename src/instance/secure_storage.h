#pragma once

#include <mbedtls/gcm.h>
#include <esp_efuse.h>
#include <SPIFFS.h>
#include <string>
#include <vector>

namespace one {
namespace instance {

class SecureStorage {
public:
    SecureStorage();
    ~SecureStorage();

    // Initialize secure storage with device-specific key
    bool initialize();

    // Encrypt and save data to file
    bool saveEncrypted(const char* path, const uint8_t* data, size_t length);

    // Load and decrypt data from file
    bool loadEncrypted(const char* path, std::vector<uint8_t>& data);

private:
    static constexpr size_t KEY_SIZE = 32;  // AES-256
    static constexpr size_t IV_SIZE = 12;   // GCM recommended IV size
    static constexpr size_t TAG_SIZE = 16;  // Authentication tag size

    // Generate a device-specific key using hardware features
    bool generateDeviceKey();
    
    // Derive encryption key from device-specific features
    bool deriveKey();

    uint8_t key_[KEY_SIZE];
    mbedtls_gcm_context aes_;
    bool initialized_;
};

} // namespace instance
} // namespace one 