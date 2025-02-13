#include "../src/one_crypto.h"
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>

using namespace one::crypto;

// Global key pair to maintain state
static KeyPair g_keys;
static bool g_keys_initialized = false;

void printUsage() {
    std::cout << "ONE Crypto Tool\n\n"
              << "Key Management:\n"
              << "  generate                     - Generate a new key pair and display them\n"
              << "  export-keys <file>           - Export keys to file\n"
              << "  import-keys <file>           - Import keys from file\n"
              << "  show-keys                    - Display current keys\n\n"
              << "Credential Operations:\n"  
              << "  create-credential <file>     - Create a verifiable credential from JSON template\n"
              << "  verify-credential <file>     - Verify a credential file\n"
              << "  sign-data <data>            - Sign arbitrary data and show signature\n"
              << "  verify-data <data> <sig>    - Verify signed data\n";
}

void displayKeys(const KeyPair& keys) {
    auto publicKey = keys.exportPublicKey();
    auto privateKey = keys.exportPrivateKey();
    
    std::cout << "\nKey Pair:\n";
    std::cout << "Public Key (hex):  " << KeyPair::toHex(publicKey) << "\n";
    std::cout << "Public Key (b64):  " << KeyPair::toBase64(publicKey) << "\n";
    std::cout << "Private Key (hex): " << KeyPair::toHex(privateKey) << "\n";
    std::cout << "Private Key (b64): " << KeyPair::toBase64(privateKey) << "\n\n";
}

bool saveKeys(const KeyPair& keys, const std::string& filename) {
    std::ofstream file(filename);
    if (!file) {
        std::cerr << "Failed to open file for writing: " << filename << "\n";
        return false;
    }

    auto privateKey = keys.exportPrivateKey();
    auto publicKey = keys.exportPublicKey();

    if (privateKey.empty() || publicKey.empty()) {
        std::cerr << "No keys available to export\n";
        return false;
    }

    // Write keys in a more robust format
    file << "{\n";
    file << "  \"privateKey\": \"" << KeyPair::toBase64(privateKey) << "\",\n";
    file << "  \"publicKey\": \"" << KeyPair::toBase64(publicKey) << "\"\n";
    file << "}\n";

    file.close();
    return true;
}

