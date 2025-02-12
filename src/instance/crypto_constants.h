#pragma once

#include <cstddef>    // For std::size_t

namespace one {
namespace instance {
namespace crypto {  // Additional namespace to avoid conflicts

// ECDSA P-256 constants
static constexpr std::size_t ECDSA_PUBLIC_KEY_LENGTH = 64;   // X and Y coordinates for P-256
static constexpr std::size_t ECDSA_PRIVATE_KEY_LENGTH = 32;  // Private key length for P-256
static constexpr std::size_t SALT_LENGTH = 32;               // Salt length for key derivation
static constexpr std::size_t DERIVED_KEY_LENGTH = 32;        // Derived key length

} // namespace crypto
} // namespace instance
} // namespace one 