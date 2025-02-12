# Self-Issued OpenID Provider (SIOP) Implementation

## Overview
Implementation of OpenID Connect Self-Issued Provider v2 (SIOPv2) on ESP32, focusing on device authentication and verifiable credential presentation.

## Goals
1. Implement minimal SIOPv2 for device authentication
2. Support verifiable credential presentation
3. Leverage ESP32 hardware security features
4. Minimize memory and storage usage

## Requirements

### Hardware Requirements
- ESP32 with:
  - Crypto acceleration support
  - At least 4MB Flash
  - Optional: PSRAM for larger credential handling

### Software Dependencies
- Arduino framework
- ESP32 SDK
- Required libraries:
  - mbedtls (crypto)
  - ArduinoJson (JSON handling)
  - SPIFFS (storage)

### Security Requirements
1. Private key protection
   - Secure storage in encrypted flash
   - Key operations using hardware crypto
   - Optional: Secure boot

2. Credential protection
   - Encrypted storage
   - Secure presentation
   - Access control

3. Communication security
   - TLS for all network operations
   - Nonce-based replay protection
   - Request validation

## Implementation Phases

### Phase 1: Core SIOP Infrastructure
1. Key Management
   - Generate/store key pairs
   - JWK thumbprint generation
   - Key operations (sign/verify)

2. Basic SIOP Protocol
   - Authorization endpoint
   - ID Token generation
   - Request validation
   - Response handling

3. Storage System
   - Secure key storage
   - Credential storage
   - Configuration storage

### Phase 2: Verifiable Credentials
1. Credential Management
   - Store credentials
   - Access control
   - Credential validation

2. Presentation Protocol
   - Parse presentation requests
   - Generate presentations
   - Sign presentations

3. Credential Types
   - Basic identity credentials
   - Authorization credentials
   - Device credentials

### Phase 3: Security Hardening
1. Hardware Security
   - Secure boot setup
   - Hardware crypto integration
   - Memory protection

2. Protocol Security
   - Request validation
   - Nonce management
   - Replay protection

3. Storage Security
   - Encryption
   - Access control
   - Secure deletion

## Minimal Implementation

### Core Components

\`\`\`cpp
// Basic SIOP structures
struct SelfIssuedOP {
    String id;                 // Subject identifier (JWK thumbprint)
    InstanceKeys keys;         // Signing keys
    SecureStorage storage;     // Secure storage
};

struct IDToken {
    String iss;               // Must equal sub
    String sub;               // Subject identifier
    String aud;              // RP client_id
    long iat;                // Issued at
    long exp;                // Expiration
    String nonce;            // Anti-replay
    bool i_am_siop;          // Required SIOPv2 claim
};

struct SIOPMetadata {
    const char* subject_syntax_types_supported[1] = {"jwk_thumb"};
    const char* id_token_signing_alg_values_supported[1] = {"ES256"};
    const char* response_types_supported[1] = {"id_token"};
    const char* scopes_supported[1] = {"openid"};
    const char* authorization_endpoint = "siop://authorize";
};
\`\`\`

### Minimal Protocol Flow

1. Authorization Request
   - RP sends request to authorization endpoint
   - Required parameters:
     - response_type=id_token
     - scope=openid
     - client_id
     - nonce
     - redirect_uri

2. Request Processing
   - Validate request parameters
   - Verify client (if registered)
   - Generate ID Token
   - Sign response

3. Authorization Response
   - Return signed ID Token
   - Include required claims
   - Handle errors

### Security Considerations

1. Key Protection
   - Store private keys in encrypted flash
   - Use hardware crypto for operations
   - Implement key rotation if needed

2. Request Validation
   - Verify all required parameters
   - Validate signatures
   - Check nonce freshness

3. Response Security
   - Sign all responses
   - Include minimal claims
   - Use secure transport

## Next Steps

1. Initial Setup
   - Configure ESP32 development environment
   - Set up required libraries
   - Test crypto hardware

2. Core Implementation
   - Implement key management
   - Create basic SIOP endpoint
   - Set up secure storage

3. Testing
   - Unit tests for components
   - Integration testing
   - Security testing

## Questions to Address

1. Storage Strategy
   - How to securely store keys?
   - Credential storage format?
   - Configuration persistence?

2. Memory Management
   - Buffer sizes for operations?
   - PSRAM usage?
   - Stack vs heap allocation?

3. Security Implementation
   - Key protection mechanism?
   - Credential encryption?
   - Access control implementation?

4. Protocol Decisions
   - Supported credential types?
   - Authentication methods?
   - Extension support? 