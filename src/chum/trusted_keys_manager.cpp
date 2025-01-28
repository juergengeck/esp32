#include "trusted_keys_manager.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <mbedtls/sha256.h>
#include <Base64.h>
#include <SPIFFS.h>
#include <esp_log.h>
#include <mbedtls/base64.h>

namespace chum {

static const char* TAG = "TrustedKeysManager";

// Helper function to compute SHA256 hash
std::string computeHash(const uint8_t* data, size_t length) {
    uint8_t hash[32];
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);  // 0 for SHA-256
    mbedtls_sha256_update(&ctx, data, length);
    mbedtls_sha256_finish(&ctx, hash);
    mbedtls_sha256_free(&ctx);
    
    // Convert to hex string
    char hexHash[65];
    for(int i = 0; i < 32; i++) {
        sprintf(hexHash + (i * 2), "%02x", hash[i]);
    }
    return std::string(hexHash);
}

TrustedKeysManager::TrustedKeysManager(std::shared_ptr<Security> security) 
    : security_(security) {
    if (!SPIFFS.begin(true)) {
        ESP_LOGE("TrustedKeysManager", "Failed to initialize SPIFFS");
        return;
    }
}

TrustedKeysManager::~TrustedKeysManager() {
    shutdown();
}

bool TrustedKeysManager::initialize() {
    if (!SPIFFS.begin(true)) {
        return false;
    }

    if (!loadFromStorage()) {
        return false;
    }

    // Initialize security subsystem
    if (!security_->generateKeyPair()) {
        return false;
    }

    // Update caches
    updateKeysMaps();
    updatePersonRightsMap();

    return true;
}

void TrustedKeysManager::shutdown() {
    saveToStorage();
    SPIFFS.end();
}

bool TrustedKeysManager::isKeyTrusted(const std::string& key) {
    auto it = keysTrustCache_.find(key);
    if (it != keysTrustCache_.end()) {
        return it->second.trusted;
    }

    // Get root key infos
    std::vector<KeyTrustInfo> rootKeyInfos;
    for (const auto& rootKey : getRootKeys()) {
        KeyTrustInfo info;
        info.keyId = rootKey;
        info.trusted = true;
        info.reason = "Root key";
        rootKeyInfos.push_back(info);
    }

    // Get trust info through depth-first search
    auto trustInfo = getKeyTrustInfoDP(key, rootKeyInfos, {});
    keysTrustCache_[key] = trustInfo;
    return trustInfo.trusted;
}

bool TrustedKeysManager::verifySignatureWithTrustedKeys(const Signature& sig) {
    auto trustInfo = findKeyThatVerifiesSignature(sig);
    return trustInfo && trustInfo->trusted;
}

std::unique_ptr<KeyTrustInfo> TrustedKeysManager::findKeyThatVerifiesSignature(const Signature& sig) {
    auto it = keysOfPerson_.find(sig.signer);
    if (it == keysOfPerson_.end()) {
        return nullptr;
    }

    for (const auto& key : it->second) {
        if (security_->verify(sig.data.data(), sig.data.size(), 
                            sig.signature.data(), sig.signature.size(),
                            decodeBase64(key))) {
            auto trustInfo = std::make_unique<KeyTrustInfo>();
            trustInfo->keyId = key;
            trustInfo->trusted = isKeyTrusted(key);
            trustInfo->reason = "signature verified";
            return trustInfo;
        }
    }

    return nullptr;
}

std::unique_ptr<CertificateData> TrustedKeysManager::certify(CertificateType type, const std::vector<uint8_t>& data) {
    auto cert = std::make_unique<CertificateData>();
    cert->certificate = data;
    cert->signature = security_->sign(data.data(), data.size());
    cert->timestamp = millis();
    cert->trusted = true;
    cert->certificateHash = computeHash(data.data(), data.size());
    cert->signatureHash = computeHash(cert->signature.data(), cert->signature.size());
    return cert;
}

bool TrustedKeysManager::isCertifiedBy(const std::string& data, CertificateType type, 
                                     const std::string& issuer) {
    auto certs = getCertificatesOfType(data, type);
    for (const auto& cert : certs) {
        Signature sig;
        sig.data = cert.signature;
        sig.signer = issuer;
        if (cert.trusted && verifySignatureWithTrustedKeys(sig)) {
            return true;
        }
    }
    return false;
}

std::vector<CertificateData> TrustedKeysManager::getCertificatesOfType(const std::string& data,
                                                                      CertificateType type) {
    return loadCertificates(data);
}

