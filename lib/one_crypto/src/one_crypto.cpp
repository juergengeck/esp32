#include "one_crypto.h"
#include <mbedtls/pk.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/ecdsa.h>
#include <mbedtls/base64.h>
#include <mbedtls/sha256.h>
#include <mbedtls/pem.h>
#include <mbedtls/error.h>
#include <sstream>
#include <iomanip>
#include <iostream>

#ifdef ARDUINO
#include <Arduino.h>
#include <ArduinoJson.h>
using String = ::String;
#else
#include <nlohmann/json.hpp>
using json = nlohmann::json;
using String = std::string;
#endif

namespace one {
namespace crypto {

KeyPair::KeyPair() 
    : ctx_(new mbedtls_pk_context)
    , entropy_(new mbedtls_entropy_context)
    , ctr_drbg_(new mbedtls_ctr_drbg_context)
    , hasPrivateKey_(false)
    , hasPublicKey_(false) {
    
    mbedtls_pk_init(ctx_);
    mbedtls_entropy_init(entropy_);
    mbedtls_ctr_drbg_init(ctr_drbg_);

    // Initialize random number generator
    int ret = mbedtls_ctr_drbg_seed(ctr_drbg_, mbedtls_entropy_func, entropy_,
                                   nullptr, 0);
    if (ret != 0) {
        std::cerr << "Failed to seed RNG" << std::endl;
        return;
    }

    // Set up ECDSA context
    ret = mbedtls_pk_setup(ctx_, mbedtls_pk_info_from_type(MBEDTLS_PK_ECDSA));
    if (ret != 0) {
        std::cerr << "Failed to setup PK context" << std::endl;
        return;
    }

    // Set up ECDSA curve
    mbedtls_ecp_keypair* keypair = mbedtls_pk_ec(*ctx_);
    ret = mbedtls_ecp_group_load(&keypair->MBEDTLS_PRIVATE(grp), MBEDTLS_ECP_DP_SECP256R1);
    if (ret != 0) {
        std::cerr << "Failed to load curve" << std::endl;
        return;
    }
}

KeyPair::~KeyPair() {
    mbedtls_pk_free(ctx_);
    mbedtls_entropy_free(entropy_);
    mbedtls_ctr_drbg_free(ctr_drbg_);
    delete ctx_;
    delete entropy_;
    delete ctr_drbg_;
}

bool KeyPair::generate() {
    int ret;

    // Generate key pair
    mbedtls_ecp_keypair* keypair = mbedtls_pk_ec(*ctx_);
    ret = mbedtls_ecp_gen_key(MBEDTLS_ECP_DP_SECP256R1,
                             keypair,
                             mbedtls_ctr_drbg_random,
                             ctr_drbg_);
    if (ret != 0) {
        char error_buf[100];
        mbedtls_strerror(ret, error_buf, sizeof(error_buf));
        std::cerr << "Failed to generate key pair: " << error_buf << " (" << ret << ")" << std::endl;
        return false;
    }

    hasPrivateKey_ = true;
    hasPublicKey_ = true;
    return true;
}

bool KeyPair::importPrivateKey(const std::vector<uint8_t>& privateKey) {
    // Check private key size
    if (privateKey.size() != 32) {
        std::cerr << "Invalid private key size" << std::endl;
        return false;
    }

    // Import the private key
    mbedtls_ecp_keypair* keypair = mbedtls_pk_ec(*ctx_);
    int ret = mbedtls_mpi_read_binary(&keypair->MBEDTLS_PRIVATE(d), privateKey.data(), privateKey.size());
    if (ret != 0) {
        std::cerr << "Failed to import private key" << std::endl;
        return false;
    }

    // Generate public key from private key
    ret = mbedtls_ecp_mul(&keypair->MBEDTLS_PRIVATE(grp), 
                         &keypair->MBEDTLS_PRIVATE(Q),
                         &keypair->MBEDTLS_PRIVATE(d),
                         &keypair->MBEDTLS_PRIVATE(grp).G,
                         mbedtls_ctr_drbg_random,
                         ctr_drbg_);
    if (ret != 0) {
        std::cerr << "Failed to generate public key from private key" << std::endl;
        return false;
    }

    hasPrivateKey_ = true;
    hasPublicKey_ = true;
    return true;
}

bool KeyPair::importPublicKey(const std::vector<uint8_t>& publicKey) {
    // Import the public key (expecting uncompressed format: 0x04 + X + Y)
    if (publicKey.size() != 65 || publicKey[0] != 0x04) {
        std::cerr << "Invalid public key format" << std::endl;
        return false;
    }

    mbedtls_ecp_keypair* keypair = mbedtls_pk_ec(*ctx_);
    int ret = mbedtls_ecp_point_read_binary(&keypair->MBEDTLS_PRIVATE(grp), 
                                           &keypair->MBEDTLS_PRIVATE(Q),
                                           publicKey.data(), publicKey.size());
    if (ret != 0) {
        std::cerr << "Failed to import public key" << std::endl;
        return false;
    }

    hasPublicKey_ = true;
    return true;
}

std::vector<uint8_t> KeyPair::exportPrivateKey() const {
    if (!hasPrivateKey_) {
        std::cerr << "No private key to export" << std::endl;
        return std::vector<uint8_t>();
    }

    std::vector<uint8_t> buffer(32); // ECDSA P-256 private key is always 32 bytes
    mbedtls_ecp_keypair* keypair = mbedtls_pk_ec(*ctx_);
    
    int ret = mbedtls_mpi_write_binary(&keypair->MBEDTLS_PRIVATE(d), buffer.data(), buffer.size());
    if (ret != 0) {
        std::cerr << "Failed to export private key" << std::endl;
        return std::vector<uint8_t>();
    }

    return buffer;
}

std::vector<uint8_t> KeyPair::exportPublicKey() const {
    if (!hasPublicKey_) {
        std::cerr << "No public key to export" << std::endl;
        return std::vector<uint8_t>();
    }

    // Export uncompressed public key (65 bytes: 0x04 + 32 bytes X + 32 bytes Y)
    std::vector<uint8_t> buffer(65);
    mbedtls_ecp_keypair* keypair = mbedtls_pk_ec(*ctx_);
    size_t length;
    
    int ret = mbedtls_ecp_point_write_binary(&keypair->MBEDTLS_PRIVATE(grp), 
                                            &keypair->MBEDTLS_PRIVATE(Q),
                                            MBEDTLS_ECP_PF_UNCOMPRESSED,
                                            &length, buffer.data(), buffer.size());
    if (ret != 0) {
        std::cerr << "Failed to export public key" << std::endl;
        return std::vector<uint8_t>();
    }

    buffer.resize(length);
    return buffer;
}

std::optional<std::vector<uint8_t>> KeyPair::sign(const std::vector<uint8_t>& message) const {
    if (!hasPrivateKey_) {
        return std::nullopt;
    }

    // Calculate SHA256 of the message
    unsigned char hash[32];
    mbedtls_sha256_context sha256_ctx;
    mbedtls_sha256_init(&sha256_ctx);
    mbedtls_sha256_starts(&sha256_ctx, 0); // 0 for SHA256
    mbedtls_sha256_update(&sha256_ctx, message.data(), message.size());
    mbedtls_sha256_finish(&sha256_ctx, hash);
    mbedtls_sha256_free(&sha256_ctx);

    // Sign the hash
    std::vector<uint8_t> signature(MBEDTLS_ECDSA_MAX_LEN);
    size_t signatureLen;

    int ret = mbedtls_pk_sign(ctx_, MBEDTLS_MD_SHA256,
                             hash, sizeof(hash),
                             signature.data(), signature.size(), &signatureLen,
                             mbedtls_ctr_drbg_random, ctr_drbg_);

    if (ret != 0) {
        return std::nullopt;
    }

    signature.resize(signatureLen);
    return signature;
}

bool KeyPair::verify(const std::vector<uint8_t>& message, const std::vector<uint8_t>& signature) const {
    if (!hasPublicKey_) {
        return false;
    }

    // Calculate SHA256 of the message
    unsigned char hash[32];
    mbedtls_sha256_context sha256_ctx;
    mbedtls_sha256_init(&sha256_ctx);
    mbedtls_sha256_starts(&sha256_ctx, 0); // 0 for SHA256
    mbedtls_sha256_update(&sha256_ctx, message.data(), message.size());
    mbedtls_sha256_finish(&sha256_ctx, hash);
    mbedtls_sha256_free(&sha256_ctx);

    int ret = mbedtls_pk_verify(ctx_, MBEDTLS_MD_SHA256,
                               hash, sizeof(hash),
                               signature.data(), signature.size());

    return ret == 0;
}

std::string KeyPair::toHex(const std::vector<uint8_t>& data) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (uint8_t byte : data) {
        ss << std::setw(2) << static_cast<int>(byte);
    }
    return ss.str();
}

std::vector<uint8_t> KeyPair::fromHex(const std::string& hex) {
    std::vector<uint8_t> data;
    for (size_t i = 0; i < hex.length(); i += 2) {
        std::string byteString = hex.substr(i, 2);
        uint8_t byte = static_cast<uint8_t>(std::stoi(byteString, nullptr, 16));
        data.push_back(byte);
    }
    return data;
}

std::string KeyPair::toBase64(const std::vector<uint8_t>& data) {
    size_t outputLen;
    std::vector<uint8_t> output((data.size() * 4 / 3) + 4); // Add padding space

    mbedtls_base64_encode(output.data(), output.size(), &outputLen,
                         data.data(), data.size());

    return std::string(reinterpret_cast<char*>(output.data()), outputLen);
}

std::vector<uint8_t> KeyPair::fromBase64(const std::string& base64) {
    size_t outputLen;
    std::vector<uint8_t> output((base64.length() * 3 / 4) + 1);

    mbedtls_base64_decode(output.data(), output.size(), &outputLen,
                         reinterpret_cast<const uint8_t*>(base64.c_str()),
                         base64.length());

    output.resize(outputLen);
    return output;
}

// VerifiableCredential implementation
VerifiableCredential::VerifiableCredential()
    : context_("https://www.w3.org/2018/credentials/v1")
    , type_("VerifiableCredential")
    , issuanceDate_(time(nullptr))
    , expirationDate_(0) {
}

void VerifiableCredential::setContext(const std::string& context) {
    context_ = context;
}

void VerifiableCredential::setType(const std::string& type) {
    type_ = type;
}

void VerifiableCredential::setIssuer(const std::string& issuer) {
    issuer_ = issuer;
}

void VerifiableCredential::setIssuanceDate(const std::string& date) {
    struct tm tm = {};
    strptime(date.c_str(), "%Y-%m-%dT%H:%M:%SZ", &tm);
    tm.tm_isdst = 0;  // No DST
    issuanceDate_ = timegm(&tm);  // Use timegm for UTC
}

void VerifiableCredential::setExpirationDate(const std::string& date) {
    struct tm tm = {};
    strptime(date.c_str(), "%Y-%m-%dT%H:%M:%SZ", &tm);
    tm.tm_isdst = 0;  // No DST
    expirationDate_ = timegm(&tm);  // Use timegm for UTC
}

void VerifiableCredential::setSubject(const std::string& id, const std::string& name) {
    subjectId_ = id;
    subjectName_ = name;
}

void VerifiableCredential::addCapability(const std::string& capability) {
    capabilities_.push_back(capability);
}

bool VerifiableCredential::hasCapability(const std::string& capability) const {
    return std::find(capabilities_.begin(), capabilities_.end(), capability) != capabilities_.end();
}

bool VerifiableCredential::isExpired() const {
    if (expirationDate_ == 0) return false;  // No expiration
    return time(nullptr) > expirationDate_;
}

std::string VerifiableCredential::getCanonicalForm() const {
#ifdef ARDUINO
    DynamicJsonDocument doc(4096);
    doc["@context"] = context_;
    doc["type"] = type_;
    doc["issuer"] = issuer_;
    doc["issuanceDate"] = formatDate(issuanceDate_);
    if (expirationDate_ > 0) {
        doc["expirationDate"] = formatDate(expirationDate_);
    }

    JsonObject subject = doc.createNestedObject("credentialSubject");
    subject["id"] = subjectId_;
    subject["name"] = subjectName_;
    
    if (!capabilities_.empty()) {
        JsonArray caps = subject.createNestedArray("capabilities");
        for (const auto& cap : capabilities_) {
            caps.add(cap);
        }
    }

    String output;
    serializeJson(doc, output);
    return std::string(output.c_str());
#else
    json j;
    j["@context"] = context_;
    j["type"] = type_;
    j["issuer"] = issuer_;
    j["issuanceDate"] = formatDate(issuanceDate_);
    if (expirationDate_ > 0) {
        j["expirationDate"] = formatDate(expirationDate_);
    }

    j["credentialSubject"] = {
        {"id", subjectId_},
        {"name", subjectName_}
    };

    if (!capabilities_.empty()) {
        j["credentialSubject"]["capabilities"] = capabilities_;
    }

    return j.dump();
#endif
}

bool VerifiableCredential::sign(const KeyPair& issuerKeys) {
    if (isExpired()) {
        std::cerr << "Credential is expired" << std::endl;
        return false;
    }

    std::string canonicalForm = getCanonicalForm();
    std::cout << "Signing message: " << canonicalForm << std::endl;

    std::vector<uint8_t> message(canonicalForm.begin(), canonicalForm.end());
    auto signatureOpt = issuerKeys.sign(message);
    if (!signatureOpt) {
        std::cerr << "Failed to sign credential" << std::endl;
        return false;
    }

    signature_ = *signatureOpt;
    std::cout << "Generated signature (hex): " << KeyPair::toHex(signature_) << std::endl;
    return true;
}

bool VerifiableCredential::verify(const std::vector<uint8_t>& issuerPublicKey) const {
    if (signature_.empty() || isExpired()) {
        std::cerr << "Invalid credential state" << std::endl;
        return false;
    }

    std::string canonicalForm = getCanonicalForm();
    std::cout << "Verifying signature for message: " << canonicalForm << std::endl;
    std::cout << "Signature (hex): " << KeyPair::toHex(signature_) << std::endl;
    std::cout << "Public key (hex): " << KeyPair::toHex(issuerPublicKey) << std::endl;

    std::vector<uint8_t> message(canonicalForm.begin(), canonicalForm.end());

    // Create temporary KeyPair for verification
    KeyPair verifier;
    if (!verifier.importPublicKey(issuerPublicKey)) {
        std::cerr << "Failed to import issuer public key" << std::endl;
        return false;
    }

    return verifier.verify(message, signature_);
}

std::string VerifiableCredential::toJson() const {
#ifdef ARDUINO
    DynamicJsonDocument doc(4096);
    
    doc["@context"] = context_;
    doc["type"] = type_;
    doc["issuer"] = issuer_;
    doc["issuanceDate"] = formatDate(issuanceDate_);
    if (expirationDate_ > 0) {
        doc["expirationDate"] = formatDate(expirationDate_);
    }

    JsonObject subject = doc.createNestedObject("credentialSubject");
    subject["id"] = subjectId_;
    subject["name"] = subjectName_;
    
    if (!capabilities_.empty()) {
        JsonArray caps = subject.createNestedArray("capabilities");
        for (const auto& cap : capabilities_) {
            caps.add(cap);
        }
    }

    JsonObject proof = doc.createNestedObject("proof");
    proof["type"] = "EcdsaSecp256r1Signature2019";
    proof["created"] = formatDate(issuanceDate_);
    proof["proofPurpose"] = "assertionMethod";
    proof["verificationMethod"] = issuer_ + "#key-1";
    proof["proofValue"] = KeyPair::toBase64(signature_);

    String output;
    serializeJson(doc, output);
    return std::string(output.c_str());
#else
    json j;
    j["@context"] = context_;
    j["type"] = type_;
    j["issuer"] = issuer_;
    j["issuanceDate"] = formatDate(issuanceDate_);
    if (expirationDate_ > 0) {
        j["expirationDate"] = formatDate(expirationDate_);
    }

    j["credentialSubject"] = {
        {"id", subjectId_},
        {"name", subjectName_}
    };

    if (!capabilities_.empty()) {
        j["credentialSubject"]["capabilities"] = capabilities_;
    }

    j["proof"] = {
        {"type", "EcdsaSecp256r1Signature2019"},
        {"created", formatDate(issuanceDate_)},
        {"proofPurpose", "assertionMethod"},
        {"verificationMethod", issuer_ + "#key-1"},
        {"proofValue", KeyPair::toBase64(signature_)}
    };

    return j.dump();
#endif
}

std::optional<VerifiableCredential> VerifiableCredential::fromJson(const std::string& json) {
    auto vc = std::make_optional<VerifiableCredential>();

#ifdef ARDUINO
    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, json);
    
