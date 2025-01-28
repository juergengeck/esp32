#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cstdint>

namespace chum {

enum class CertificateType {
    Affirmation,              // Basic trust assertion
    TrustKeys,                // Key trust declaration
    RightToDeclareTrustedKeysForEverybody,  // Global trust authority
    RightToDeclareTrustedKeysForSelf        // Self-trust authority
};

enum class RootKeyMode {
    MainId,  // Only main identity keys
    All     // All identity keys
};

// Rights structure for person rights map
struct PersonRights {
    bool rightToDeclareTrustedKeysForEverybody;
    bool rightToDeclareTrustedKeysForSelf;
};

} // namespace chum 