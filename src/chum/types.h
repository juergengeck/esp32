#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <Arduino.h>

namespace chum {

// Forward declarations
struct KeyTrustInfo;
struct CertificateData;

enum class ConnectionState {
    NotConnected,
    Connecting,
    Connected,
    Disconnecting
};

enum class MessageType : uint8_t {
    Discovery,      // Peer discovery message
    KeyExchange,    // Key exchange message
    CertificateSync, // Certificate synchronization
    ProfileSync,    // Profile synchronization
    Data,           // Application data
    Ack             // Message acknowledgment
};

struct Message {
    std::string sender;               // Sender's ID
    std::string recipient;            // Recipient's ID (empty for broadcast)
    uint32_t sequence;                // Message sequence number
    MessageType type;                 // Type of message
    std::vector<uint8_t> payload;     // Message payload
    std::vector<uint8_t> signature;   // Message signature
    uint64_t timestamp;               // Message timestamp
};

struct StorageMetrics {
    uint32_t totalBytes;
    uint32_t usedBytes;
    uint32_t freeBytes;
};

struct NodeIdentity {
    std::string instanceId;
    std::string nodeType;
    uint32_t capabilities;
    StorageMetrics storage;
    uint32_t uptime;
    uint8_t status;
};

struct Signature {
    std::vector<uint8_t> signature;  // The actual signature bytes
    std::string signer;              // Signer identifier
    std::vector<uint8_t> data;       // Data that was signed
};

struct CertificateData {
    std::string id;                   // Certificate identifier
    std::vector<uint8_t> certificate; // Certificate data
    std::vector<uint8_t> signature;   // Signature data
    uint64_t timestamp;               // Timestamp
    bool trusted;                     // Trust status
    std::string certificateHash;      // Hash of certificate
    std::string signatureHash;        // Hash of signature
    std::shared_ptr<KeyTrustInfo> keyTrustInfo; // Trust info for the key
};

struct KeyTrustInfo {
    std::string keyId;                // Key identifier
    bool trusted;                     // Trust status
    std::string reason;               // Reason for trust status
};

struct ProfileData {
    std::string id;                   // Unique identifier
    std::string personId;             // Person identifier
    std::string owner;                // Owner identifier
    std::string profileId;            // Profile identifier
    std::string profileHash;          // Hash of the profile
    std::vector<std::string> keys;    // Associated keys
    std::vector<uint8_t> certificate; // Certificate data
    std::vector<uint8_t> signature;   // Signature data
    uint64_t timestamp;               // Last update timestamp
    std::vector<CertificateData> certificates; // Associated certificates
};

enum class CertificateType {
    Affirmation,              // Basic trust assertion
    TrustKeys,                // Key trust declaration
    RightToDeclareTrustedKeysForEverybody,  // Global trust authority
    RightToDeclareTrustedKeysForSelf        // Self-trust authority
};

enum class RootKeyMode {
    MainId,  // Only main identity keys
    All     // All identity keys
};

// Rights structure for person rights map
struct PersonRights {
    bool rightToDeclareTrustedKeysForEverybody;
    bool rightToDeclareTrustedKeysForSelf;
};

// Helper function to convert Arduino String to std::string
inline std::string toString(const String& arduinoStr) {
    return std::string(arduinoStr.c_str());
}

// Helper function to convert std::string to Arduino String
inline String toArduinoString(const std::string& stdStr) {
    return String(stdStr.c_str());
}

} // namespace chum 