std::vector<std::string> TrustedKeysManager::getRootKeys(RootKeyMode mode) {
    std::vector<std::string> rootKeys;
    
    // In a real implementation, we would:
    // 1. Get the main identity's keys
    // 2. If mode is All, get all identity keys
    // 3. Filter for local keys only
    
    return rootKeys;
}

bool TrustedKeysManager::isSignedByRootKey(const Signature& sig, RootKeyMode mode) {
    std::vector<uint8_t> rootKeyData = decodeBase64(rootKey_);
    return security_->verify(sig.data.data(), sig.data.size(),
                           rootKeyData.data(), rootKeyData.size(),
                           sig.signature);
}

KeyTrustInfo TrustedKeysManager::getKeyTrustInfoDP(const std::string& key,
                                                  const std::vector<KeyTrustInfo>& rootKeyInfos,
                                                  const std::vector<std::string>& visitedKeys) {
    // Check if we've already visited this key
    if (std::find(visitedKeys.begin(), visitedKeys.end(), key) != visitedKeys.end()) {
        KeyTrustInfo result;
        result.keyId = key;
        result.trusted = false;
        result.reason = "Circular dependency detected";
        return result;
    }

    // Check if this is a root key
    for (const auto& rootInfo : rootKeyInfos) {
        if (rootInfo.keyId == key) {
            return rootInfo;
        }
    }

    // Get all certificates that certify this key
    std::vector<CertificateData> certs = getCertificatesOfType(key, CertificateType::TrustKeys);
    if (certs.empty()) {
        KeyTrustInfo result;
        result.keyId = key;
        result.trusted = false;
        result.reason = "No trust certificates found";
        return result;
    }

    // Add this key to visited keys
    std::vector<std::string> newVisited = visitedKeys;
    newVisited.push_back(key);

    // Check each certificate
    for (const auto& cert : certs) {
        if (!validateCertificate(cert)) {
            continue;
        }

        // Get trust info for the certificate's key
        auto certKeyInfo = getKeyTrustInfoDP(cert.id, rootKeyInfos, newVisited);
        if (certKeyInfo.trusted) {
            KeyTrustInfo result;
            result.keyId = key;
            result.trusted = true;
            result.reason = "Trusted by " + cert.id;
            return result;
        }
    }

    KeyTrustInfo result;
    result.keyId = key;
    result.trusted = false;
    result.reason = "No trusted certification path found";
    return result;
}

bool TrustedKeysManager::validateCertificate(const CertificateData& cert) {
    // Verify certificate hash
    std::string computedCertHash = computeHash(cert.certificate.data(), cert.certificate.size());
    if (computedCertHash != cert.certificateHash) {
        return false;
    }

    // Verify signature hash
    std::string computedSigHash = computeHash(cert.signature.data(), cert.signature.size());
    if (computedSigHash != cert.signatureHash) {
        return false;
    }

    // Verify signature using security module
    if (!security_->verify(cert.certificate.data(), cert.certificate.size(),
                          cert.signature.data(), cert.signature.size(),
                          std::vector<uint8_t>())) {
        return false;
    }

    return true;
}

