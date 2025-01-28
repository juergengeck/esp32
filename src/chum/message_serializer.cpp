#include "message_serializer.h"
#include <sstream>
#include <iomanip>
#include <Base64.h>
#include <ArduinoJson.h>

namespace chum {

std::vector<uint8_t> MessageSerializer::serializeMessage(const Message& msg) {
    StaticJsonDocument<JSON_BUFFER_SIZE> doc;
    
    doc["sender"] = msg.sender;
    doc["recipient"] = msg.recipient;
    doc["sequence"] = msg.sequence;
    doc["type"] = static_cast<uint8_t>(msg.type);
    doc["timestamp"] = msg.timestamp;
    
    std::string payloadBase64;
    size_t encodedLength = Base64.encodedLength(msg.payload.size());
    payloadBase64.resize(encodedLength);
    Base64.encode((char*)payloadBase64.data(), (char*)msg.payload.data(), msg.payload.size());
    doc["payload"] = payloadBase64;
    
    if (!msg.signature.empty()) {
        std::string signatureBase64;
        encodedLength = Base64.encodedLength(msg.signature.size());
        signatureBase64.resize(encodedLength);
        Base64.encode((char*)signatureBase64.data(), (char*)msg.signature.data(), msg.signature.size());
        doc["signature"] = signatureBase64;
    }
    
    std::string output;
    serializeJson(doc, output);
    return std::vector<uint8_t>(output.begin(), output.end());
}

std::optional<Message> MessageSerializer::deserializeMessage(const uint8_t* data, size_t len) {
    StaticJsonDocument<JSON_BUFFER_SIZE> doc;
    DeserializationError error = deserializeJson(doc, data, len);
    
    if (error) {
        return std::nullopt;
    }
    
    Message msg;
    msg.sender = doc["sender"].as<std::string>();
    msg.recipient = doc["recipient"].as<std::string>();
    msg.sequence = doc["sequence"].as<uint32_t>();
    msg.type = static_cast<MessageType>(doc["type"].as<uint8_t>());
    msg.timestamp = doc["timestamp"].as<uint64_t>();
    
    if (doc.containsKey("payload")) {
        std::string payloadBase64 = doc["payload"].as<std::string>();
        size_t decodedLength = Base64.decodedLength((char*)payloadBase64.c_str(), payloadBase64.length());
        msg.payload.resize(decodedLength);
        Base64.decode((char*)msg.payload.data(), (char*)payloadBase64.c_str(), payloadBase64.length());
    }
    
    if (doc.containsKey("signature")) {
        std::string signatureBase64 = doc["signature"].as<std::string>();
        size_t decodedLength = Base64.decodedLength((char*)signatureBase64.c_str(), signatureBase64.length());
        msg.signature.resize(decodedLength);
        Base64.decode((char*)msg.signature.data(), (char*)signatureBase64.c_str(), signatureBase64.length());
    }
    
    return msg;
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

std::vector<uint8_t> MessageSerializer::serializeProfile(const ProfileData& profile) {
    StaticJsonDocument<JSON_BUFFER_SIZE> doc;
    
    doc["id"] = profile.id;
    doc["personId"] = profile.personId;
    doc["owner"] = profile.owner;
    doc["profileId"] = profile.profileId;
    doc["profileHash"] = profile.profileHash;
    doc["timestamp"] = profile.timestamp;
    
    JsonArray keysArray = doc.createNestedArray("keys");
    for (const auto& key : profile.keys) {
        keysArray.add(key);
    }
    
    // Encode certificate and signature as base64
    std::string certBase64;
    size_t certEncodedLength = Base64.encodedLength(profile.certificate.size());
    certBase64.resize(certEncodedLength);
    Base64.encode((char*)certBase64.data(), (char*)profile.certificate.data(), profile.certificate.size());
    doc["certificate"] = certBase64;
    
    std::string sigBase64;
    size_t sigEncodedLength = Base64.encodedLength(profile.signature.size());
    sigBase64.resize(sigEncodedLength);
    Base64.encode((char*)sigBase64.data(), (char*)profile.signature.data(), profile.signature.size());
    doc["signature"] = sigBase64;
    
    std::string output;
    serializeJson(doc, output);
    return std::vector<uint8_t>(output.begin(), output.end());
}

std::optional<ProfileData> MessageSerializer::deserializeProfile(const uint8_t* data, size_t len) {
    StaticJsonDocument<JSON_BUFFER_SIZE> doc;
    DeserializationError error = deserializeJson(doc, data, len);
    
    if (error) {
        return std::nullopt;
    }
    
    ProfileData profile;
    profile.id = doc["id"].as<std::string>();
    profile.personId = doc["personId"].as<std::string>();
    profile.owner = doc["owner"].as<std::string>();
    profile.profileId = doc["profileId"].as<std::string>();
    profile.profileHash = doc["profileHash"].as<std::string>();
    profile.timestamp = doc["timestamp"].as<uint64_t>();
    
    JsonArray keysArray = doc["keys"];
    for (const auto& key : keysArray) {
        profile.keys.push_back(key.as<std::string>());
    }
    
    // Decode base64 certificate and signature
    std::string certBase64 = doc["certificate"].as<std::string>();
    size_t certDecodedLength = Base64.decodedLength((char*)certBase64.c_str(), certBase64.length());
    profile.certificate.resize(certDecodedLength);
    Base64.decode((char*)profile.certificate.data(), (char*)certBase64.c_str(), certBase64.length());
    
    std::string sigBase64 = doc["signature"].as<std::string>();
    size_t sigDecodedLength = Base64.decodedLength((char*)sigBase64.c_str(), sigBase64.length());
    profile.signature.resize(sigDecodedLength);
    Base64.decode((char*)profile.signature.data(), (char*)sigBase64.c_str(), sigBase64.length());
    
    return profile;
}

std::vector<uint8_t> MessageSerializer::serializeCertificates(const std::vector<CertificateData>& certs) {
    StaticJsonDocument<JSON_BUFFER_SIZE> doc;
    JsonArray certsArray = doc.createNestedArray("certificates");
    
    for (const auto& cert : certs) {
        JsonObject certObj = certsArray.createNestedObject();
        certObj["id"] = cert.id;
        
        std::string certBase64;
        size_t encodedLength = Base64.encodedLength(cert.certificate.size());
        certBase64.resize(encodedLength);
        Base64.encode((char*)certBase64.data(), (char*)cert.certificate.data(), cert.certificate.size());
        certObj["certificate"] = certBase64;
        
        std::string sigBase64;
        encodedLength = Base64.encodedLength(cert.signature.size());
        sigBase64.resize(encodedLength);
        Base64.encode((char*)sigBase64.data(), (char*)cert.signature.data(), cert.signature.size());
        certObj["signature"] = sigBase64;
        
        certObj["timestamp"] = cert.timestamp;
    }
    
    std::string output;
    serializeJson(doc, output);
    return std::vector<uint8_t>(output.begin(), output.end());
}

std::optional<std::vector<CertificateData>> MessageSerializer::deserializeCertificates(const uint8_t* data, size_t len) {
    StaticJsonDocument<JSON_BUFFER_SIZE> doc;
    DeserializationError error = deserializeJson(doc, data, len);
    
    if (error) {
        return std::nullopt;
    }
    
    std::vector<CertificateData> certs;
    JsonArray certsArray = doc["certificates"].as<JsonArray>();
    
    for (JsonObject certObj : certsArray) {
        CertificateData cert;
        cert.id = certObj["id"].as<std::string>();
        
        std::string certBase64 = certObj["certificate"].as<std::string>();
        size_t decodedLength = Base64.decodedLength((char*)certBase64.c_str(), certBase64.length());
        cert.certificate.resize(decodedLength);
        Base64.decode((char*)cert.certificate.data(), (char*)certBase64.c_str(), certBase64.length());
        
        std::string sigBase64 = certObj["signature"].as<std::string>();
        decodedLength = Base64.decodedLength((char*)sigBase64.c_str(), sigBase64.length());
        cert.signature.resize(decodedLength);
        Base64.decode((char*)cert.signature.data(), (char*)sigBase64.c_str(), sigBase64.length());
        
        cert.timestamp = certObj["timestamp"].as<uint64_t>();
        certs.push_back(cert);
    }
    
    return certs;
}

} // namespace chum 