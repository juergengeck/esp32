#pragma once

// Our own crypto constants must be included first
#include "crypto_constants.h"

// Standard library includes
#include <vector>
#include <memory>

// Arduino includes
#include <Arduino.h>
#include <ArduinoJson.h>

// mbedtls includes
#include "mbedtls/sha256.h"
#include "mbedtls/pk.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/ecdsa.h"
#include "mbedtls/ecp.h"
#include "mbedtls/error.h"

namespace one {
namespace instance {

using namespace crypto;  // Use the crypto namespace for constants

struct VerifiableCredential {
    String instanceId;  // UUID format
    String did;        // did:one:<instance_id>
    String type;
    std::vector<String> capabilities;
    String proof;
    String publicKey;  // ECDSA P-256 public key in hex
};

class CredentialManager {
public:
    static CredentialManager& getInstance() {
        static CredentialManager instance;
        return instance;
    }

    bool initialize(const char* instanceId, const char* ownerPublicKey);
    bool verifyCredential(const char* presentedVc);
    bool verifyOwnerSignature(const String& data, const String& signature);
    bool hasCapability(const String& capability) const;
    
    // Identity getters
    const String& getInstanceId() const { return instanceId_; }
    const String& getDid() const { return did_; }
    const String& getOwnerPublicKey() const { return ownerPublicKey_; }
    
private:
    CredentialManager();
    ~CredentialManager();
    CredentialManager(const CredentialManager&) = delete;
    CredentialManager& operator=(const CredentialManager&) = delete;

    // Verification operations
    bool verifySignature(const String& data, const String& signature, const String& publicKey);
    bool parseCredential(const char* vcJson, VerifiableCredential& vc);
    
    // Helper functions
    String constructDid(const String& instanceId) const;
    bool validateInstanceId(const String& instanceId) const;
    bool initializeCrypto();
    void cleanupCrypto();

    String instanceId_;     // UUID format
    String did_;           // did:one:<instance_id>
    String ownerPublicKey_; // Owner's ECDSA P-256 public key in hex
    VerifiableCredential deviceCredential_;
    
    // mbedtls contexts for verification
    mbedtls_pk_context pk_;
    mbedtls_entropy_context entropy_;
    mbedtls_ctr_drbg_context ctr_drbg_;
    
    bool isInitialized_ = false;
};

} // namespace instance
} // namespace one 