bool TrustedKeysManager::loadFromStorage() {
    // Load certificates using c-string path
    File file = SPIFFS.open("/certificates.json", "r");
    if (!file) {
        return false;
    }

    StaticJsonDocument<4096> doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        return false;
    }

    // Load certificates
    JsonArray certificatesArray = doc["certificates"].as<JsonArray>();
    for (JsonObject certObj : certificatesArray) {
        CertificateData cert;
        
        // Load binary data directly
        const char* certData = certObj["certificate"].as<const char*>();
        const char* sigData = certObj["signature"].as<const char*>();
        cert.certificate = std::vector<uint8_t>(certData, certData + strlen(certData));
        cert.signature = std::vector<uint8_t>(sigData, sigData + strlen(sigData));
        
        // Load hashes and trust status
        cert.certificateHash = certObj["certificateHash"].as<std::string>();
        cert.signatureHash = certObj["signatureHash"].as<std::string>();
        cert.trusted = certObj["trusted"].as<bool>();
        certificates_.push_back(cert);
    }

    // Load profiles
    File profileFile = SPIFFS.open("/profiles.json", "r");
    if (profileFile) {
        StaticJsonDocument<4096> profileDoc;
        error = deserializeJson(profileDoc, profileFile);
        profileFile.close();

        if (!error) {
            JsonArray profilesArray = profileDoc["profiles"].as<JsonArray>();
            for (JsonObject profObj : profilesArray) {
                ProfileData profile;
                profile.personId = profObj["personId"].as<std::string>();
                profile.owner = profObj["owner"].as<std::string>();
                profile.profileId = profObj["profileId"].as<std::string>();
                profile.profileHash = profObj["profileHash"].as<std::string>();
                profile.timestamp = profObj["timestamp"].as<uint64_t>();

                JsonArray keysArray = profObj["keys"].as<JsonArray>();
                for (const auto& key : keysArray) {
                    profile.keys.push_back(key.as<std::string>());
                }

                // Map keys to profile
                for (const auto& key : profile.keys) {
                    keysToProfileMap_[key][profile.profileHash] = profile;
                    if (!profile.owner.empty()) {
                        keysOfPerson_[profile.owner].push_back(key);
                    }
                }
            }
        }
    }

    // Load rights
    File rightsFile = SPIFFS.open("/rights.json", "r");
    if (rightsFile) {
        StaticJsonDocument<4096> rightsDoc;
        error = deserializeJson(rightsDoc, rightsFile);
        rightsFile.close();

        if (!error) {
            JsonObject rightsObj = rightsDoc.as<JsonObject>();
            for (JsonPair kv : rightsObj) {
                const char* personId = kv.key().c_str();
                JsonObject rights = kv.value().as<JsonObject>();
                
                personRightsMap_[personId] = {
                    .rightToDeclareTrustedKeysForEverybody = rights["global"].as<bool>(),
                    .rightToDeclareTrustedKeysForSelf = rights["self"].as<bool>()
                };
            }
        }
    }

    return true;
}

bool TrustedKeysManager::saveToStorage() {
    // Save certificates
    StaticJsonDocument<4096> doc;
    JsonArray certsArray = doc.createNestedArray("certificates");

    for (const auto& cert : certificates_) {
        JsonObject certObj = certsArray.createNestedObject();
        certObj["id"] = cert.id;
        certObj["certificate"] = encodeBase64(cert.certificate);
        certObj["signature"] = encodeBase64(cert.signature);
        certObj["timestamp"] = cert.timestamp;
        certObj["trusted"] = cert.trusted;
        certObj["certificateHash"] = cert.certificateHash;
        certObj["signatureHash"] = cert.signatureHash;
    }

    // Save trust cache
    JsonObject trustCacheObj = doc.createNestedObject("trustCache");
    for (const auto& [key, trustInfo] : keysTrustCache_) {
        JsonObject keyObj = trustCacheObj.createNestedObject(key);
        keyObj["trusted"] = trustInfo.trusted;
        keyObj["reason"] = trustInfo.reason;
        keyObj["keyId"] = trustInfo.keyId;
    }

    // Save profiles
    JsonObject profilesObj = doc.createNestedObject("profiles");
    for (const auto& [key, profileMap] : keysToProfileMap_) {
        JsonObject keyObj = profilesObj.createNestedObject(key);
        for (const auto& [hash, profile] : profileMap) {
            JsonObject profObj = keyObj.createNestedObject(hash);
            profObj["id"] = profile.id;
            profObj["personId"] = profile.personId;
            profObj["owner"] = profile.owner;
            profObj["profileId"] = profile.profileId;
            profObj["profileHash"] = profile.profileHash;
            profObj["timestamp"] = profile.timestamp;

            JsonArray keysArray = profObj.createNestedArray("keys");
            for (const auto& key : profile.keys) {
                keysArray.add(key);
            }

            JsonArray certsArray = profObj.createNestedArray("certificates");
            for (const auto& cert : profile.certificates) {
                JsonObject certObj = certsArray.createNestedObject();
                certObj["id"] = cert.id;
                certObj["certificate"] = encodeBase64(cert.certificate);
                certObj["signature"] = encodeBase64(cert.signature);
                certObj["timestamp"] = cert.timestamp;
                certObj["trusted"] = cert.trusted;
                certObj["certificateHash"] = cert.certificateHash;
                certObj["signatureHash"] = cert.signatureHash;
            }
        }
    }

    // Save person rights
    JsonObject rightsObj = doc.createNestedObject("personRights");
    for (const auto& [personId, rights] : personRightsMap_) {
        JsonObject personObj = rightsObj.createNestedObject(personId);
        personObj["rightToDeclareTrustedKeysForEverybody"] = rights.rightToDeclareTrustedKeysForEverybody;
        personObj["rightToDeclareTrustedKeysForSelf"] = rights.rightToDeclareTrustedKeysForSelf;
    }

    // Write to file
    File file = SPIFFS.open("/trusted_keys.json", "w");
    if (!file) {
        return false;
    }

    if (serializeJson(doc, file) == 0) {
        file.close();
        return false;
    }

    file.close();
    return true;
}

