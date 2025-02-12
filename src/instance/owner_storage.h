#pragma once

#include "credential.h"
#include <SPIFFS.h>
#include <mbedtls/aes.h>
#include <string>
#include <vector>

namespace one {
namespace instance {

class OwnerStorage {
public:
    // Initialize storage with a verified credential
    bool initialize(const Credential& ownerCredential);

    // Write data to owner's encrypted space
    bool write(const std::string& path, const uint8_t* data, size_t length);

    // Read data from owner's encrypted space
    bool read(const std::string& path, std::vector<uint8_t>& data);

    // Get owner's root directory path
    std::string getOwnerPath() const { return ownerPath_; }

private:
    // Derive encryption key from owner info in credential
    bool deriveKey(const Credential& cred);
    
    // Encrypt/decrypt data
    bool encrypt(const uint8_t* data, size_t length, std::vector<uint8_t>& encrypted);
    bool decrypt(const uint8_t* data, size_t length, std::vector<uint8_t>& decrypted);

    std::string ownerPath_;              // /owner_name_hash/
    uint8_t encryptionKey_[32];         // AES-256 key derived from owner info
    bool initialized_ = false;
};

} // namespace instance
} // namespace one 