bool loadKeys(KeyPair& keys, const std::string& filename) {
    std::ifstream file(filename);
    if (!file) {
        std::cerr << "Failed to open key file: " << filename << "\n";
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string json = buffer.str();
    file.close();

    std::cout << "Loading keys from file: " << filename << "\n";
    std::cout << "File contents: " << json << "\n";

    // Basic JSON parsing
    size_t privStart = json.find("\"privateKey\":") + 13;
    while (privStart < json.length() && (json[privStart] == ' ' || json[privStart] == '"')) privStart++;
    size_t privEnd = json.find('"', privStart);

    size_t pubStart = json.find("\"publicKey\":") + 12;
    while (pubStart < json.length() && (json[pubStart] == ' ' || json[pubStart] == '"')) pubStart++;
    size_t pubEnd = json.find('"', pubStart);

    if (privStart == std::string::npos || pubStart == std::string::npos ||
        privEnd == std::string::npos || pubEnd == std::string::npos) {
        std::cerr << "Invalid key file format\n";
        return false;
    }

    std::string privateKeyB64 = json.substr(privStart, privEnd - privStart);
    std::string publicKeyB64 = json.substr(pubStart, pubEnd - pubStart);

    std::cout << "Extracted private key (b64): " << privateKeyB64 << "\n";
    std::cout << "Extracted public key (b64): " << publicKeyB64 << "\n";

    auto privateKey = KeyPair::fromBase64(privateKeyB64);
    auto publicKey = KeyPair::fromBase64(publicKeyB64);

    std::cout << "Decoded private key size: " << privateKey.size() << " bytes\n";
    std::cout << "Decoded public key size: " << publicKey.size() << " bytes\n";

    if (privateKey.empty() || publicKey.empty()) {
        std::cerr << "Invalid key data in file\n";
        return false;
    }

    // First import the private key
    if (!keys.importPrivateKey(privateKey)) {
        std::cerr << "Failed to import private key\n";
        return false;
    }

    // Then import the public key
    if (!keys.importPublicKey(publicKey)) {
        std::cerr << "Failed to import public key\n";
        return false;
    }

    std::cout << "Successfully loaded keys\n";
    return true;
}

bool createCredential(const KeyPair& keys, const std::string& filename) {
    // Read the JSON template
    std::ifstream file(filename);
    if (!file) {
        std::cerr << "Failed to open template file: " << filename << "\n";
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string json = buffer.str();

    // Create and sign the credential
    auto vc = VerifiableCredential::fromJson(json);
    if (!vc) {
        std::cerr << "Failed to parse credential template\n";
        return false;
    }

    if (!vc->sign(keys)) {
        std::cerr << "Failed to sign credential\n";
        return false;
    }

    // Save the signed credential
    std::string outFile = filename.substr(0, filename.find_last_of('.')) + "_signed.json";
    std::ofstream out(outFile);
    if (!out) {
        std::cerr << "Failed to create output file: " << outFile << "\n";
        return false;
    }

    out << vc->toJson();
    std::cout << "Created signed credential: " << outFile << "\n";
    return true;
}

bool verifyCredential(const KeyPair& keys, const std::string& filename) {
    std::ifstream file(filename);
    if (!file) {
        std::cerr << "Failed to open file: " << filename << "\n";
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string json = buffer.str();

    auto vc = VerifiableCredential::fromJson(json);
    if (!vc) {
        std::cerr << "Failed to parse credential\n";
        return false;
    }

    auto publicKey = keys.exportPublicKey();
    if (!vc->verify(publicKey)) {
        std::cerr << "Invalid credential signature\n";
        return false;
    }

    std::cout << "Credential verified successfully!\n";
    std::cout << "Contents:\n" << json << "\n";
    return true;
}

bool signData(const KeyPair& keys, const std::string& data) {
    std::vector<uint8_t> message(data.begin(), data.end());
    auto signature = keys.sign(message);
    
    if (!signature) {
        std::cerr << "Failed to sign data\n";
        return false;
    }

    std::cout << "Data: " << data << "\n";
    std::cout << "Signature (hex): " << KeyPair::toHex(*signature) << "\n";
    std::cout << "Signature (b64): " << KeyPair::toBase64(*signature) << "\n";
    return true;
}

bool verifyData(const KeyPair& keys, const std::string& data, const std::string& signatureB64) {
    std::vector<uint8_t> message(data.begin(), data.end());
    auto signature = KeyPair::fromBase64(signatureB64);

    if (!keys.verify(message, signature)) {
        std::cerr << "Invalid signature\n";
        return false;
    }

    std::cout << "Signature verified successfully!\n";
    return true;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage();
        return 1;
    }

    std::string command = argv[1];
    const std::string KEY_FILE = "./keys.json";

    // Try to load existing keys from default location
    if (!g_keys_initialized) {
        if (loadKeys(g_keys, KEY_FILE)) {
            g_keys_initialized = true;
            std::cout << "Loaded existing keys from " << KEY_FILE << "\n";
            displayKeys(g_keys);
        }
    }

    if (command == "generate") {
        if (!g_keys.generate()) {
            std::cerr << "Failed to generate keys\n";
            return 1;
        }
        g_keys_initialized = true;
        std::cout << "Key pair generated successfully\n";
        std::cout << "Key type: ECDSA\n";
        std::cout << "Keys generated successfully:\n\n";
        displayKeys(g_keys);
        
        // Save the generated keys
        if (!saveKeys(g_keys, KEY_FILE)) {
            std::cerr << "Warning: Failed to save generated keys to " << KEY_FILE << "\n";
        } else {
            std::cout << "Keys saved to " << KEY_FILE << "\n";
        }
        return 0;
    }
    else if (command == "show-keys") {
        if (!g_keys_initialized) {
            std::cerr << "No keys available. Generate or import keys first.\n";
            return 1;
        }
        displayKeys(g_keys);
        return 0;
    }
    else if (command == "export-keys" && argc == 3) {
        if (!g_keys_initialized) {
            std::cerr << "No keys available. Generate or import keys first.\n";
            return 1;
        }
        if (!saveKeys(g_keys, argv[2])) {
            std::cerr << "Failed to save keys\n";
            return 1;
        }
        std::cout << "Keys exported to: " << argv[2] << "\n";
        displayKeys(g_keys);
        return 0;
    }
    else if (command == "import-keys" && argc == 3) {
        if (!loadKeys(g_keys, argv[2])) {
            std::cerr << "Failed to load keys\n";
            return 1;
        }
        g_keys_initialized = true;
        std::cout << "Keys imported from: " << argv[2] << "\n";
        displayKeys(g_keys);
        return 0;
    }
    else if (command == "create-credential" && argc == 3) {
        if (!g_keys_initialized) {
            std::cerr << "No keys available. Generate or import keys first.\n";
            return 1;
        }
        if (!createCredential(g_keys, argv[2])) {
            std::cerr << "Failed to create credential\n";
            return 1;
        }
        return 0;
    }
    else if (command == "verify-credential" && argc == 3) {
        if (!g_keys_initialized) {
            std::cerr << "No keys available. Generate or import keys first.\n";
            return 1;
        }
        if (!verifyCredential(g_keys, argv[2])) {
            std::cerr << "Failed to verify credential\n";
            return 1;
        }
        return 0;
    }
    else if (command == "sign-data" && argc == 3) {
        if (!g_keys_initialized) {
            std::cerr << "No keys available. Generate or import keys first.\n";
            return 1;
        }
        if (!signData(g_keys, argv[2])) {
            std::cerr << "Failed to sign data\n";
            return 1;
        }
        return 0;
    }
    else if (command == "verify-data" && argc == 4) {
        if (!g_keys_initialized) {
            std::cerr << "No keys available. Generate or import keys first.\n";
            return 1;
        }
        if (!verifyData(g_keys, argv[2], argv[3])) {
            std::cerr << "Failed to verify data\n";
            return 1;
        }
        return 0;
    }
    else {
        printUsage();
        return 1;
    }
} 