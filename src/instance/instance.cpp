#include "instance.h"
#include <SPIFFS.h>
#include <esp_random.h>
#include <mbedtls/sha256.h>
#include <ArduinoJson.h>
#include "esp_system.h"

namespace one {
namespace instance {

Instance::Instance(const char* name, const char* version, const char* owner, const char* email)
    : name_(name)
    , version_(version)
    , owner_(owner)
    , email_(email)
    , initialized_(false) {
}

bool Instance::initialize() {
    // Generate UUID v4 for instance ID
    uint8_t uuid[16];
    for(int i = 0; i < 16; i++) {
        uuid[i] = esp_random() & 0xff;
    }
    // Set version (4) and variant (2) bits
    uuid[6] = (uuid[6] & 0x0F) | 0x40;
    uuid[8] = (uuid[8] & 0x3F) | 0x80;
    
    // Format UUID string
    char uuidStr[37];
    sprintf(uuidStr, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
            uuid[0], uuid[1], uuid[2], uuid[3],
            uuid[4], uuid[5],
            uuid[6], uuid[7],
            uuid[8], uuid[9],
            uuid[10], uuid[11], uuid[12], uuid[13], uuid[14], uuid[15]);
    
    instanceId_ = String(uuidStr);
    did_ = "did:one:" + instanceId_;
    
    // Initialize credential manager with instance ID
    auto& credMgr = CredentialManager::getInstance();
    if (!credMgr.initialize(instanceId_.c_str(), owner_.c_str())) {
        Serial.println("Failed to initialize credential manager");
        return false;
    }

    // Try to load name from credential, if not found use default
    if (!loadNameFromCredential()) {
        name_ = "1";
        if (!saveNameCredential()) {
            Serial.println("Warning: Failed to save initial name credential");
        }
    }
    
    initialized_ = true;
    Serial.println("Instance initialized:");
    Serial.printf("ID: %s\n", instanceId_.c_str());
    Serial.printf("Name: %s\n", name_.c_str());
    Serial.printf("Owner: %s\n", owner_.c_str());
    Serial.printf("DID: %s\n", did_.c_str());
    
    return true;
}

bool Instance::updateName(const String& newName, const String& signature) {
    // Create the message that was signed (name + instanceId)
    String message = newName + instanceId_;
    
    // Verify the signature using credential manager
    auto& credMgr = CredentialManager::getInstance();
    if (!credMgr.verifyOwnerSignature(message, signature)) {
        Serial.println("Invalid signature for name update");
        return false;
    }
    
    // Update the name
    name_ = newName;
    
    // Save the new name credential
    if (!saveNameCredential()) {
        Serial.println("Warning: Failed to save updated name credential");
        return false;
    }
    
    Serial.println("Name updated successfully to: " + name_);
    Serial.println("System will restart in 3 seconds...");
    
    // Stop BLE advertising and disconnect any clients
    auto& bleDiscovery = BLEDiscovery::getInstance();
    bleDiscovery.stopAdvertising();
    bleDiscovery.disconnect();
    
    // Give some time for cleanup and messages to be sent
    delay(3000);
    
    // Restart the ESP32
    ESP.restart();
    
    return true; // This won't actually be reached due to restart
}

bool Instance::loadNameFromCredential() {
    if (!SPIFFS.exists("/instance/name.vc")) {
        return false;
    }
    
    File file = SPIFFS.open("/instance/name.vc", "r");
    if (!file) {
        return false;
    }
    
    String vcJson = file.readString();
    file.close();
    
    // Parse and verify the credential
    auto& credMgr = CredentialManager::getInstance();
    if (!credMgr.verifyCredential(vcJson.c_str())) {
        Serial.println("Invalid name credential");
        return false;
    }
    
    // Parse the credential to get the name
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, vcJson);
    if (error) {
        return false;
    }
    
    const char* name = doc["credentialSubject"]["name"] | "";
    if (strlen(name) > 0) {
        name_ = name;
        return true;
    }
    
