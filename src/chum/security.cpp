#include "security.h"
#include <Arduino.h>
#include <mbedtls/rsa.h>
#include <mbedtls/error.h>
#include <algorithm>

namespace chum {

// Constants
constexpr const char* PERSONALIZATION_STRING = "CHUM_SECURITY_INIT";
constexpr size_t KEY_SIZE = 2048;  // RSA key size in bits

Security::Security() : initialized_(false) {
    mbedtls_pk_init(&pk_);
    mbedtls_entropy_init(&entropy_);
    mbedtls_ctr_drbg_init(&ctr_drbg_);
    initialized_ = initializeMbedTLS();
}

Security::~Security() {
    cleanupMbedTLS();
}

bool Security::initializeMbedTLS() {
    int ret = mbedtls_ctr_drbg_seed(&ctr_drbg_, mbedtls_entropy_func, &entropy_,
                                   reinterpret_cast<const unsigned char*>(PERSONALIZATION_STRING),
                                   strlen(PERSONALIZATION_STRING));
    return ret == 0;
}

void Security::cleanupMbedTLS() {
    mbedtls_pk_free(&pk_);
    mbedtls_ctr_drbg_free(&ctr_drbg_);
    mbedtls_entropy_free(&entropy_);
    initialized_ = false;
}

bool Security::generateKeyPair() {
    if (!initialized_) return false;
    
    int ret = mbedtls_pk_setup(&pk_, mbedtls_pk_info_from_type(MBEDTLS_PK_RSA));
    if (ret != 0) {
        return false;
    }

    ret = mbedtls_rsa_gen_key(mbedtls_pk_rsa(pk_), mbedtls_ctr_drbg_random, &ctr_drbg_,
                             KEY_SIZE, 65537);
    if (ret != 0) {
        return false;
    }

    // Export public key
    unsigned char temp_buf[4096];
    ret = mbedtls_pk_write_pubkey_pem(&pk_, temp_buf, sizeof(temp_buf));
    if (ret != 0) {
        return false;
    }
    keyPair_.publicKey.assign(temp_buf, temp_buf + strlen(reinterpret_cast<char*>(temp_buf)));

    // Export private key
    ret = mbedtls_pk_write_key_pem(&pk_, temp_buf, sizeof(temp_buf));
    if (ret != 0) {
        return false;
    }
    keyPair_.privateKey.assign(temp_buf, temp_buf + strlen(reinterpret_cast<char*>(temp_buf)));

    return true;
}

bool Security::setKeyPair(const uint8_t* publicKey, size_t pubLen, 
                         const uint8_t* privateKey, size_t privLen) {
    if (!initialized_) return false;

    keyPair_.publicKey.assign(publicKey, publicKey + pubLen);
    keyPair_.privateKey.assign(privateKey, privateKey + privLen);

    // Parse the private key
    int ret = mbedtls_pk_parse_key(&pk_, 
                                  privateKey, privLen,
                                  nullptr, 0);
    return ret == 0;
}

std::vector<uint8_t> Security::encrypt(const uint8_t* data, size_t length,
                                     const std::vector<uint8_t>& recipientPublicKey) {
    if (!initialized_) return std::vector<uint8_t>();

    mbedtls_pk_context recipient_ctx;
    mbedtls_pk_init(&recipient_ctx);

    std::vector<uint8_t> output;
    output.resize(KEY_SIZE / 8);

    int ret = mbedtls_pk_parse_public_key(&recipient_ctx, 
                                         recipientPublicKey.data(),
                                         recipientPublicKey.size());
    if (ret != 0) {
        mbedtls_pk_free(&recipient_ctx);
        return std::vector<uint8_t>();
    }

    size_t olen = 0;
    ret = mbedtls_pk_encrypt(&recipient_ctx, data, length,
                            output.data(), &olen, output.size(),
                            mbedtls_ctr_drbg_random, &ctr_drbg_);

    mbedtls_pk_free(&recipient_ctx);

    if (ret != 0) {
        return std::vector<uint8_t>();
    }

    output.resize(olen);
    return output;
}

std::vector<uint8_t> Security::decrypt(const uint8_t* data, size_t length) {
    if (!initialized_) return std::vector<uint8_t>();

    std::vector<uint8_t> output;
    output.resize(KEY_SIZE / 8);

    size_t olen = 0;
    int ret = mbedtls_pk_decrypt(&pk_, data, length,
                                output.data(), &olen, output.size(),
                                mbedtls_ctr_drbg_random, &ctr_drbg_);

    if (ret != 0) {
        return std::vector<uint8_t>();
    }

    output.resize(olen);
    return output;
}

std::vector<uint8_t> Security::sign(const uint8_t* data, size_t length) {
    if (!initialized_) return std::vector<uint8_t>();

    std::vector<uint8_t> signature;
    signature.resize(MBEDTLS_MPI_MAX_SIZE);

    size_t olen = 0;
    int ret = mbedtls_pk_sign(&pk_, MBEDTLS_MD_SHA256, data, length,
                             signature.data(), &olen,
                             mbedtls_ctr_drbg_random, &ctr_drbg_);

    if (ret != 0) {
        return std::vector<uint8_t>();
    }

    signature.resize(olen);
    return signature;
}

bool Security::verify(const uint8_t* data, size_t length,
                     const uint8_t* signature, size_t sigLen,
                     const std::vector<uint8_t>& signerPublicKey) {
    mbedtls_pk_context signer_ctx;
    mbedtls_pk_init(&signer_ctx);

    int ret = mbedtls_pk_parse_public_key(&signer_ctx,
                                         signerPublicKey.data(),
                                         signerPublicKey.size());
    if (ret != 0) {
        mbedtls_pk_free(&signer_ctx);
        return false;
    }

    ret = mbedtls_pk_verify(&signer_ctx, MBEDTLS_MD_SHA256,
                           data, length, signature, sigLen);

    mbedtls_pk_free(&signer_ctx);
    return ret == 0;
}

bool Security::addTrustedPeer(const std::vector<uint8_t>& publicKey) {
    if (isTrustedPeer(publicKey)) {
        return false;
    }
    trustedPeers_.push_back(publicKey);
    return true;
}

bool Security::removeTrustedPeer(const std::vector<uint8_t>& publicKey) {
    auto it = std::find(trustedPeers_.begin(), trustedPeers_.end(), publicKey);
    if (it != trustedPeers_.end()) {
        trustedPeers_.erase(it);
        return true;
    }
    return false;
}

bool Security::isTrustedPeer(const std::vector<uint8_t>& publicKey) const {
    return std::find(trustedPeers_.begin(), trustedPeers_.end(), publicKey) != trustedPeers_.end();
}

} // namespace chum 