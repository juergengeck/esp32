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
    name_ = "esp32";
    owner_ = "juergen.geck@gmx.de";
    did_ = "did:one:" + instanceId_;
    
    // Initialize credential manager with instance ID
    auto& credMgr = CredentialManager::getInstance();
    if (!credMgr.initialize(instanceId_.c_str(), owner_.c_str())) {
        Serial.println("Failed to initialize credential manager");
        return false;
    }
    
    initialized_ = true;
    Serial.println("Instance initialized:");
    Serial.printf("ID: %s\n", instanceId_.c_str());
    Serial.printf("Name: %s\n", name_.c_str());
    Serial.printf("Owner: %s\n", owner_.c_str());
    Serial.printf("DID: %s\n", did_.c_str());
    
    return true;
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