#include "message_serializer.h"
#include <sstream>
#include <iomanip>
#include <Base64.h>
#include <ArduinoJson.h>
#include "mbedtls/base64.h"

namespace chum {

std::vector<unsigned char> MessageSerializer::serializeMessage(const Message& msg) {
    DynamicJsonDocument doc(2048);
    doc["sender"] = msg.sender.c_str();
    doc["recipient"] = msg.recipient.c_str();
    doc["sequence"] = msg.sequence;
    doc["type"] = static_cast<int>(msg.type);
    doc["timestamp"] = msg.timestamp;
    
    // Base64 encode payload
    size_t encodedLength;
    std::vector<unsigned char> encodedPayload(msg.payload.size() * 2); // Ensure enough space
    mbedtls_base64_encode(encodedPayload.data(), encodedPayload.size(), &encodedLength,
                         msg.payload.data(), msg.payload.size());
    doc["payload"] = String((char*)encodedPayload.data());
    
    if (!msg.signature.empty()) {
        std::vector<unsigned char> encodedSig(msg.signature.size() * 2);
        size_t sigLength;
        mbedtls_base64_encode(encodedSig.data(), encodedSig.size(), &sigLength,
                             msg.signature.data(), msg.signature.size());
        doc["signature"] = String((char*)encodedSig.data());
    }
    
    String output;
    serializeJson(doc, output);
    
    return std::vector<unsigned char>(output.begin(), output.end());
}

std::optional<Message> MessageSerializer::deserializeMessage(const uint8_t* data, size_t length) {
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, data, length);
    
    if (error) {
        return std::nullopt;
    }
    
    Message msg;
    const char* sender = doc["sender"] | "";
    const char* recipient = doc["recipient"] | "";
    msg.sender = std::string(sender);
    msg.recipient = std::string(recipient);
    msg.sequence = doc["sequence"] | 0;
    msg.type = static_cast<MessageType>(doc["type"] | 0);
    msg.timestamp = doc["timestamp"] | 0;
    
    // Decode payload from base64
    const char* payloadBase64 = doc["payload"] | "";
    if (strlen(payloadBase64) > 0) {
        size_t decodedLength;
        std::vector<unsigned char> decodedPayload(strlen(payloadBase64));
        mbedtls_base64_decode(decodedPayload.data(), decodedPayload.size(), &decodedLength,
                             (const unsigned char*)payloadBase64, strlen(payloadBase64));
        msg.payload = std::vector<unsigned char>(decodedPayload.begin(), 
                                               decodedPayload.begin() + decodedLength);
    }
    
    // Decode signature if present
    if (doc.containsKey("signature")) {
        const char* signatureBase64 = doc["signature"] | "";
        if (strlen(signatureBase64) > 0) {
            size_t decodedLength;
            std::vector<unsigned char> decodedSig(strlen(signatureBase64));
            mbedtls_base64_decode(decodedSig.data(), decodedSig.size(), &decodedLength,
                                 (const unsigned char*)signatureBase64, strlen(signatureBase64));
            msg.signature = std::vector<unsigned char>(decodedSig.begin(),
                                                     decodedSig.begin() + decodedLength);
        }
    }
    
    return msg;
}

std::vector<unsigned char> MessageSerializer::serializeProfile(const ProfileData& profile) {
    DynamicJsonDocument doc(4096);
    doc["id"] = profile.id.c_str();
    doc["personId"] = profile.personId.c_str();
    doc["owner"] = profile.owner.c_str();
    doc["profileId"] = profile.profileId.c_str();
    doc["profileHash"] = profile.profileHash.c_str();
    
    JsonArray keysArray = doc.createNestedArray("keys");
    for (const auto& key : profile.keys) {
        keysArray.add(key.c_str());
    }
    
    // Base64 encode certificate
    size_t certEncodedLength;
    std::vector<unsigned char> encodedCert(profile.certificate.size() * 2);
    mbedtls_base64_encode(encodedCert.data(), encodedCert.size(), &certEncodedLength,
                         profile.certificate.data(), profile.certificate.size());
    doc["certificate"] = String((char*)encodedCert.data());
    
    // Base64 encode signature
    if (!profile.signature.empty()) {
        std::vector<unsigned char> encodedSig(profile.signature.size() * 2);
        size_t sigLength;
        mbedtls_base64_encode(encodedSig.data(), encodedSig.size(), &sigLength,
                             profile.signature.data(), profile.signature.size());
        doc["signature"] = String((char*)encodedSig.data());
    }
    
    doc["timestamp"] = profile.timestamp;
    
    JsonArray certsArray = doc.createNestedArray("certificates");
    for (const auto& cert : profile.certificates) {
        JsonObject certObj = certsArray.createNestedObject();
        certObj["id"] = cert.id.c_str();
        
        // Base64 encode certificate
        size_t encodedLength;
        std::vector<unsigned char> encodedCert(cert.certificate.size() * 2);
        mbedtls_base64_encode(encodedCert.data(), encodedCert.size(), &encodedLength,
                             cert.certificate.data(), cert.certificate.size());
        certObj["certificate"] = String((char*)encodedCert.data());
    }
    
    String output;
    serializeJson(doc, output);
    
    return std::vector<unsigned char>(output.begin(), output.end());
}

