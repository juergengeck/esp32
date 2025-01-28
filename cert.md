# Certificate Management Plan for Chum Protocol

## Overview
Certificate and trust management system for secure peer identification and authentication in the ESP32-based chum protocol, based on one.models' approach.

## Trust Levels

### 1. Inner Circle (Self)
- Local keys of main identity
- Remote keys of main identity
- Local keys of secondary identities
- Remote keys of secondary identities

### 2. Others
- Keys certified by inner circle
- Keys certified by authorized peers

## Core Components

### 1. Certificate Structure
```cpp
struct CertificateData {
    std::vector<uint8_t> certificate;
    std::vector<uint8_t> signature;
    std::string signatureHash;
    std::string certificateHash;
    bool trusted;
    KeyTrustInfo* keyTrustInfo;
};

struct KeyTrustInfo {
    std::string key;
    bool trusted;
    std::string reason;
    std::vector<CertificateData*> certificates;
};

struct ProfileData {
    std::string personId;
    std::string owner;
    std::string profileId;
    std::string profileHash;
    uint64_t timestamp;
    std::vector<std::string> keys;
    std::vector<CertificateData> certificates;
};
```

### 2. Certificate Types
1. Trust Certificates
   - AffirmationCertificate: Basic trust assertion
   - TrustKeysCertificate: Key trust declaration

2. Rights Certificates
   - RightToDeclareTrustedKeysForEverybody: Global trust authority
   - RightToDeclareTrustedKeysForSelf: Self-trust authority

### 3. Trust Management
```cpp
class TrustedKeysManager {
public:
    // Key trust operations
    bool isKeyTrusted(const std::string& key);
    bool verifySignatureWithTrustedKeys(const Signature& sig);
    KeyTrustInfo findKeyThatVerifiesSignature(const Signature& sig);
    
    // Certificate operations
    CertificateData certify(const std::string& type, const CertData& data);
    bool isCertifiedBy(const std::string& data, const std::string& type, 
                      const std::string& issuer);
    std::vector<CertificateData> getCertificatesOfType(const std::string& data, 
                                                      const std::string& type);
    
    // Trust chain management
    std::vector<std::string> getRootKeys(const std::string& mode);
    bool isSignedByRootKey(const Signature& sig, const std::string& mode);
    
    // Profile management
    ProfileData getProfileData(const std::string& profileHash, uint64_t timestamp);
    void updateKeysMaps();
    void updatePersonRightsMap();

private:
    KeyTrustInfo getKeyTrustInfoDP(const std::string& key, 
                                  const std::vector<KeyTrustInfo>& rootKeys,
                                  const std::vector<std::string>& keyStack);
};
```

### 4. Storage System
- SPIFFS-based persistent storage
- In-memory key and certificate caches
- Trust chain caching
- Profile data storage

## Implementation Phases

### Phase 1: Basic Trust Management
1. Key Management
   - Key generation and storage
   - Key trust verification
   - Signature verification

2. Certificate Management
   - Basic certificate types
   - Certificate generation
   - Certificate verification

### Phase 2: Rights and Profiles
1. Rights Management
   - Trust declaration rights
   - Rights verification
   - Rights propagation

2. Profile Management
   - Profile data structure
   - Profile verification
   - Profile updates

### Phase 3: Trust Chain
1. Trust Chain Building
   - Root key management
   - Trust inheritance
   - Chain validation

2. Dynamic Programming Algorithm
   - Trust state caching
   - Recursive trust verification
   - Loop detection

## Security Considerations

### 1. Trust Model
- Clear trust hierarchy
- Trust propagation rules
- Trust revocation handling

### 2. Key Security
- Secure key generation
- Key storage protection
- Key usage tracking

### 3. Certificate Security
- Certificate validation
- Trust chain verification
- Signature verification

## Testing Strategy

### 1. Unit Tests
- Key trust verification
- Certificate operations
- Trust chain building
- Rights management

### 2. Integration Tests
- Profile management
- Trust propagation
- Certificate chain validation
- Rights inheritance

### 3. Security Tests
- Trust model validation
- Key security
- Certificate validation
- Rights verification

## Next Steps
1. Implement TrustedKeysManager
2. Add basic certificate types
3. Implement trust chain algorithm
4. Add profile management
5. Implement rights system
6. Create test suite
7. Add persistent storage
8. Document API and usage 
