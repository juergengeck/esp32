# Cryptographic Design

## Implementation Strategy

### Phase 1: Core Functionality
First implement and debug core system components without encryption:
- Verifiable credentials system
- Basic storage functionality
- Owner/device management
- Core application features

### Phase 2: Storage Encryption
Once core system is stable and fully debugged, add encryption layer:

1. Verifiable Credentials
   - Ed25519 signatures for credential verification
   - Claims include: owner, email
   - Credentials are verified using issuer's public key

2. Storage Encryption
   - Each owner gets an isolated encrypted storage space
   - Encryption key derived from VC claims (owner + email)
   - No key storage needed - key can be re-derived from same VC

### Flow

1. Owner Authentication
   ```
   VC -> verify(issuerPublicKey) -> owner, email claims
   ```

2. Storage Key Derivation
   ```
   owner + email -> HKDF-SHA256 -> storageKey
   ```

3. Storage Access
   ```
   /storage/[owner_hash]/
     - All files encrypted with owner's storageKey
     - Each file has unique IV
     - AES-256-GCM for authenticated encryption
   ```

### Security Properties

1. Isolation
   - Each owner's data encrypted with different key
   - Owners cannot access each other's data
   - Same owner always gets same storage key from VC

2. Verification
   - VCs cryptographically signed by issuer
   - Owner claims cannot be forged
   - Storage access requires valid VC

3. No Key Storage
   - Storage keys derived from VC claims
   - No keys stored on device
   - No key management needed

### Implementation Plan

1. Phase 1: Core System (Current Focus)
   - [x] Ed25519 key generation
   - [x] Credential creation
   - [x] Credential verification
   - [ ] Basic storage system
   - [ ] Owner management
   - [ ] System debugging and testing

2. Phase 2: Encryption Layer (Future)
   - [ ] HKDF implementation
   - [ ] Claim-based key derivation
   - [ ] Owner directory encryption
   - [ ] Per-file encryption
   - [ ] Security testing 