    if (error) {
        std::cerr << "Failed to parse JSON" << std::endl;
        return std::nullopt;
    }
    
    // Parse basic fields
    vc->setContext(doc["@context"] | "https://www.w3.org/2018/credentials/v1");
    vc->setType(doc["type"] | "VerifiableCredential");
    vc->setIssuer(doc["issuer"] | "");
    vc->setIssuanceDate(doc["issuanceDate"] | "");
    if (doc.containsKey("expirationDate")) {
        vc->setExpirationDate(doc["expirationDate"]);
    }

    // Parse subject
    JsonObject subject = doc["credentialSubject"];
    if (!subject.isNull()) {
        vc->setSubject(subject["id"] | "", subject["name"] | "");
        
        // Parse capabilities
        JsonArray caps = subject["capabilities"];
        if (!caps.isNull()) {
            for (JsonVariant cap : caps) {
                vc->addCapability(cap.as<std::string>());
            }
        }
    }

    // Parse proof
    JsonObject proof = doc["proof"];
    if (!proof.isNull()) {
        std::string proofValue = proof["proofValue"] | "";
        if (!proofValue.empty()) {
            vc->signature_ = KeyPair::fromBase64(proofValue);
        }
    }
#else
    try {
        auto j = json::parse(json);
        
        // Parse basic fields
        vc->setContext(j.value("@context", "https://www.w3.org/2018/credentials/v1"));
        vc->setType(j.value("type", "VerifiableCredential"));
        vc->setIssuer(j.value("issuer", ""));
        vc->setIssuanceDate(j.value("issuanceDate", ""));
        if (j.contains("expirationDate")) {
            vc->setExpirationDate(j["expirationDate"]);
        }

        // Parse subject
        if (j.contains("credentialSubject")) {
            const auto& subject = j["credentialSubject"];
            vc->setSubject(
                subject.value("id", ""),
                subject.value("name", "")
            );

            // Parse capabilities
            if (subject.contains("capabilities")) {
                for (const auto& cap : subject["capabilities"]) {
                    vc->addCapability(cap);
                }
            }
        }

        // Parse proof
        if (j.contains("proof")) {
            const auto& proof = j["proof"];
            if (proof.contains("proofValue")) {
                vc->signature_ = KeyPair::fromBase64(proof["proofValue"]);
            }
        }
    } catch (const json::exception& e) {
        std::cerr << "Failed to parse JSON: " << e.what() << std::endl;
        return std::nullopt;
    }
#endif

    return vc;
}

std::string VerifiableCredential::formatDate(time_t timestamp) const {
    char buf[30];
    struct tm* tm = gmtime(&timestamp);
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", tm);
    return std::string(buf);
}

} // namespace crypto
} // namespace one 