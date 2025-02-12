#include "credential.h"
#include <esp_log.h>
#include <sstream>
#include <iomanip>
#include <mbedtls/pk.h>

namespace one {
namespace instance {

static const char* TAG = "Credential";

Credential::Credential(InstanceKeys& issuerKeys)
    : issuerKeys_(issuerKeys)
    , issuedAt_(0)
    , expiry_(0)
    , isIssued_(false) {
    memset(signature_, 0, SIGNATURE_SIZE);
}

void Credential::addClaim(const std::string& type, const std::string& value) {
    claims_.push_back({type, value});
}

void Credential::generateMessage(uint8_t* message, size_t* messageLen) const {
    // Create JSON document for the credential
    DynamicJsonDocument doc(4096);
    
    doc["subject"] = subject_;
    doc["issuedAt"] = issuedAt_;
    doc["expiry"] = expiry_;
    
    JsonArray claimsArray = doc.createNestedArray("claims");
    for (const auto& claim : claims_) {
        JsonObject claimObj = claimsArray.createNestedObject();
        claimObj["type"] = claim.type;
        claimObj["value"] = claim.value;
    }

    // Serialize to string
    String jsonStr;
    serializeJson(doc, jsonStr);
    
    // Copy to message buffer
    memcpy(message, jsonStr.c_str(), jsonStr.length());
    *messageLen = jsonStr.length();
}

bool Credential::issue() {
    if (isIssued_) {
        ESP_LOGE(TAG, "Credential already issued");
        return false;
    }

    if (subject_.empty()) {
        ESP_LOGE(TAG, "Subject not set");
        return false;
    }

    if (claims_.empty()) {
        ESP_LOGE(TAG, "No claims added");
        return false;
    }

    // Set issuedAt if not set
    if (issuedAt_ == 0) {
        issuedAt_ = time(nullptr);
    }

    // Generate message to sign
    uint8_t message[4096];
    size_t messageLen;
    generateMessage(message, &messageLen);

    // Sign the message
    size_t signatureLen = SIGNATURE_SIZE;
    if (!issuerKeys_.sign(message, messageLen, signature_, &signatureLen)) {
        ESP_LOGE(TAG, "Failed to sign credential");
        return false;
    }

    isIssued_ = true;
    ESP_LOGI(TAG, "Credential issued successfully");
    return true;
}

bool Credential::verify(const uint8_t* issuerPublicKey) const {
    if (!isIssued_) {
        ESP_LOGE(TAG, "Credential not issued");
        return false;
    }

    // Check expiry
    if (expiry_ > 0 && time(nullptr) > expiry_) {
        ESP_LOGE(TAG, "Credential expired");
        return false;
    }

    // Generate message that was signed
    uint8_t message[4096];
    size_t messageLen;
    generateMessage(message, &messageLen);

    // Create temporary InstanceKeys for verification
    InstanceKeys verifyKeys;
    memcpy(const_cast<uint8_t*>(verifyKeys.getPublicKey()), 
           issuerPublicKey, InstanceKeys::KEY_SIZE);

    // Verify signature
    if (!verifyKeys.verify(message, messageLen, signature_, SIGNATURE_SIZE)) {
        ESP_LOGE(TAG, "Signature verification failed");
        return false;
    }

    ESP_LOGI(TAG, "Credential verified successfully");
    return true;
}

String Credential::serialize() const {
    DynamicJsonDocument doc(4096);
    
    doc["subject"] = subject_;
    doc["issuedAt"] = issuedAt_;
    doc["expiry"] = expiry_;
    doc["isIssued"] = isIssued_;
    
    JsonArray claimsArray = doc.createNestedArray("claims");
    for (const auto& claim : claims_) {
        JsonObject claimObj = claimsArray.createNestedObject();
        claimObj["type"] = claim.type;
        claimObj["value"] = claim.value;
    }

    // Convert signature to hex string
    std::stringstream ss;
    for (size_t i = 0; i < SIGNATURE_SIZE; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') 
           << static_cast<int>(signature_[i]);
    }
    doc["signature"] = ss.str();

    String output;
    serializeJson(doc, output);
    return output;
}

bool Credential::deserialize(const String& json) {
    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, json);
    
    if (error) {
        ESP_LOGE(TAG, "Failed to parse credential JSON: %s", error.c_str());
        return false;
    }

    subject_ = doc["subject"].as<std::string>();
    issuedAt_ = doc["issuedAt"];
    expiry_ = doc["expiry"];
    isIssued_ = doc["isIssued"];

    claims_.clear();
    JsonArray claimsArray = doc["claims"];
    for (JsonObject claimObj : claimsArray) {
        Claim claim;
        claim.type = claimObj["type"].as<std::string>();
        claim.value = claimObj["value"].as<std::string>();
        claims_.push_back(claim);
    }

    // Convert hex string signature back to bytes
    std::string sigHex = doc["signature"];
    for (size_t i = 0; i < SIGNATURE_SIZE; i++) {
        std::string byteStr = sigHex.substr(i * 2, 2);
        signature_[i] = std::stoi(byteStr, nullptr, 16);
    }

    return true;
}

} // namespace instance
} // namespace one 