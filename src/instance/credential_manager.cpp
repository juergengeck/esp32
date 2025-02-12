#include "credential_manager.h"

// Standard library includes
#include <vector>
#include <memory>

// Arduino includes
#include <ArduinoJson.h>
#include <SPIFFS.h>

// mbedtls includes
#include "mbedtls/platform.h"
#include "mbedtls/sha256.h"
#include "mbedtls/pk.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/ecdsa.h"
#include "mbedtls/ecp.h"
#include "mbedtls/error.h"
#include "mbedtls/base64.h"

namespace one {
namespace instance {

using namespace crypto;  // Use the crypto namespace for constants

CredentialManager::CredentialManager() {
    mbedtls_pk_init(&pk_);
    mbedtls_entropy_init(&entropy_);
    mbedtls_ctr_drbg_init(&ctr_drbg_);
}

CredentialManager::~CredentialManager() {
    cleanupCrypto();
}

bool CredentialManager::initializeCrypto() {
    int ret;

    // Initialize random number generator
    ret = mbedtls_ctr_drbg_seed(&ctr_drbg_, mbedtls_entropy_func, &entropy_,
                                nullptr, 0);
    if (ret != 0) {
        return false;
    }

    // Initialize key context with ECDSA P-256
    ret = mbedtls_pk_setup(&pk_, mbedtls_pk_info_from_type(MBEDTLS_PK_ECDSA));
    if (ret != 0) {
        return false;
    }

    // Set up ECDSA curve
    mbedtls_ecp_keypair *keypair = mbedtls_pk_ec(pk_);
    ret = mbedtls_ecp_group_load(&keypair->grp, MBEDTLS_ECP_DP_SECP256R1);
    if (ret != 0) {
        return false;
    }

    return true;
}

void CredentialManager::cleanupCrypto() {
    mbedtls_pk_free(&pk_);
    mbedtls_entropy_free(&entropy_);
    mbedtls_ctr_drbg_free(&ctr_drbg_);
}

bool CredentialManager::initialize(const char* instanceId, const char* ownerPublicKey) {
    if (isInitialized_) {
        return true;
    }

    if (!validateInstanceId(instanceId)) {
        Serial.println("Invalid Instance ID format");
        return false;
    }

    if (!initializeCrypto()) {
        Serial.println("Failed to initialize crypto");
        return false;
    }

    instanceId_ = instanceId;
    did_ = constructDid(instanceId_);
    ownerPublicKey_ = ownerPublicKey;
    
    // Create minimal device credential
    deviceCredential_.instanceId = instanceId_;
    deviceCredential_.did = did_;
    deviceCredential_.type = "ESP32Device";
    deviceCredential_.capabilities = {"quic", "secure-storage"};
    deviceCredential_.publicKey = ownerPublicKey_; // The owner's public key
    
    isInitialized_ = true;
    
    Serial.println("Credential Manager initialized:");
    Serial.printf("Instance ID: %s\n", instanceId_.c_str());
    Serial.printf("DID: %s\n", did_.c_str());
    Serial.printf("Owner Public Key: %s\n", ownerPublicKey_.c_str());
    
    return true;
}

bool CredentialManager::verifyCredential(const char* presentedVc) {
    if (!isInitialized_) {
        return false;
    }

    VerifiableCredential vc;
    if (!parseCredential(presentedVc, vc)) {
        Serial.println("Failed to parse credential");
        return false;
    }

    // Verify the credential structure
    if (vc.instanceId.isEmpty() || vc.did.isEmpty() || 
        vc.type.isEmpty() || vc.proof.isEmpty() || vc.publicKey.isEmpty()) {
        Serial.println("Invalid credential structure");
        return false;
    }

    // Verify DID construction
    if (vc.did != constructDid(vc.instanceId)) {
        Serial.println("DID does not match Instance ID");
        return false;
    }

    // Extract and verify signature
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, presentedVc);
    if (error) {
        Serial.println("Failed to parse VC JSON");
        return false;
    }

    String payload = doc["credentialSubject"].as<String>();
    String signature = doc["proof"]["proofValue"].as<String>();
    
