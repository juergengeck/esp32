#pragma once

#include <Arduino.h>
#include <memory>
#include "keys.h"
#include "credential_manager.h"

namespace one {
namespace instance {

class Instance {
public:
    static Instance& getInstance() {
        static Instance instance("1", "0.1.0", "juergen.geck@gmx.de", "juergen.geck@gmx.de");
        return instance;
    }

    Instance(const char* name, const char* version, const char* owner, const char* email);
    ~Instance() = default;
    
    // Instance properties
    const String& getName() const { return name_; }
    const String& getVersion() const { return version_; }
    const String& getOwner() const { return owner_; }
    const String& getEmail() const { return email_; }
    const uint8_t* getPublicKey() const { return keys_.getPublicKey(); }
    bool isInitialized() const { return initialized_; }
    const String& getInstanceId() const { return instanceId_; }
    const String& getDid() const { return did_; }

    // Instance operations
    bool initialize();
    bool loadKeys();
    bool saveKeys();
    bool generateKeys();
    
    // Credential-based name management
    bool updateName(const String& newName, const String& signature);
    bool loadNameFromCredential();
    bool saveNameCredential();
    
    // ID hash calculation
    String calculateOwnerIdHash() const;
    String calculateInstanceIdHash() const;
    String generateIdMicrodata() const;

private:
    Instance(const Instance&) = delete;
    Instance& operator=(const Instance&) = delete;

    String name_;
    String version_;
    String owner_;
    String email_;
    InstanceKeys keys_;
    bool initialized_;
    String instanceId_;
    String did_;

    // Helper functions
    String sha256(const String& input) const;
    String sha256(const uint8_t* input, size_t length) const;
    bool verifyOwnerSignature(const String& data, const String& signature) const;
    String createNameCredential(const String& name) const;
};

} // namespace instance
} // namespace one 