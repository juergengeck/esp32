#pragma once

#include <string>
#include <vector>
#include <optional>
#include <ArduinoJson.h>
#include "types.h"

namespace chum {

class MessageSerializer {
public:
    // Message serialization
    static std::vector<uint8_t> serializeMessage(const Message& msg);
    static std::optional<Message> deserializeMessage(const uint8_t* data, size_t len);

    // Profile serialization
    static std::vector<uint8_t> serializeProfile(const ProfileData& profile);
    static std::optional<ProfileData> deserializeProfile(const uint8_t* data, size_t len);

    // Certificate serialization
    static std::vector<uint8_t> serializeCertificates(const std::vector<CertificateData>& certs);
    static std::optional<std::vector<CertificateData>> deserializeCertificates(const uint8_t* data, size_t len);

    static void macToString(const uint8_t* mac, std::string& str);
    static void stringToMac(const std::string& str, uint8_t* mac);

private:
    // Helper methods
    static constexpr size_t JSON_BUFFER_SIZE = 512;
    
    template<typename T>
    static std::vector<uint8_t> serializeVector(const std::vector<T>& vec);
    
    template<typename T>
    static std::optional<std::vector<T>> deserializeVector(const uint8_t* data, size_t len);
};

} // namespace chum 