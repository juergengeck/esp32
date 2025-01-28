#pragma once

#include <mbedtls/pk.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <memory>
#include <vector>
#include <string>

namespace chum {

struct KeyPair {
    std::vector<uint8_t> publicKey;
    std::vector<uint8_t> privateKey;
};

class Security {
public:
    Security();
    ~Security();

    // Key management
    bool generateKeyPair();
    bool setKeyPair(const uint8_t* publicKey, size_t publicKeyLen,
                   const uint8_t* privateKey, size_t privateKeyLen);
    const KeyPair& getKeyPair() const { return keyPair_; }

    // Cryptographic operations
    std::vector<uint8_t> encrypt(const uint8_t* data, size_t length,
                                const std::vector<uint8_t>& recipientPublicKey);
    std::vector<uint8_t> decrypt(const uint8_t* data, size_t length);
    std::vector<uint8_t> sign(const uint8_t* data, size_t len);
    bool verify(const uint8_t* data, size_t dataLen,
               const uint8_t* signature, size_t signatureLen,
               const std::vector<uint8_t>& publicKey);

    // Trust management
    bool addTrustedPeer(const std::vector<uint8_t>& publicKey);
    bool removeTrustedPeer(const std::vector<uint8_t>& publicKey);
    bool isTrustedPeer(const std::vector<uint8_t>& publicKey) const;

private:
    bool initializeMbedTLS();
    void cleanupMbedTLS();

    KeyPair keyPair_;
    mbedtls_pk_context pk_;
    mbedtls_entropy_context entropy_;
    mbedtls_ctr_drbg_context ctr_drbg_;
    bool initialized_;
    std::vector<std::vector<uint8_t>> trustedPeers_;
};

} // namespace chum 