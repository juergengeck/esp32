#include "esp32_crypto.h"
#include <esp_system.h>
#include <esp_random.h>
#include <esp_sha.h>
#include <esp_ecc.h>

namespace one {
namespace crypto {

ESP32KeyPair::ESP32KeyPair()
    : hasPrivateKey_(false)
    , hasPublicKey_(false) {
}

bool ESP32KeyPair::generate() {
    // Generate private key using ESP32's hardware RNG
    privateKey_.resize(ECDSA_PRIVATE_KEY_LENGTH);
    esp_fill_random(privateKey_.data(), ECDSA_PRIVATE_KEY_LENGTH);
    hasPrivateKey_ = true;

    // Calculate public key
    return calculatePublicKey();
}

bool ESP32KeyPair::importPrivateKey(const std::vector<uint8_t>& privateKey) {
    if (privateKey.size() != ECDSA_PRIVATE_KEY_LENGTH) {
        return false;
    }

    privateKey_ = privateKey;
    hasPrivateKey_ = true;

    // Calculate public key
    return calculatePublicKey();
}

bool ESP32KeyPair::importPublicKey(const std::vector<uint8_t>& publicKey) {
    if (publicKey.size() != ECDSA_PUBLIC_KEY_LENGTH) {
        return false;
    }

    publicKey_ = publicKey;
    hasPublicKey_ = true;
    return true;
}

std::vector<uint8_t> ESP32KeyPair::exportPrivateKey() const {
    if (!hasPrivateKey_) {
        return std::vector<uint8_t>();
    }
    return privateKey_;
}

std::vector<uint8_t> ESP32KeyPair::exportPublicKey() const {
    if (!hasPublicKey_) {
        return std::vector<uint8_t>();
    }
    return publicKey_;
}

std::optional<std::vector<uint8_t>> ESP32KeyPair::sign(const std::vector<uint8_t>& message) const {
    if (!hasPrivateKey_) {
        return std::nullopt;
    }

    // Calculate SHA256 of the message
    auto hash = sha256(message);

    // Sign using ESP32's hardware ECC
    esp_ecc_point_t signature;
    if (esp_ecc_point_sign(ESP_ECC_CURVE_P256, 
                          privateKey_.data(),
                          hash.data(),
                          &signature) != ESP_OK) {
        return std::nullopt;
    }

    // Convert signature to R,S format
    std::vector<uint8_t> result(64);  // R(32) + S(32)
    memcpy(result.data(), signature.x, 32);
    memcpy(result.data() + 32, signature.y, 32);
    return result;
}

bool ESP32KeyPair::verify(const std::vector<uint8_t>& message, 
                         const std::vector<uint8_t>& signature) const {
    if (!hasPublicKey_ || signature.size() != 64) {
        return false;
    }

    // Calculate SHA256 of the message
    auto hash = sha256(message);

    // Convert signature from R,S format
    esp_ecc_point_t sig_point;
    memcpy(sig_point.x, signature.data(), 32);
    memcpy(sig_point.y, signature.data() + 32, 32);

    // Convert public key to point format
    esp_ecc_point_t pub_point;
    memcpy(pub_point.x, publicKey_.data(), 32);
    memcpy(pub_point.y, publicKey_.data() + 32, 32);

    // Verify using ESP32's hardware ECC
    return esp_ecc_point_verify(ESP_ECC_CURVE_P256,
                              &pub_point,
                              hash.data(),
                              &sig_point) == ESP_OK;
}

bool ESP32KeyPair::calculatePublicKey() {
    if (!hasPrivateKey_) {
        return false;
    }

    // Calculate public key using ESP32's hardware ECC
    esp_ecc_point_t pub_point;
    if (esp_ecc_point_multiply(ESP_ECC_CURVE_P256,
                              privateKey_.data(),
                              nullptr,  // Use curve's base point
                              &pub_point) != ESP_OK) {
        return false;
    }

    // Store public key in X,Y format
    publicKey_.resize(64);
    memcpy(publicKey_.data(), pub_point.x, 32);
    memcpy(publicKey_.data() + 32, pub_point.y, 32);
    hasPublicKey_ = true;
    return true;
}

std::vector<uint8_t> ESP32KeyPair::sha256(const std::vector<uint8_t>& data) const {
    std::vector<uint8_t> hash(32);
    esp_sha256_t sha256_ctx;
    
    esp_sha256_init(&sha256_ctx);
    esp_sha256_update(&sha256_ctx, data.data(), data.size());
    esp_sha256_finish(&sha256_ctx, hash.data());
    
    return hash;
}

} // namespace crypto
} // namespace one 