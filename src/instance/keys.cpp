#include "keys.h"
#include <Arduino.h>
#include <SPIFFS.h>
#include <esp_system.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/pk.h>
#include <mbedtls/ecp.h>
#include <esp_log.h>

namespace one {
namespace instance {

static const char* TAG = "InstanceKeys";

InstanceKeys::InstanceKeys() : hasKeys_(false) {
    mbedtls_pk_init(&pk_);
    mbedtls_entropy_init(&entropy_);
    mbedtls_ctr_drbg_init(&ctr_drbg_);
}

InstanceKeys::~InstanceKeys() {
    mbedtls_pk_free(&pk_);
    mbedtls_entropy_free(&entropy_);
    mbedtls_ctr_drbg_free(&ctr_drbg_);
}

bool InstanceKeys::generate() {
    int ret;
    const char* pers = "esp32_key_gen";
    
    // Initialize random number generator
    ret = mbedtls_ctr_drbg_seed(&ctr_drbg_, mbedtls_entropy_func, &entropy_,
                                (const unsigned char*)pers, strlen(pers));
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to seed RNG: -0x%04x", -ret);
        return false;
    }
    
    // Setup for ECDSA with curve secp256r1
    ret = mbedtls_pk_setup(&pk_, mbedtls_pk_info_from_type(MBEDTLS_PK_ECDSA));
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to setup key context: -0x%04x", -ret);
        return false;
    }
    
    // Generate key pair
    mbedtls_ecp_keypair* keypair = mbedtls_pk_ec(pk_);
    ret = mbedtls_ecp_gen_key(MBEDTLS_ECP_DP_SECP256R1,
                             keypair,
                             mbedtls_ctr_drbg_random,
                             &ctr_drbg_);
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to generate key pair: -0x%04x", -ret);
        return false;
    }
    
    // Export public key
    unsigned char temp[128];
    size_t len = 0;
    
    ret = mbedtls_pk_write_pubkey_der(&pk_, temp, sizeof(temp));
    if (ret < 0) {
        ESP_LOGE(TAG, "Failed to export public key: -0x%04x", -ret);
        return false;
    }
    len = ret;
    memcpy(publicKey_, temp + sizeof(temp) - len, len > KEY_SIZE ? KEY_SIZE : len);
    
    // Export private key
    ret = mbedtls_pk_write_key_der(&pk_, temp, sizeof(temp));
    if (ret < 0) {
        ESP_LOGE(TAG, "Failed to export private key: -0x%04x", -ret);
        return false;
    }
    len = ret;
    memcpy(privateKey_, temp + sizeof(temp) - len, len > KEY_SIZE ? KEY_SIZE : len);
    
    hasKeys_ = true;
    ESP_LOGI(TAG, "Key pair generated successfully");
    return true;
}

bool InstanceKeys::verify(const uint8_t* message, size_t messageLen,
                         const uint8_t* signature, size_t signatureLen) const {
    if (!hasKeys_) {
        ESP_LOGE(TAG, "No keys available for verification");
        return false;
    }
    
    int ret;
    mbedtls_pk_context verify_ctx;
    mbedtls_pk_init(&verify_ctx);
    
    ret = mbedtls_pk_parse_public_key(&verify_ctx, publicKey_, KEY_SIZE);
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to parse public key: -0x%04x", -ret);
        mbedtls_pk_free(&verify_ctx);
        return false;
    }
    
    ret = mbedtls_pk_verify(&verify_ctx, MBEDTLS_MD_SHA256,
                           message, messageLen,
                           signature, signatureLen);
    
    mbedtls_pk_free(&verify_ctx);
    
    if (ret != 0) {
        ESP_LOGE(TAG, "Signature verification failed: -0x%04x", -ret);
        return false;
    }
    
    return true;
}

