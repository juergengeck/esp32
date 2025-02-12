#pragma once

// Our own crypto constants must be included first
#include "crypto_constants.h"

// mbedtls includes
#include <mbedtls/pk.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/ecdsa.h>
#include <cstdint>

namespace one {
namespace instance {

using namespace crypto;  // Use the crypto namespace for constants

// Forward declare Instance class
class Instance;

class InstanceKeys {
public:
    static const size_t KEY_SIZE = ECDSA_PRIVATE_KEY_LENGTH;  // Size for ECDSA secp256r1
    
    InstanceKeys();
    ~InstanceKeys();
    
    bool generate();
    bool verify(const uint8_t* message, size_t messageLen,
               const uint8_t* signature, size_t signatureLen) const;
    bool sign(const uint8_t* message, size_t messageLen,
             uint8_t* signature, size_t* signatureLen);
    
    bool load(const char* path);
    bool save(const char* path) const;
    
    // Add missing method declarations
    bool importKeys(const uint8_t* priv_key, const uint8_t* pub_key);
    bool exportPublicKey(uint8_t* pub_key) const;
    
    // Getters for public key
    const uint8_t* getPublicKey() const { return publicKey_; }
    size_t getPublicKeySize() const { return KEY_SIZE; }

private:
    // Make Instance a friend class
    friend class Instance;

    uint8_t privateKey_[KEY_SIZE];
    uint8_t publicKey_[KEY_SIZE];
    bool hasKeys_;
    
    mbedtls_pk_context pk_;
    mbedtls_entropy_context entropy_;
    mbedtls_ctr_drbg_context ctr_drbg_;
};

} // namespace instance
} // namespace one 