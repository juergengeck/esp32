#pragma once

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <set>
#include "types.h"
#include "security.h"
#include <SPIFFS.h>
#include <optional>

namespace chum {

class TrustedKeysManager {
public:
    explicit TrustedKeysManager(std::shared_ptr<Security> security);
    ~TrustedKeysManager();
    
    bool storeCertificate(const CertificateData& cert);
    std::vector<CertificateData> loadCertificates(const std::string& personId);
    std::unique_ptr<KeyTrustInfo> findKeyThatVerifiesSignature(const Signature& sig);
    bool verifySignatureWithTrustedKeys(const Signature& sig);
    bool isKeyTrusted(const std::string& key);
    std::vector<std::string> getRootKeys(RootKeyMode mode = RootKeyMode::MainId);
    bool isSignedByRootKey(const Signature& sig, RootKeyMode mode = RootKeyMode::MainId);
    
    bool initialize();
    void shutdown();
    bool isCertifiedBy(const std::string& data, CertificateType type, const std::string& issuer);
    std::vector<CertificateData> getCertificatesOfType(const std::string& data, CertificateType type);
    bool isTrusted(const std::string& peerId) const;
    std::optional<ProfileData> getLocalProfile() const;
    std::vector<CertificateData> getCertificates() const;
    
    std::unique_ptr<CertificateData> certify(CertificateType type, const std::vector<uint8_t>& data);
    std::unique_ptr<ProfileData> getProfileData(const std::string& profileHash, uint64_t timestamp);

private:
    bool loadFromStorage();
    bool saveToStorage();
    std::string getStoragePath(const std::string& type) const;
    void updateKeysMaps();
    void updatePersonRightsMap();
    KeyTrustInfo getKeyTrustInfoDP(const std::string& key,
                                  const std::vector<KeyTrustInfo>& rootKeyInfos,
                                  const std::vector<std::string>& visitedKeys);
    bool validateCertificate(const CertificateData& cert);
    
    std::string encodeBase64(const std::vector<uint8_t>& data);
    std::vector<uint8_t> decodeBase64(const std::string& encoded);
    
    std::shared_ptr<Security> security_;
    std::vector<CertificateData> certificates_;
    std::map<std::string, std::vector<std::string>> keysOfPerson_;
    std::string rootKey_;
    std::map<std::string, KeyTrustInfo> keysTrustCache_;
    std::set<std::string> trustedPeers_;
    std::optional<ProfileData> localProfile_;
    std::map<std::string, std::map<std::string, ProfileData>> keysToProfileMap_;
    std::map<std::string, PersonRights> personRightsMap_;
};

} // namespace chum 