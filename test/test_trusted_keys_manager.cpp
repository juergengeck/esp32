#include <unity.h>
#include <SPIFFS.h>
#include "chum/trusted_keys_manager.h"
#include "chum/security.h"

using namespace chum;

TrustedKeysManager* manager = nullptr;
std::unique_ptr<Security> security = nullptr;

// Test data
const char* TEST_PERSON_ID = "test_person";
const char* TEST_PROFILE_ID = "test_profile";
const char* TEST_PROFILE_HASH = "test_hash";
const uint64_t TEST_TIMESTAMP = 1234567890;

void setUp() {
    SPIFFS.begin(true);  // Format on first use
    manager = new TrustedKeysManager();
    security = std::make_unique<Security>();
    security->initializeMbedTLS();
}

void tearDown() {
    delete manager;
    manager = nullptr;
    security.reset();
    SPIFFS.format();  // Clean up after tests
}

// Helper function to create a test certificate
CertificateData createTestCertificate(CertificateType type, const std::string& data) {
    std::vector<uint8_t> certData(sizeof(CertificateType) + data.size());
    memcpy(certData.data(), &type, sizeof(CertificateType));
    memcpy(certData.data() + sizeof(CertificateType), data.c_str(), data.size());
    
    CertificateData cert;
    cert.certificate = certData;
    cert.signature = security->sign(certData.data(), certData.size());
    cert.certificateHash = security->hash(certData.data(), certData.size());
    cert.signatureHash = security->hash(cert.signature.data(), cert.signature.size());
    cert.trusted = true;
    cert.keyTrustInfo = nullptr;
    return cert;
}

// Helper function to create a test profile
ProfileData createTestProfile() {
    ProfileData profile;
    profile.personId = TEST_PERSON_ID;
    profile.owner = TEST_PERSON_ID;
    profile.profileId = TEST_PROFILE_ID;
    profile.profileHash = TEST_PROFILE_HASH;
    profile.timestamp = TEST_TIMESTAMP;
    
    // Generate a test key pair
    auto keyPair = security->generateKeyPair();
    profile.keys.push_back(keyPair.publicKey);
    
    // Add a test certificate
    profile.certificates.push_back(
        createTestCertificate(CertificateType::Affirmation, "test certificate")
    );
    
    return profile;
}

void test_initialization() {
    TEST_ASSERT_TRUE(manager->initialize());
}

void test_certificate_operations() {
    // Test certificate creation
    auto cert = createTestCertificate(CertificateType::Affirmation, "test data");
    TEST_ASSERT_TRUE(manager->validateCertificate(cert));
    TEST_ASSERT_TRUE(manager->storeCertificate(cert));
    
    // Test certificate loading
    auto certs = manager->loadCertificates("test");
    TEST_ASSERT_EQUAL(1, certs.size());
    TEST_ASSERT_EQUAL_MEMORY(cert.certificate.data(), 
                            certs[0].certificate.data(),
                            cert.certificate.size());
}

void test_profile_management() {
    // Create and store a test profile
    auto profile = createTestProfile();
    manager->updateKeysMaps();  // This will store the profile
    
    // Test profile retrieval
    auto retrieved = manager->getProfileData(TEST_PROFILE_HASH, TEST_TIMESTAMP);
    TEST_ASSERT_NOT_NULL(retrieved.get());
    TEST_ASSERT_EQUAL_STRING(profile.personId.c_str(), retrieved->personId.c_str());
    TEST_ASSERT_EQUAL_STRING(profile.profileHash.c_str(), retrieved->profileHash.c_str());
    TEST_ASSERT_EQUAL(profile.timestamp, retrieved->timestamp);
}

void test_key_trust() {
    // Create a root key profile
    auto rootProfile = createTestProfile();
    auto rootKey = rootProfile.keys[0];
    
    // Create a regular profile with a key to be trusted
    auto profile = createTestProfile();
    auto key = profile.keys[0];
    
    // Add trust certificate signed by root key
    auto trustCert = createTestCertificate(CertificateType::TrustKeys, key);
    profile.certificates.push_back(trustCert);
    
    manager->updateKeysMaps();
    
    // Test trust verification
    TEST_ASSERT_TRUE(manager->isKeyTrusted(key));
}

void test_rights_management() {
    // Create a profile with global rights
    auto profile = createTestProfile();
    auto globalRightsCert = createTestCertificate(
        CertificateType::RightToDeclareTrustedKeysForEverybody,
        "global rights"
    );
    profile.certificates.push_back(globalRightsCert);
    
    manager->updateKeysMaps();
    manager->updatePersonRightsMap();
    
    // Verify rights are properly set
    auto rights = manager->personRightsMap_[TEST_PERSON_ID];
    TEST_ASSERT_TRUE(rights.rightToDeclareTrustedKeysForEverybody);
}

void test_trust_chain() {
    // Create a chain of trust: root -> intermediate -> leaf
    auto rootProfile = createTestProfile();
    auto intermediateProfile = createTestProfile();
    auto leafProfile = createTestProfile();
    
    // Root certifies intermediate
    auto intermediateCert = createTestCertificate(
        CertificateType::TrustKeys,
        intermediateProfile.keys[0]
    );
    intermediateProfile.certificates.push_back(intermediateCert);
    
    // Intermediate certifies leaf
    auto leafCert = createTestCertificate(
        CertificateType::TrustKeys,
        leafProfile.keys[0]
    );
    leafProfile.certificates.push_back(leafCert);
    
    manager->updateKeysMaps();
    
    // Verify trust chain
    TEST_ASSERT_TRUE(manager->isKeyTrusted(leafProfile.keys[0]));
}

void test_signature_verification() {
    // Create a profile with a key
    auto profile = createTestProfile();
    auto key = profile.keys[0];
    
    // Create and sign some test data
    std::string testData = "test data";
    auto signature = security->sign(
        reinterpret_cast<const uint8_t*>(testData.data()),
        testData.size()
    );
    
    Signature sig{signature, TEST_PERSON_ID};
    
    manager->updateKeysMaps();
    
    // Test signature verification
    TEST_ASSERT_TRUE(manager->verifySignatureWithTrustedKeys(sig));
}

void runTests() {
    UNITY_BEGIN();
    
    RUN_TEST(test_initialization);
    RUN_TEST(test_certificate_operations);
    RUN_TEST(test_profile_management);
    RUN_TEST(test_key_trust);
    RUN_TEST(test_rights_management);
    RUN_TEST(test_trust_chain);
    RUN_TEST(test_signature_verification);
    
    UNITY_END();
}

void setup() {
    delay(2000);  // Allow serial to settle
    runTests();
}

void loop() {
    // Empty
} 