std::optional<ProfileData> MessageSerializer::deserializeProfile(const uint8_t* data, size_t length) {
    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, data, length);
    
    if (error) {
        return std::nullopt;
    }
    
    ProfileData profile;
    const char* id = doc["id"] | "";
    const char* personId = doc["personId"] | "";
    const char* owner = doc["owner"] | "";
    const char* profileId = doc["profileId"] | "";
    const char* profileHash = doc["profileHash"] | "";
    
    profile.id = std::string(id);
    profile.personId = std::string(personId);
    profile.owner = std::string(owner);
    profile.profileId = std::string(profileId);
    profile.profileHash = std::string(profileHash);
    
    JsonArray keysArray = doc["keys"].as<JsonArray>();
    for (JsonVariant key : keysArray) {
        const char* keyStr = key | "";
        profile.keys.push_back(std::string(keyStr));
    }
    
    // Decode certificate from base64
    const char* certBase64 = doc["certificate"] | "";
    if (strlen(certBase64) > 0) {
        size_t decodedLength;
        std::vector<unsigned char> decodedCert(strlen(certBase64));
        mbedtls_base64_decode(decodedCert.data(), decodedCert.size(), &decodedLength,
                             (const unsigned char*)certBase64, strlen(certBase64));
        profile.certificate = std::vector<unsigned char>(decodedCert.begin(),
                                                       decodedCert.begin() + decodedLength);
    }
    
    // Decode signature if present
    if (doc.containsKey("signature")) {
        const char* signatureBase64 = doc["signature"] | "";
        if (strlen(signatureBase64) > 0) {
            size_t decodedLength;
            std::vector<unsigned char> decodedSig(strlen(signatureBase64));
            mbedtls_base64_decode(decodedSig.data(), decodedSig.size(), &decodedLength,
                                 (const unsigned char*)signatureBase64, strlen(signatureBase64));
            profile.signature = std::vector<unsigned char>(decodedSig.begin(),
                                                         decodedSig.begin() + decodedLength);
        }
    }
    
    profile.timestamp = doc["timestamp"] | 0ULL;
    
    JsonArray certsArray = doc["certificates"].as<JsonArray>();
    for (JsonObject certObj : certsArray) {
        CertificateData cert;
        const char* certId = certObj["id"] | "";
        cert.id = std::string(certId);
        
        // Decode certificate from base64
        const char* certBase64 = certObj["certificate"] | "";
        if (strlen(certBase64) > 0) {
            size_t decodedLength;
            std::vector<unsigned char> decodedCert(strlen(certBase64));
            mbedtls_base64_decode(decodedCert.data(), decodedCert.size(), &decodedLength,
                                 (const unsigned char*)certBase64, strlen(certBase64));
            cert.certificate = std::vector<unsigned char>(decodedCert.begin(),
                                                        decodedCert.begin() + decodedLength);
        }
        
        profile.certificates.push_back(cert);
    }
    
    return profile;
}

// Helper functions
String MessageSerializer::toArduinoString(const std::string& str) {
    return String(str.c_str());
}

std::string MessageSerializer::toString(const String& str) {
    return std::string(str.c_str());
}

void MessageSerializer::macToString(const uint8_t* mac, std::string& str) {
    std::stringstream ss;
    for (int i = 0; i < 6; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(mac[i]);
        if (i < 5) ss << ":";
    }
    str = ss.str();
}

void MessageSerializer::stringToMac(const std::string& str, uint8_t* mac) {
    std::stringstream ss(str);
    std::string octet;
    int i = 0;
    
    while (std::getline(ss, octet, ':') && i < 6) {
        mac[i++] = static_cast<uint8_t>(std::stoi(octet, nullptr, 16));
    }
}

std::vector<unsigned char> MessageSerializer::serializeCertificates(const std::vector<CertificateData>& certs) {
    DynamicJsonDocument doc(8192);
    JsonArray certsArray = doc.createNestedArray("certificates");
    
    for (const auto& cert : certs) {
        JsonObject certObj = certsArray.createNestedObject();
        certObj["id"] = cert.id;
        
        // Base64 encode certificate
        size_t encodedLength;
        std::vector<unsigned char> encodedCert(cert.certificate.size() * 2);
        mbedtls_base64_encode(encodedCert.data(), encodedCert.size(), &encodedLength,
                             cert.certificate.data(), cert.certificate.size());
        certObj["certificate"] = String((char*)encodedCert.data());
    }
    
    String output;
    serializeJson(doc, output);
    
    return std::vector<unsigned char>(output.begin(), output.end());
}

std::optional<std::vector<CertificateData>> MessageSerializer::deserializeCertificates(const uint8_t* data, size_t length) {
    DynamicJsonDocument doc(8192);
    DeserializationError error = deserializeJson(doc, data, length);
    
    if (error) {
        return std::nullopt;
    }
    
    std::vector<CertificateData> certs;
    JsonArray certsArray = doc["certificates"].as<JsonArray>();
    
    for (JsonObject certObj : certsArray) {
        CertificateData cert;
        cert.id = certObj["id"].as<String>().c_str();
        
        // Decode certificate from base64
        String certBase64 = certObj["certificate"].as<String>();
        size_t decodedLength;
        std::vector<unsigned char> decodedCert(certBase64.length());
        mbedtls_base64_decode(decodedCert.data(), decodedCert.size(), &decodedLength,
                             (const unsigned char*)certBase64.c_str(), certBase64.length());
        cert.certificate = std::vector<unsigned char>(decodedCert.begin(),
                                                    decodedCert.begin() + decodedLength);
        
        certs.push_back(cert);
    }
    
    return certs;
}

} // namespace chum