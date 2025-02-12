#pragma once

// Our own crypto constants must be included first
#include "crypto_constants.h"
#include "keys.h"

// Standard library includes
#include <string>
#include <vector>
#include <ctime>

// Arduino includes
#include <ArduinoJson.h>

// mbedtls includes
#include <mbedtls/pk.h>

namespace one {
namespace instance {

using namespace crypto;  // Use the crypto namespace for constants

// Standard claim types
namespace claims {
    constexpr const char* OWNER = "owner";
    constexpr const char* EMAIL = "email";
    constexpr const char* ROLE = "role";
    constexpr const char* DEVICE_ID = "deviceId";
    constexpr const char* VERSION = "version";
}

class Credential {
public:
    // Use mbedTLS max signature size
    static constexpr size_t SIGNATURE_SIZE = MBEDTLS_PK_SIGNATURE_MAX_SIZE;

    struct Claim {
        std::string type;
        std::string value;
    };

    Credential(InstanceKeys& issuerKeys);

    // Add a claim to the credential
    void addClaim(const std::string& type, const std::string& value);

    // Set credential metadata
    void setSubject(const std::string& subject) { subject_ = subject; }
    void setExpiry(time_t expiry) { expiry_ = expiry; }
    void setIssuedAt(time_t issuedAt) { issuedAt_ = issuedAt; }

    // Issue the credential by signing it
    bool issue();

    // Verify the credential using issuer's public key
    bool verify(const uint8_t* issuerPublicKey) const;

    // Serialize/deserialize credential
    String serialize() const;
    bool deserialize(const String& json);

private:
    // Generate the message to be signed
    void generateMessage(uint8_t* message, size_t* messageLen) const;

    InstanceKeys& issuerKeys_;
    std::string subject_;
    time_t issuedAt_;
    time_t expiry_;
    std::vector<Claim> claims_;
    uint8_t signature_[SIGNATURE_SIZE];
    bool isIssued_;
};

} // namespace instance
} // namespace one 