    // Verify using the owner's public key
    return verifySignature(payload, signature, ownerPublicKey_);
}

bool CredentialManager::verifyOwnerSignature(const String& data, const String& signature) {
    if (!isInitialized_) {
        return false;
    }
    
    return verifySignature(data, signature, ownerPublicKey_);
}

bool CredentialManager::hasCapability(const String& capability) const {
    if (!isInitialized_) {
        return false;
    }

    for (const auto& cap : deviceCredential_.capabilities) {
        if (cap == capability) {
            return true;
        }
    }
    return false;
}

bool CredentialManager::verifySignature(const String& data, const String& signature, const String& publicKey) {
    std::vector<unsigned char> sig;
    size_t sigLen;
    
    // Decode base64 signature
    unsigned char* sig_buf = new unsigned char[signature.length()];
    size_t sig_len;
    
    int ret = mbedtls_base64_decode(sig_buf, signature.length(), &sig_len,
                               (const unsigned char*)signature.c_str(), signature.length());
    if (ret != 0) {
        delete[] sig_buf;
        return false;
    }
    
    sig.assign(sig_buf, sig_buf + sig_len);
    delete[] sig_buf;
    
    // Initialize key context
    mbedtls_pk_context pk;
    mbedtls_pk_init(&pk);
    
    // Setup key context with ECDSA P-256
    ret = mbedtls_pk_setup(&pk, mbedtls_pk_info_from_type(MBEDTLS_PK_ECDSA));
    if (ret != 0) {
        mbedtls_pk_free(&pk);
        return false;
    }

    // Set up ECDSA curve
    mbedtls_ecp_keypair *keypair = mbedtls_pk_ec(pk);
    ret = mbedtls_ecp_group_load(&keypair->grp, MBEDTLS_ECP_DP_SECP256R1);
    if (ret != 0) {
        mbedtls_pk_free(&pk);
        return false;
    }
    
    // Import public key
    ret = mbedtls_pk_parse_public_key(&pk, (const unsigned char*)publicKey.c_str(), publicKey.length());
    if (ret != 0) {
        mbedtls_pk_free(&pk);
        return false;
    }
    
    // Calculate hash of data
    unsigned char hash[32];
    ret = mbedtls_sha256_ret((const unsigned char*)data.c_str(), data.length(), hash, 0);
    if (ret != 0) {
        mbedtls_pk_free(&pk);
        return false;
    }
    
    // Verify signature
    ret = mbedtls_pk_verify(&pk, MBEDTLS_MD_SHA256, hash, sizeof(hash), sig.data(), sig.size());
    mbedtls_pk_free(&pk);
    
    return ret == 0;
}

bool CredentialManager::parseCredential(const char* vcJson, VerifiableCredential& vc) {
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, vcJson);
    if (error) {
        return false;
    }

    JsonObject credentialSubject = doc["credentialSubject"];
    if (!credentialSubject.isNull()) {
        vc.instanceId = credentialSubject["instanceId"].as<const char*>();
        vc.did = credentialSubject["id"].as<const char*>();
        vc.type = credentialSubject["type"].as<const char*>();
        vc.publicKey = credentialSubject["publicKey"].as<const char*>();
        
        JsonArray caps = credentialSubject["capabilities"];
        if (!caps.isNull()) {
            for (JsonVariant cap : caps) {
                vc.capabilities.push_back(cap.as<String>());
            }
        }
    }

    JsonObject proof = doc["proof"];
    if (!proof.isNull()) {
        vc.proof = proof["proofValue"].as<const char*>();
    }

    return true;
}

String CredentialManager::constructDid(const String& instanceId) const {
    return "did:one:" + instanceId;
}

bool CredentialManager::validateInstanceId(const String& instanceId) const {
    // Basic UUID format validation
    if (instanceId.length() != 36) {
        return false;
    }
    
    // Check for hyphens in correct positions
    if (instanceId[8] != '-' || instanceId[13] != '-' || 
        instanceId[18] != '-' || instanceId[23] != '-') {
        return false;
    }
    
    // Check for valid hex characters
    for (size_t i = 0; i < instanceId.length(); i++) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            continue;  // Skip hyphens
        }
        char c = instanceId[i];
        if (!isHexadecimalDigit(c)) {
            return false;
        }
    }
    
    return true;
}

} // namespace instance
} // namespace one 