    return false;
}

bool Instance::saveNameCredential() {
    String vcJson = createNameCredential(name_);
    
    // Create directory if it doesn't exist
    if (!SPIFFS.exists("/instance")) {
        if (!SPIFFS.mkdir("/instance")) {
            return false;
        }
    }
    
    File file = SPIFFS.open("/instance/name.vc", "w");
    if (!file) {
        return false;
    }
    
    size_t written = file.print(vcJson);
    file.close();
    
    return written == vcJson.length();
}

String Instance::createNameCredential(const String& name) const {
    DynamicJsonDocument doc(1024);
    
    doc["@context"] = "https://www.w3.org/2018/credentials/v1";
    doc["type"] = "VerifiableCredential";
    doc["issuer"] = owner_;
    doc["issuanceDate"] = "2024-01-01T00:00:00Z"; // TODO: Use real date
    
    JsonObject credentialSubject = doc.createNestedObject("credentialSubject");
    credentialSubject["id"] = did_;
    credentialSubject["name"] = name;
    
    String vcJson;
    serializeJson(doc, vcJson);
    return vcJson;
}

bool Instance::loadKeys() {
    if (!SPIFFS.exists("/instance/keys.bin")) {
        return false;
    }
    
    File file = SPIFFS.open("/instance/keys.bin", "r");
    if (!file) {
        return false;
    }
    
    size_t bytesRead = file.read(keys_.privateKey_, sizeof(keys_.privateKey_));
    file.close();
    
    if (bytesRead != sizeof(keys_.privateKey_)) {
        return false;
    }
    
    // Derive public key from private key
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, keys_.privateKey_, sizeof(keys_.privateKey_));
    mbedtls_sha256_finish(&ctx, keys_.publicKey_);
    
    return true;
}

bool Instance::saveKeys() {
    // Create directory if it doesn't exist
    if (!SPIFFS.exists("/instance")) {
        if (!SPIFFS.mkdir("/instance")) {
            return false;
        }
    }
    
    File file = SPIFFS.open("/instance/keys.bin", "w");
    if (!file) {
        return false;
    }
    
    size_t bytesWritten = file.write(keys_.privateKey_, sizeof(keys_.privateKey_));
    file.close();
    
    return bytesWritten == sizeof(keys_.privateKey_);
}

bool Instance::generateKeys() {
    // Generate private key using hardware RNG
    esp_fill_random(keys_.privateKey_, sizeof(keys_.privateKey_));
    
    // Derive public key using SHA256
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, keys_.privateKey_, sizeof(keys_.privateKey_));
    mbedtls_sha256_finish(&ctx, keys_.publicKey_);
    
    return true;
}

String Instance::sha256(const String& input) const {
    return sha256((const uint8_t*)input.c_str(), input.length());
}

String Instance::sha256(const uint8_t* input, size_t length) const {
    uint8_t hash[32];
    mbedtls_sha256_context ctx;
    
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0); // 0 for SHA256, 1 for SHA224
    mbedtls_sha256_update(&ctx, input, length);
    mbedtls_sha256_finish(&ctx, hash);
    mbedtls_sha256_free(&ctx);

    char hex[65];
    for(int i = 0; i < 32; i++) {
        sprintf(hex + i*2, "%02x", (int)hash[i]);
    }
    hex[64] = 0;
    return String(hex);
}

String Instance::generateIdMicrodata() const {
    String microdata = "<div itemscope itemtype=\"http://one-db.org/schema/2022-02/instance\">\n";
    microdata += "  <meta itemprop=\"name\" content=\"" + name_ + "\">\n";
    microdata += "  <meta itemprop=\"version\" content=\"" + version_ + "\">\n";
    microdata += "  <meta itemprop=\"owner\" content=\"" + owner_ + "\">\n";
    microdata += "  <meta itemprop=\"email\" content=\"" + email_ + "\">\n";
    microdata += "</div>";
    return microdata;
}

String Instance::calculateOwnerIdHash() const {
    // Create ID object microdata for Person with data-id-object attribute
    String ownerData = "<div data-id-object=\"true\" itemscope itemtype=\"//refin.io/Person\">";
    ownerData += "<span itemprop=\"email\">" + email_ + "</span>";
    ownerData += "</div>";
    
    Serial.println("Owner ID Object Microdata:");
    Serial.println(ownerData);
    
    String hash = sha256(ownerData);
    Serial.println("Owner ID Hash: " + hash);
    return hash;
}

String Instance::calculateInstanceIdHash() const {
    // First calculate owner hash
    String ownerHash = calculateOwnerIdHash();
    
    // Create ID object microdata for Instance with data-id-object attribute
    String instanceData = "<div data-id-object=\"true\" itemscope itemtype=\"//refin.io/Instance\">";
    instanceData += "<span itemprop=\"name\">" + name_ + "</span>";
    instanceData += "<a itemprop=\"owner\" data-type=\"id\">" + ownerHash + "</a>";
    instanceData += "</div>";
    
    Serial.println("\nInstance ID Object Microdata:");
    Serial.println(instanceData);
    
    String hash = sha256(instanceData);
    Serial.println("Instance ID Hash: " + hash);
    return hash;
}

} // namespace instance
} // namespace one 