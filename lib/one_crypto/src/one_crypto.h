#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <optional>
#include <ctime>

// Forward declare mbedtls types to avoid including headers in interface
typedef struct mbedtls_pk_context mbedtls_pk_context;
typedef struct mbedtls_entropy_context mbedtls_entropy_context;
typedef struct mbedtls_ctr_drbg_context mbedtls_ctr_drbg_context;

namespace one {
namespace crypto {

// Constants for ECDSA P-256
constexpr size_t ECDSA_PUBLIC_KEY_LENGTH = 64;   // X and Y coordinates
constexpr size_t ECDSA_PRIVATE_KEY_LENGTH = 32;  // Private key length
constexpr size_t ECDSA_SIGNATURE_LENGTH = 64;    // R and S values

class KeyPair {
public:
    KeyPair();
    ~KeyPair();

    // Key generation and management
    bool generate();
    bool importPrivateKey(const std::vector<uint8_t>& privateKey);
    bool importPublicKey(const std::vector<uint8_t>& publicKey);
    std::vector<uint8_t> exportPrivateKey() const;
    std::vector<uint8_t> exportPublicKey() const;

    // Signing and verification
    std::optional<std::vector<uint8_t>> sign(const std::vector<uint8_t>& message) const;
    bool verify(const std::vector<uint8_t>& message, const std::vector<uint8_t>& signature) const;

    // Serialization helpers
    static std::string toHex(const std::vector<uint8_t>& data);
    static std::vector<uint8_t> fromHex(const std::string& hex);
    static std::string toBase64(const std::vector<uint8_t>& data);
    static std::vector<uint8_t> fromBase64(const std::string& base64);

private:
    mbedtls_pk_context* ctx_;
    mbedtls_entropy_context* entropy_;
    mbedtls_ctr_drbg_context* ctr_drbg_;

    bool hasPrivateKey_;
    bool hasPublicKey_;
};

class VerifiableCredential {
public:
    VerifiableCredential();
    ~VerifiableCredential() = default;

    // Credential fields
    void setContext(const std::string& context);
    void setType(const std::string& type);
    void setIssuer(const std::string& issuer);
    void setIssuanceDate(const std::string& date);
    void setExpirationDate(const std::string& date);
    void setSubject(const std::string& id, const std::string& name);
    
    // Capability management
    void addCapability(const std::string& capability);
    bool hasCapability(const std::string& capability) const;
    bool isExpired() const;

    // Signing and verification
    bool sign(const KeyPair& issuerKeys);
    bool verify(const std::vector<uint8_t>& issuerPublicKey) const;

    // Serialization
    std::string toJson() const;
    static std::optional<VerifiableCredential> fromJson(const std::string& json);

    // Get canonical form for signing/verification
    std::string getCanonicalForm() const;

private:
    std::string formatDate(time_t timestamp) const;

    std::string context_;
    std::string type_;
    std::string issuer_;
    time_t issuanceDate_;
    time_t expirationDate_;
    std::string subjectId_;
    std::string subjectName_;
    std::vector<std::string> capabilities_;
    std::vector<uint8_t> signature_;
};

} // namespace crypto
} // namespace one 