bool InstanceKeys::sign(const uint8_t* message, size_t messageLen,
                     uint8_t* signature, size_t* signatureLen) {
    if (!hasKeys_) {
        ESP_LOGE(TAG, "No keys available for signing");
        return false;
    }

    // Initialize the PK context for signing
    int ret = mbedtls_pk_setup(&pk_, mbedtls_pk_info_from_type(MBEDTLS_PK_ECDSA));
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to setup PK context for signing: %d", ret);
        return false;
    }

    // Set up the private key
    mbedtls_ecp_keypair* keypair = mbedtls_pk_ec(pk_);
    ret = mbedtls_ecp_group_load(&keypair->grp, MBEDTLS_ECP_DP_SECP256R1);
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to load EC group: %d", ret);
        return false;
    }

    // Import the private key
    ret = mbedtls_mpi_read_binary(&keypair->d, privateKey_, KEY_SIZE);
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to import private key: %d", ret);
        return false;
    }

    // Initialize entropy source and RNG
    const char* pers = "ecdsa";
    ret = mbedtls_ctr_drbg_seed(&ctr_drbg_, mbedtls_entropy_func, &entropy_,
                                (const unsigned char*)pers, strlen(pers));
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to seed RNG: %d", ret);
        return false;
    }

    // Sign the message
    ret = mbedtls_pk_sign(&pk_, MBEDTLS_MD_SHA256, message, messageLen,
                         signature, signatureLen, mbedtls_ctr_drbg_random, &ctr_drbg_);
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to sign message: %d", ret);
        return false;
    }

    return true;
}

bool InstanceKeys::load(const char* path) {
    if (!SPIFFS.exists(path)) {
        ESP_LOGE(TAG, "Key file does not exist: %s", path);
        return false;
    }
    
    File file = SPIFFS.open(path, "r");
    if (!file) {
        ESP_LOGE(TAG, "Failed to open key file: %s", path);
        return false;
    }
    
    bool success = file.readBytes((char*)privateKey_, KEY_SIZE) == KEY_SIZE &&
                  file.readBytes((char*)publicKey_, KEY_SIZE) == KEY_SIZE;
    
    file.close();
    
    if (success) {
        hasKeys_ = true;
        ESP_LOGI(TAG, "Keys loaded successfully");
    } else {
        ESP_LOGE(TAG, "Failed to read keys from file");
    }
    
    return success;
}

bool InstanceKeys::save(const char* path) const {
    if (!hasKeys_) {
        ESP_LOGE(TAG, "No keys available to save");
        return false;
    }
    
    File file = SPIFFS.open(path, "w");
    if (!file) {
        ESP_LOGE(TAG, "Failed to create key file: %s", path);
        return false;
    }
    
    bool success = file.write(privateKey_, KEY_SIZE) == KEY_SIZE &&
                  file.write(publicKey_, KEY_SIZE) == KEY_SIZE;
    
    file.close();
    
    if (success) {
        ESP_LOGI(TAG, "Keys saved successfully");
    } else {
        ESP_LOGE(TAG, "Failed to write keys to file");
    }
    
    return success;
}

bool InstanceKeys::importKeys(const uint8_t* priv_key, const uint8_t* pub_key) {
    if (hasKeys_) {
        ESP_LOGE(TAG, "Keys already initialized");
        return false;
    }

    // Copy the keys
    memcpy(privateKey_, priv_key, KEY_SIZE);
    memcpy(publicKey_, pub_key, KEY_SIZE);
    
    // Verify the key pair is valid by signing and verifying a test message
    const uint8_t testMsg[] = "test";
    uint8_t signature[KEY_SIZE];
    size_t sigLen = sizeof(signature);
    
    if (!sign(testMsg, sizeof(testMsg), signature, &sigLen)) {
        ESP_LOGE(TAG, "Imported private key verification failed");
        memset(privateKey_, 0, KEY_SIZE);
        memset(publicKey_, 0, KEY_SIZE);
        return false;
    }
    
    if (!verify(testMsg, sizeof(testMsg), signature, sigLen)) {
        ESP_LOGE(TAG, "Imported public key verification failed");
        memset(privateKey_, 0, KEY_SIZE);
        memset(publicKey_, 0, KEY_SIZE);
        return false;
    }

    hasKeys_ = true;
    ESP_LOGI(TAG, "Keys imported and verified successfully");
    return true;
}

bool InstanceKeys::exportPublicKey(uint8_t* pub_key) const {
    if (!hasKeys_) {
        ESP_LOGE(TAG, "No keys available to export");
        return false;
    }
    
    memcpy(pub_key, publicKey_, KEY_SIZE);
    return true;
}

} // namespace instance
} // namespace one 