std::string TrustedKeysManager::getStoragePath(const std::string& type) const {
    return "/" + type + ".json";
}

std::vector<CertificateData> TrustedKeysManager::loadCertificates(const std::string& personId) {
    std::vector<CertificateData> certs;
    
    if (!SPIFFS.begin(true)) {
        return certs;
    }

    File root = SPIFFS.open("/certs");
    if (!root || !root.isDirectory()) {
        return certs;
    }

    File file = root.openNextFile();
    while (file) {
        if (!file.isDirectory()) {
            DynamicJsonDocument doc(4096);
            DeserializationError error = deserializeJson(doc, file);
            
            if (!error) {
                CertificateData cert;
                cert.id = doc["id"].as<std::string>();
                cert.certificate = decodeBase64(doc["certificate"].as<std::string>());
                cert.signature = decodeBase64(doc["signature"].as<std::string>());
                cert.timestamp = doc["timestamp"].as<uint64_t>();
                cert.trusted = doc["trusted"].as<bool>();
                cert.certificateHash = doc["certificateHash"].as<std::string>();
                cert.signatureHash = doc["signatureHash"].as<std::string>();
                
                if (doc.containsKey("keyTrustInfo")) {
                    auto trustInfo = std::make_shared<KeyTrustInfo>();
                    trustInfo->keyId = doc["keyTrustInfo"]["keyId"].as<std::string>();
                    trustInfo->trusted = doc["keyTrustInfo"]["trusted"].as<bool>();
                    trustInfo->reason = doc["keyTrustInfo"]["reason"].as<std::string>();
                    cert.keyTrustInfo = trustInfo;
                }
                
                certs.push_back(std::move(cert));
            }
        }
        file = root.openNextFile();
    }

    return certs;
}

void TrustedKeysManager::updateKeysMaps() {
    keysTrustCache_.clear();
    keysToProfileMap_.clear();

    // Get root key infos
    std::vector<KeyTrustInfo> rootKeyInfos;
    for (const auto& rootKey : getRootKeys()) {
        KeyTrustInfo info;
        info.keyId = rootKey;
        info.trusted = true;
        info.reason = "Root key";
        rootKeyInfos.push_back(info);
    }

    // Update trust info for all keys
    for (const auto& cert : certificates_) {
        if (keysTrustCache_.find(cert.id) == keysTrustCache_.end()) {
            keysTrustCache_[cert.id] = getKeyTrustInfoDP(cert.id, rootKeyInfos, {});
        }
    }
}

void TrustedKeysManager::updatePersonRightsMap() {
    personRightsMap_.clear();

    File file = SPIFFS.open("/rights.json", "r");
    if (!file) {
        return;
    }

    StaticJsonDocument<4096> doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        return;
    }

    JsonObject rightsObj = doc.as<JsonObject>();
    for (JsonPair kv : rightsObj) {
        const char* personId = kv.key().c_str();
        JsonObject rights = kv.value().as<JsonObject>();

        auto& personRights = personRightsMap_[personId];
        personRights.rightToDeclareTrustedKeysForEverybody = false;
        personRights.rightToDeclareTrustedKeysForSelf = false;

        if (rights["global"].as<bool>()) {
            for (const auto& cert : loadCertificates(personId)) {
                if (cert.trusted) {
                    Signature sig;
                    sig.data = cert.signature;
                    sig.signer = personId;
                    if (verifySignatureWithTrustedKeys(sig) && cert.certificate.size() > 0) {
                        CertificateType type;
                        memcpy(&type, cert.certificate.data(), sizeof(CertificateType));
                        if (type == CertificateType::RightToDeclareTrustedKeysForEverybody) {
                            personRights.rightToDeclareTrustedKeysForEverybody = true;
                            break;
                        }
                    }
                }
            }
        }

        if (rights["self"].as<bool>()) {
            for (const auto& cert : loadCertificates(personId)) {
                if (cert.trusted) {
                    Signature sig;
                    sig.data = cert.signature;
                    sig.signer = personId;
                    if (verifySignatureWithTrustedKeys(sig) && cert.certificate.size() > 0) {
                        CertificateType type;
                        memcpy(&type, cert.certificate.data(), sizeof(CertificateType));
                        if (type == CertificateType::RightToDeclareTrustedKeysForSelf) {
                            personRights.rightToDeclareTrustedKeysForSelf = true;
                            break;
                        }
                    }
                }
            }
        }

        auto rootKeys = getRootKeys(RootKeyMode::All);
        for (const auto& key : rootKeys) {
            auto it = keysOfPerson_.find(personId);
            if (it != keysOfPerson_.end() && 
                std::find(it->second.begin(), it->second.end(), key) != it->second.end()) {
                personRights.rightToDeclareTrustedKeysForEverybody = true;
                personRights.rightToDeclareTrustedKeysForSelf = true;
                break;
            }
        }
    }

    saveToStorage();
}

