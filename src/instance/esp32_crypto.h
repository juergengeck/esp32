#pragma once

#include <cstdint>
#include <vector>
#include <optional>
#include "crypto_constants.h"

// ESP32 specific includes
#include "esp_system.h"
#include "esp_random.h"
#include "esp_sha.h"

namespace one {
namespace crypto {

class ESP32KeyPair {
public:
    ESP32KeyPair();
    ~ESP32KeyPair() = default;

    // Key generation and management
    bool generate();
    bool importPrivateKey(const std::vector<uint8_t>& privateKey);
    bool importPublicKey(const std::vector<uint8_t>& publicKey);
    std::vector<uint8_t> exportPrivateKey() const;
    std::vector<uint8_t> exportPublicKey() const;

    // Signing and verification
    std::optional<std::vector<uint8_t>> sign(const std::vector<uint8_t>& message) const;
    bool verify(const std::vector<uint8_t>& message, const std::vector<uint8_t>& signature) const;

private:
    // Internal helpers
    bool calculatePublicKey();
    std::vector<uint8_t> sha256(const std::vector<uint8_t>& data) const;

    std::vector<uint8_t> privateKey_;  // 32 bytes for P-256
    std::vector<uint8_t> publicKey_;   // 64 bytes (X,Y) for P-256
    bool hasPrivateKey_;
    bool hasPublicKey_;
};

} // namespace crypto
} // namespace one 