std::unique_ptr<ProfileData> TrustedKeysManager::getProfileData(const std::string& profileHash, uint64_t timestamp) {
    // Search through all profiles
    for (const auto& [key, profileMap] : keysToProfileMap_) {
        auto it = profileMap.find(profileHash);
        if (it != profileMap.end()) {
            // Check timestamp if provided
            if (timestamp != 0 && it->second.timestamp != timestamp) {
                continue;
            }
            
            // Create a copy of the profile
            auto profile = std::make_unique<ProfileData>();
            profile->personId = it->second.personId;
            profile->owner = it->second.owner;
            profile->profileId = it->second.profileId;
            profile->profileHash = it->second.profileHash;
            profile->timestamp = it->second.timestamp;
            profile->keys = it->second.keys;
            profile->certificates = it->second.certificates;
            
            return profile;
        }
    }
    
    return nullptr;
}

bool TrustedKeysManager::isTrusted(const std::string& peerId) const {
    return std::find(trustedPeers_.begin(), trustedPeers_.end(), peerId) != trustedPeers_.end();
}

std::optional<ProfileData> TrustedKeysManager::getLocalProfile() const {
    return localProfile_;
}

std::vector<CertificateData> TrustedKeysManager::getCertificates() const {
    return certificates_;
}

std::string TrustedKeysManager::encodeBase64(const std::vector<uint8_t>& data) {
    size_t output_len;
    mbedtls_base64_encode(nullptr, 0, &output_len, data.data(), data.size());
    
    std::vector<unsigned char> output(output_len);
    mbedtls_base64_encode(output.data(), output.size(), &output_len, data.data(), data.size());
    
    return std::string(output.begin(), output.begin() + output_len);
}

std::vector<uint8_t> TrustedKeysManager::decodeBase64(const std::string& encoded) {
    size_t output_len;
    mbedtls_base64_decode(nullptr, 0, &output_len, 
                         (const unsigned char*)encoded.c_str(), encoded.length());
    
    std::vector<uint8_t> output(output_len);
    mbedtls_base64_decode(output.data(), output.size(), &output_len,
                         (const unsigned char*)encoded.c_str(), encoded.length());
    
    output.resize(output_len);
    return output;
}

bool TrustedKeysManager::storeCertificate(const CertificateData& cert) {
    // Create certificates directory if it doesn't exist
    if (!SPIFFS.exists("/certs")) {
        if (!SPIFFS.mkdir("/certs")) {
            ESP_LOGE("TrustedKeysManager", "Failed to create certs directory");
            return false;
        }
    }

    char filename[64];
    snprintf(filename, sizeof(filename), "/certs/%s.json", cert.id.c_str());
    File file = SPIFFS.open(filename, "w");
    if (!file) {
        ESP_LOGE("TrustedKeysManager", "Failed to open file for writing");
        return false;
    }

    // Create JSON document
    DynamicJsonDocument doc(4096);
    doc["id"] = cert.id;
    doc["certificate"] = encodeBase64(cert.certificate);
    doc["signature"] = encodeBase64(cert.signature);
    doc["timestamp"] = cert.timestamp;
    doc["trusted"] = cert.trusted;
    doc["certificateHash"] = cert.certificateHash;
    doc["signatureHash"] = cert.signatureHash;
    
    if (cert.keyTrustInfo) {
        JsonObject trustInfo = doc.createNestedObject("keyTrustInfo");
        trustInfo["keyId"] = cert.keyTrustInfo->keyId;
        trustInfo["trusted"] = cert.keyTrustInfo->trusted;
        trustInfo["reason"] = cert.keyTrustInfo->reason;
    }

    if (serializeJson(doc, file) == 0) {
        ESP_LOGE("TrustedKeysManager", "Failed to write to file");
        file.close();
        return false;
    }

    file.close();
    return true;
}

} // namespace chum 