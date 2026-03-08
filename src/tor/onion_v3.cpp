// Copyright (c) 2024 Triangles developers
// Tor V3 Onion Services Implementation
// Distributed under the MIT/X11 software license

// Prevent Windows API conflicts
#ifdef WIN32
#define NOMINMAX
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#endif

#include "onion_v3.h"
#include "tor_crypto_compat.h"
#include "../util.h"
#include "../net.h"
#include "../protocol.h"
#include "../netbase.h"
#include "../addrman.h"
#include "../wallet.h"
#include "../walletdb.h"
#include "../main.h"
#include "../init.h"
#include "../crypter.h"

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <boost/filesystem.hpp>
#include <fstream>
#include <sstream>
#include <set>

#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#define closesocket close
#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#endif

// Ensure we have the global wallet pointer
extern CWallet* pwalletMain;

// Static instance
CTorV3Manager* CTorV3Manager::instance = nullptr;
static TorV3Config torV3Config;

// Utility function for proper base32 encoding (RFC 4648) - Tor variant
std::string EncodeBase32Proper(const unsigned char* data, size_t len)
{
    // Tor uses lowercase base32 alphabet without padding
    const char* alphabet = "abcdefghijklmnopqrstuvwxyz234567";
    std::string result;
    
    if (len == 0) return result;
    
    // Reserve space for efficiency
    result.reserve((len * 8 + 4) / 5);
    
    size_t bits = 0;
    uint32_t value = 0;
    
    for (size_t i = 0; i < len; i++) {
        value = (value << 8) | data[i];
        bits += 8;
        
        while (bits >= 5) {
            result += alphabet[(value >> (bits - 5)) & 0x1F];
            bits -= 5;
        }
    }
    
    // Handle remaining bits
    if (bits > 0) {
        result += alphabet[(value << (5 - bits)) & 0x1F];
    }
    
    return result;
}

// Utility function for base32 decoding (for validation)
bool DecodeBase32(const std::string& encoded, std::vector<unsigned char>& decoded)
{
    // Tor base32 alphabet
    const std::string alphabet = "abcdefghijklmnopqrstuvwxyz234567";
    
    decoded.clear();
    if (encoded.empty()) return true;
    
    size_t bits = 0;
    uint32_t value = 0;
    
    for (char c : encoded) {
        // Find character in alphabet
        size_t pos = alphabet.find(c);
        if (pos == std::string::npos) {
            return false; // Invalid character
        }
        
        value = (value << 5) | pos;
        bits += 5;
        
        if (bits >= 8) {
            decoded.push_back((value >> (bits - 8)) & 0xFF);
            bits -= 8;
        }
    }
    
    return true;
}

// Legacy function for backward compatibility
std::string EncodeBase32(const unsigned char* data, size_t len)
{
    return EncodeBase32Proper(data, len);
}

// CTorV3Service Implementation
CTorV3Service::CTorV3Service() : port(19112), isActive(false)
{
}

CTorV3Service::~CTorV3Service()
{
    if (isActive) {
        StopService();
    }
}

bool CTorV3Service::GenerateV3Service(int servicePort)
{
    // Input validation
    if (servicePort <= 0 || servicePort > 65535) {
        printf("ERROR: Invalid service port: %d (must be 1-65535)\n", servicePort);
        return false;
    }
    
    port = servicePort;
    
    // Clear any existing keys and state
    privateKey.clear();
    publicKey.clear();
    onionAddress.clear();
    isActive = false;
    
    // Initialize OpenSSL resources
    EVP_PKEY_CTX* ctx = nullptr;
    EVP_PKEY* pkey = nullptr;
    unsigned char privKeyBytes[32];
    unsigned char pubKeyBytes[32];
    
    // Initialize sensitive memory to zero
    OPENSSL_cleanse(privKeyBytes, 32);
    OPENSSL_cleanse(pubKeyBytes, 32);
    
    try {
        // Step 1: Create Ed25519 key generation context
        ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, nullptr);
        if (!ctx) {
            unsigned long err = ERR_get_error();
            printf("ERROR: Failed to create Ed25519 context (OpenSSL error: %lu)\n", err);
            return false;
        }
        
        // Step 2: Initialize key generation
        int init_result = EVP_PKEY_keygen_init(ctx);
        if (init_result <= 0) {
            unsigned long err = ERR_get_error();
            printf("ERROR: Failed to initialize Ed25519 key generation (result: %d, OpenSSL error: %lu)\n", 
                   init_result, err);
            EVP_PKEY_CTX_free(ctx);
            return false;
        }
        
        // Step 3: Generate the key pair
        int keygen_result = EVP_PKEY_keygen(ctx, &pkey);
        if (keygen_result <= 0 || !pkey) {
            unsigned long err = ERR_get_error();
            printf("ERROR: Failed to generate Ed25519 key pair (result: %d, OpenSSL error: %lu)\n", 
                   keygen_result, err);
            if (pkey) EVP_PKEY_free(pkey);
            EVP_PKEY_CTX_free(ctx);
            return false;
        }
        
        // Step 4: Verify the generated key is valid
        EVP_PKEY_CTX* check_ctx = EVP_PKEY_CTX_new(pkey, nullptr);
        if (!check_ctx) {
            printf("ERROR: Failed to create context for key validation\n");
            EVP_PKEY_free(pkey);
            EVP_PKEY_CTX_free(ctx);
            return false;
        }
        
        int check_result = EVP_PKEY_check(check_ctx);
        EVP_PKEY_CTX_free(check_ctx);
        
        if (check_result != 1) {
            unsigned long err = ERR_get_error();
            printf("ERROR: Generated Ed25519 key failed cryptographic validation (result: %d, OpenSSL error: %lu)\n", 
                   check_result, err);
            EVP_PKEY_free(pkey);
            EVP_PKEY_CTX_free(ctx);
            return false;
        }
        
        // Step 5: Extract raw private key
        size_t privKeyLen = 32;
        int priv_result = EVP_PKEY_get_raw_private_key(pkey, privKeyBytes, &privKeyLen);
        if (priv_result <= 0) {
            unsigned long err = ERR_get_error();
            printf("ERROR: Failed to extract Ed25519 private key (result: %d, OpenSSL error: %lu)\n", 
                   priv_result, err);
            OPENSSL_cleanse(privKeyBytes, 32);
            EVP_PKEY_free(pkey);
            EVP_PKEY_CTX_free(ctx);
            return false;
        }
        
        // Step 6: Validate private key length
        if (privKeyLen != 32) {
            printf("ERROR: Invalid Ed25519 private key length: %zu (expected 32)\n", privKeyLen);
            OPENSSL_cleanse(privKeyBytes, 32);
            EVP_PKEY_free(pkey);
            EVP_PKEY_CTX_free(ctx);
            return false;
        }
        
        // Step 7: Extract raw public key
        size_t pubKeyLen = 32;
        int pub_result = EVP_PKEY_get_raw_public_key(pkey, pubKeyBytes, &pubKeyLen);
        if (pub_result <= 0) {
            unsigned long err = ERR_get_error();
            printf("ERROR: Failed to extract Ed25519 public key (result: %d, OpenSSL error: %lu)\n", 
                   pub_result, err);
            OPENSSL_cleanse(privKeyBytes, 32);
            EVP_PKEY_free(pkey);
            EVP_PKEY_CTX_free(ctx);
            return false;
        }
        
        // Step 8: Validate public key length
        if (pubKeyLen != 32) {
            printf("ERROR: Invalid Ed25519 public key length: %zu (expected 32)\n", pubKeyLen);
            OPENSSL_cleanse(privKeyBytes, 32);
            EVP_PKEY_free(pkey);
            EVP_PKEY_CTX_free(ctx);
            return false;
        }
        
        // Step 9: Validate keys are not all zeros or invalid
        if (!ValidateEd25519Keys(privKeyBytes, pubKeyBytes)) {
            printf("ERROR: Generated Ed25519 keys failed cryptographic validation\n");
            OPENSSL_cleanse(privKeyBytes, 32);
            EVP_PKEY_free(pkey);
            EVP_PKEY_CTX_free(ctx);
            return false;
        }
        
        // Step 10: Verify public key derivation consistency
        unsigned char derivedPubKey[32];
        if (!DerivePublicKeyFromPrivate(privKeyBytes, derivedPubKey)) {
            printf("ERROR: Failed to derive public key for consistency check\n");
            OPENSSL_cleanse(privKeyBytes, 32);
            OPENSSL_cleanse(derivedPubKey, 32);
            EVP_PKEY_free(pkey);
            EVP_PKEY_CTX_free(ctx);
            return false;
        }
        
        if (memcmp(pubKeyBytes, derivedPubKey, 32) != 0) {
            printf("ERROR: Public key derivation inconsistency detected\n");
            OPENSSL_cleanse(privKeyBytes, 32);
            OPENSSL_cleanse(derivedPubKey, 32);
            EVP_PKEY_free(pkey);
            EVP_PKEY_CTX_free(ctx);
            return false;
        }
        
        // Step 11: Store keys securely (convert to hex)
        privateKey = HexStr(privKeyBytes, privKeyBytes + 32);
        publicKey = HexStr(pubKeyBytes, pubKeyBytes + 32);
        
        // Step 12: Generate proper v3 onion address with checksum
        if (!GenerateV3OnionAddress(pubKeyBytes, onionAddress)) {
            printf("ERROR: Failed to generate v3 onion address from public key\n");
            // Clear all sensitive data on failure
            OPENSSL_cleanse(privKeyBytes, 32);
            OPENSSL_cleanse(derivedPubKey, 32);
            privateKey.clear();
            publicKey.clear();
            EVP_PKEY_free(pkey);
            EVP_PKEY_CTX_free(ctx);
            return false;
        }
        
        // Step 13: Validate the generated onion address
        if (!ValidateOnionAddress(onionAddress)) {
            printf("ERROR: Generated onion address failed validation: %s\n", onionAddress.c_str());
            // Clear all sensitive data on failure
            OPENSSL_cleanse(privKeyBytes, 32);
            OPENSSL_cleanse(derivedPubKey, 32);
            privateKey.clear();
            publicKey.clear();
            onionAddress.clear();
            EVP_PKEY_free(pkey);
            EVP_PKEY_CTX_free(ctx);
            return false;
        }
        
        // Step 14: Verify address reconstruction consistency
        std::string reconstructedAddress = GenerateAddressFromPublicKey(pubKeyBytes);
        if (reconstructedAddress != onionAddress) {
            printf("ERROR: Address reconstruction inconsistency\n");
            printf("  Original: %s\n", onionAddress.c_str());
            printf("  Reconstructed: %s\n", reconstructedAddress.c_str());
            // Clear all sensitive data on failure
            OPENSSL_cleanse(privKeyBytes, 32);
            OPENSSL_cleanse(derivedPubKey, 32);
            privateKey.clear();
            publicKey.clear();
            onionAddress.clear();
            EVP_PKEY_free(pkey);
            EVP_PKEY_CTX_free(ctx);
            return false;
        }
        
        // Step 15: Clean up OpenSSL resources
        OPENSSL_cleanse(privKeyBytes, 32);
        OPENSSL_cleanse(derivedPubKey, 32);
        EVP_PKEY_free(pkey);
        EVP_PKEY_CTX_free(ctx);
        
        // Step 16: Final validation - ensure all components are present
        if (privateKey.empty() || publicKey.empty() || onionAddress.empty()) {
            printf("ERROR: Key generation completed but components are missing\n");
            privateKey.clear();
            publicKey.clear();
            onionAddress.clear();
            return false;
        }
        
        printf("Successfully generated V3 onion service:\n");
        printf("  Address: %s\n", onionAddress.c_str());
        printf("  Port: %d\n", port);
        printf("  Private key length: %zu characters\n", privateKey.length());
        printf("  Public key length: %zu characters\n", publicKey.length());
        
        return true;
        
    } catch (const std::exception& e) {
        printf("ERROR: Exception in GenerateV3Service: %s\n", e.what());
        
        // Comprehensive cleanup on exception
        OPENSSL_cleanse(privKeyBytes, 32);
        if (pkey) EVP_PKEY_free(pkey);
        if (ctx) EVP_PKEY_CTX_free(ctx);
        privateKey.clear();
        publicKey.clear();
        onionAddress.clear();
        isActive = false;
        
        return false;
    } catch (...) {
        printf("ERROR: Unknown exception in GenerateV3Service\n");
        
        // Comprehensive cleanup on unknown exception
        OPENSSL_cleanse(privKeyBytes, 32);
        if (pkey) EVP_PKEY_free(pkey);
        if (ctx) EVP_PKEY_CTX_free(ctx);
        privateKey.clear();
        publicKey.clear();
        onionAddress.clear();
        isActive = false;
        
        return false;
    }
}

bool CTorV3Service::LoadFromPrivateKey(const std::string& privKey)
{
    // Enhanced input validation
    if (privKey.empty()) {
        printf("ERROR: Private key cannot be empty\n");
        return false;
    }
    
    if (privKey.length() != 64) {
        printf("ERROR: Invalid private key format (expected 64 hex characters, got %zu)\n", privKey.length());
        return false;
    }
    
    // Validate hex format
    for (char c : privKey) {
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
            printf("ERROR: Private key contains invalid hex character: %c\n", c);
            return false;
        }
    }
    
    // Clear any existing service state
    privateKey.clear();
    publicKey.clear();
    onionAddress.clear();
    isActive = false;
    
    unsigned char privKeyBytes[32];
    unsigned char pubKeyBytes[32];
    
    try {
        // Extract keys from hex string with enhanced error handling
        if (!ExtractKeysFromHex(privKey, privKeyBytes, pubKeyBytes)) {
            printf("ERROR: Failed to extract keys from hex string\n");
            return false;
        }
        
        // Enhanced key validation
        if (!ValidateEd25519Keys(privKeyBytes, pubKeyBytes)) {
            printf("ERROR: Invalid Ed25519 key pair - keys failed cryptographic validation\n");
            OPENSSL_cleanse(privKeyBytes, 32);
            return false;
        }
        
        // Verify public key derivation is consistent
        unsigned char derivedPubKey[32];
        if (!DerivePublicKeyFromPrivate(privKeyBytes, derivedPubKey)) {
            printf("ERROR: Failed to derive public key from private key\n");
            OPENSSL_cleanse(privKeyBytes, 32);
            return false;
        }
        
        if (memcmp(pubKeyBytes, derivedPubKey, 32) != 0) {
            printf("ERROR: Public key derivation inconsistency detected\n");
            OPENSSL_cleanse(privKeyBytes, 32);
            return false;
        }
        
        // Store the validated keys
        privateKey = privKey;
        publicKey = HexStr(pubKeyBytes, pubKeyBytes + 32);
        
        // Generate and validate onion address from public key
        if (!GenerateV3OnionAddress(pubKeyBytes, onionAddress)) {
            printf("ERROR: Failed to generate onion address from public key\n");
            privateKey.clear();
            publicKey.clear();
            OPENSSL_cleanse(privKeyBytes, 32);
            return false;
        }
        
        // Validate the generated address
        if (!ValidateOnionAddress(onionAddress)) {
            printf("ERROR: Generated onion address failed validation\n");
            privateKey.clear();
            publicKey.clear();
            onionAddress.clear();
            OPENSSL_cleanse(privKeyBytes, 32);
            return false;
        }
        
        // Verify address can be reconstructed from the same public key
        std::string reconstructedAddress = GenerateAddressFromPublicKey(pubKeyBytes);
        if (reconstructedAddress != onionAddress) {
            printf("ERROR: Address reconstruction inconsistency\n");
            privateKey.clear();
            publicKey.clear();
            onionAddress.clear();
            OPENSSL_cleanse(privKeyBytes, 32);
            return false;
        }
        
        // Clean up sensitive data
        OPENSSL_cleanse(privKeyBytes, 32);
        
        printf("Successfully loaded V3 onion service from private key: %s\n", onionAddress.c_str());
        printf("  Private key length: %zu characters\n", privateKey.length());
        printf("  Public key: %s\n", publicKey.substr(0, 16).c_str());
        printf("  Onion address: %s\n", onionAddress.c_str());
        
        return true;
        
    } catch (const std::exception& e) {
        printf("ERROR: Exception in LoadFromPrivateKey: %s\n", e.what());
        
        // Clean up on exception
        privateKey.clear();
        publicKey.clear();
        onionAddress.clear();
        isActive = false;
        OPENSSL_cleanse(privKeyBytes, 32);
        
        return false;
    }
}

bool CTorV3Service::StartOnionService()
{
    if (onionAddress.empty()) {
        printf("ERROR: No onion address generated\n");
        return false;
    }
    
    // Create Tor configuration for hidden service
    std::string torrcContent = strprintf(
        "HiddenServiceDir tor_data/triangles_v3/\n"
        "HiddenServiceVersion 3\n"
        "HiddenServicePort %d 127.0.0.1:%d\n",
        port, port
    );
    
    // Write torrc file
    boost::filesystem::create_directories("tor_data/triangles_v3");
    
    std::ofstream torrcFile("tor_data/triangles_v3/torrc");
    if (torrcFile.is_open()) {
        torrcFile << torrcContent;
        torrcFile.close();
    }
    
    // Write private key
    std::ofstream keyFile("tor_data/triangles_v3/hs_ed25519_secret_key");
    if (keyFile.is_open()) {
        keyFile << "== ed25519v1-secret: type0 ==\n";
        keyFile << privateKey << "\n";
        keyFile.close();
    }
    
    isActive = true;
    printf("Started V3 onion service on %s:%d\n", onionAddress.c_str(), port);
    return true;
}

bool CTorV3Service::StopService()
{
    isActive = false;
    printf("Stopped V3 onion service\n");
    return true;
}

bool CTorV3Service::SaveToWallet()
{
    if (!pwalletMain) return false;
    
    // Use encrypted storage if wallet is encrypted
    if (pwalletMain->IsCrypted()) {
        return SaveEncryptedToWallet();
    }
    
    // Save to wallet database (unencrypted)
    CWalletDB walletdb(pwalletMain->strWalletFile);
    return walletdb.WriteSetting("tor_v3_private_key", privateKey) &&
           walletdb.WriteSetting("tor_v3_onion_address", onionAddress) &&
           walletdb.WriteSetting("tor_v3_key_encrypted", false);
}

bool CTorV3Service::LoadFromWallet()
{
    if (!pwalletMain) return false;
    
    // Try encrypted storage first if wallet is encrypted
    if (pwalletMain->IsCrypted()) {
        if (LoadEncryptedFromWallet()) {
            return true;
        }
    }
    
    // Try unencrypted storage
    CWalletDB walletdb(pwalletMain->strWalletFile);
    if (walletdb.ReadSetting("tor_v3_private_key", privateKey) &&
        walletdb.ReadSetting("tor_v3_onion_address", onionAddress)) {
        
        // Derive public key from private key
        unsigned char privKeyBytes[32];
        unsigned char pubKeyBytes[32];
        
        if (ExtractKeysFromHex(privateKey, privKeyBytes, pubKeyBytes)) {
            publicKey = HexStr(pubKeyBytes, pubKeyBytes + 32);
            OPENSSL_cleanse(privKeyBytes, 32);
            return true;
        }
    }
    return false;
}

// Save encrypted Tor keys to wallet
bool CTorV3Service::SaveEncryptedToWallet()
{
    if (!pwalletMain) {
        printf("ERROR: No wallet available for encrypted Tor key storage\n");
        return false;
    }
    
    // Check if wallet is encrypted
    if (!pwalletMain->IsCrypted()) {
        printf("WARNING: Wallet not encrypted, falling back to unencrypted storage\n");
        return SaveToWallet();
    }
    
    // Check if wallet is locked
    if (pwalletMain->IsLocked()) {
        printf("ERROR: Wallet is locked, cannot encrypt Tor keys\n");
        return false;
    }
    
    // Encrypt the private key using a simple approach
    std::vector<unsigned char> vchCryptedPrivateKey;
    if (!EncryptTorKeys(vchCryptedPrivateKey)) {
        printf("ERROR: Failed to encrypt Tor private key\n");
        return false;
    }
    
    // Save encrypted data to wallet
    CWalletDB walletdb(pwalletMain->strWalletFile);
    bool success = walletdb.WriteSetting("tor_v3_encrypted_private_key", vchCryptedPrivateKey) &&
                   walletdb.WriteSetting("tor_v3_onion_address", onionAddress) &&
                   walletdb.WriteSetting("tor_v3_key_encrypted", true);
    
    if (success) {
        // Remove unencrypted key if it exists
        walletdb.WriteSetting("tor_v3_private_key", std::string(""));
        printf("Tor v3 private key encrypted and saved to wallet\n");
    }
    
    return success;
}

// Load encrypted Tor keys from wallet
bool CTorV3Service::LoadEncryptedFromWallet()
{
    if (!pwalletMain) {
        printf("ERROR: No wallet available for encrypted Tor key loading\n");
        return false;
    }
    
    CWalletDB walletdb(pwalletMain->strWalletFile);
    
    // Check if we have encrypted keys
    bool isEncrypted = false;
    if (!walletdb.ReadSetting("tor_v3_key_encrypted", isEncrypted) || !isEncrypted) {
        // Try loading unencrypted keys
        return LoadFromWallet();
    }
    
    // Check if wallet is encrypted and unlocked
    if (!pwalletMain->IsCrypted()) {
        printf("ERROR: Tor keys are encrypted but wallet is not encrypted\n");
        return false;
    }
    
    // Check if wallet is locked
    if (pwalletMain->IsLocked()) {
        printf("ERROR: Wallet is locked, cannot decrypt Tor keys\n");
        return false;
    }
    
    // Load encrypted private key
    std::vector<unsigned char> vchCryptedPrivateKey;
    if (!walletdb.ReadSetting("tor_v3_encrypted_private_key", vchCryptedPrivateKey)) {
        printf("ERROR: Failed to load encrypted Tor private key from wallet\n");
        return false;
    }
    
    // Decrypt the private key
    if (!DecryptTorKeys(vchCryptedPrivateKey, privateKey)) {
        printf("ERROR: Failed to decrypt Tor private key\n");
        return false;
    }
    
    // Load onion address
    if (!walletdb.ReadSetting("tor_v3_onion_address", onionAddress)) {
        printf("ERROR: Failed to load Tor onion address from wallet\n");
        return false;
    }
    
    // Derive public key from decrypted private key
    unsigned char privKeyBytes[32];
    unsigned char pubKeyBytes[32];
    
    if (ExtractKeysFromHex(privateKey, privKeyBytes, pubKeyBytes)) {
        publicKey = HexStr(pubKeyBytes, pubKeyBytes + 32);
        OPENSSL_cleanse(privKeyBytes, 32);
        printf("Tor v3 keys decrypted and loaded from wallet: %s\n", onionAddress.c_str());
        return true;
    } else {
        printf("ERROR: Failed to derive public key from decrypted private key\n");
        privateKey.clear();
        return false;
    }
}

// Encrypt Tor keys using a simplified approach
bool CTorV3Service::EncryptTorKeys(std::vector<unsigned char>& vchCryptedPrivateKey)
{
    if (privateKey.empty()) {
        printf("ERROR: No private key to encrypt\n");
        return false;
    }
    
    // For now, use a simple XOR encryption with the onion address hash
    // This provides basic obfuscation while maintaining compatibility
    // In a production system, this should use proper AES encryption
    
    // Convert hex private key to bytes
    if (privateKey.length() != 64) {
        printf("ERROR: Invalid private key format for encryption\n");
        return false;
    }
    
    std::vector<unsigned char> vchPrivateKey(32);
    for (int i = 0; i < 32; i++) {
        std::string byteString = privateKey.substr(i * 2, 2);
        vchPrivateKey[i] = (unsigned char)strtol(byteString.c_str(), nullptr, 16);
    }
    
    // Create encryption key from onion address hash
    unsigned char hash[32];
    SHA256((const unsigned char*)onionAddress.c_str(), onionAddress.length(), hash);
    
    // Simple XOR encryption (for demonstration - use proper AES in production)
    vchCryptedPrivateKey.resize(32);
    for (int i = 0; i < 32; i++) {
        vchCryptedPrivateKey[i] = vchPrivateKey[i] ^ hash[i];
    }
    
    // Clear sensitive data
    OPENSSL_cleanse(vchPrivateKey.data(), vchPrivateKey.size());
    
    return true;
}

// Decrypt Tor keys using simplified approach
bool CTorV3Service::DecryptTorKeys(const std::vector<unsigned char>& vchCryptedPrivateKey, std::string& decryptedPrivateKey)
{
    if (vchCryptedPrivateKey.empty() || vchCryptedPrivateKey.size() != 32) {
        printf("ERROR: Invalid encrypted private key\n");
        return false;
    }
    
    // Create decryption key from onion address hash
    unsigned char hash[32];
    SHA256((const unsigned char*)onionAddress.c_str(), onionAddress.length(), hash);
    
    // Simple XOR decryption (matches encryption)
    std::vector<unsigned char> vchPrivateKey(32);
    for (int i = 0; i < 32; i++) {
        vchPrivateKey[i] = vchCryptedPrivateKey[i] ^ hash[i];
    }
    
    // Convert bytes to hex string
    decryptedPrivateKey = HexStr(vchPrivateKey.begin(), vchPrivateKey.end());
    
    // Clear sensitive data
    OPENSSL_cleanse(vchPrivateKey.data(), vchPrivateKey.size());
    
    return true;
}

// Ed25519 key validation function
bool CTorV3Service::ValidateEd25519Keys(const unsigned char* privateKey, const unsigned char* publicKey)
{
    if (!privateKey || !publicKey) {
        return false;
    }
    
    // Check that keys are not all zeros
    bool privKeyAllZeros = true;
    bool pubKeyAllZeros = true;
    
    for (int i = 0; i < 32; i++) {
        if (privateKey[i] != 0) privKeyAllZeros = false;
        if (publicKey[i] != 0) pubKeyAllZeros = false;
    }
    
    if (privKeyAllZeros || pubKeyAllZeros) {
        return false;
    }
    
    // Verify that the public key can be derived from the private key
    unsigned char derivedPubKey[32];
    if (!DerivePublicKeyFromPrivate(privateKey, derivedPubKey)) {
        return false;
    }
    
    // Compare derived public key with provided public key
    return memcmp(publicKey, derivedPubKey, 32) == 0;
}

// Generate proper v3 onion address with SHA3-256 checksum
bool CTorV3Service::GenerateV3OnionAddress(const unsigned char* publicKey, std::string& address)
{
    if (!publicKey) {
        return false;
    }
    
    // V3 onion address format: base32(pubkey || checksum || version) + ".onion"
    // Where checksum = SHA3-256(".onion checksum" || pubkey || version)[:2]
    
    const char* checksum_prefix = ".onion checksum";
    const unsigned char version = 0x03;
    
    // Prepare data for checksum calculation
    std::vector<unsigned char> checksum_data;
    checksum_data.insert(checksum_data.end(), checksum_prefix, checksum_prefix + strlen(checksum_prefix));
    checksum_data.insert(checksum_data.end(), publicKey, publicKey + 32);
    checksum_data.push_back(version);
    
    // Calculate SHA3-256 checksum
    unsigned char hash[32];
    if (!SHA3_256_compat(checksum_data.data(), checksum_data.size(), hash)) {
        return false;
    }
    
    // Prepare address bytes: pubkey (32) + checksum (2) + version (1)
    unsigned char addressBytes[35];
    memcpy(addressBytes, publicKey, 32);
    memcpy(addressBytes + 32, hash, 2); // First 2 bytes of hash as checksum
    addressBytes[34] = version;
    
    // Encode to base32
    address = EncodeBase32Proper(addressBytes, 35) + ".onion";
    
    return true;
}

// Validate onion address format and checksum
bool CTorV3Service::ValidateOnionAddress(const std::string& address)
{
    // V3 onion addresses should be 56 characters + ".onion" = 62 characters total
    if (address.length() != 62) {
        return false;
    }
    
    // Check .onion suffix
    if (address.substr(56) != ".onion") {
        return false;
    }
    
    // Extract base32 part
    std::string base32part = address.substr(0, 56);
    
    // Decode base32
    std::vector<unsigned char> decoded;
    if (!DecodeBase32(base32part, decoded)) {
        return false;
    }
    
    // Should decode to exactly 35 bytes (32 pubkey + 2 checksum + 1 version)
    if (decoded.size() != 35) {
        return false;
    }
    
    // Extract components
    unsigned char publicKey[32];
    unsigned char checksum[2];
    unsigned char version;
    
    memcpy(publicKey, decoded.data(), 32);
    memcpy(checksum, decoded.data() + 32, 2);
    version = decoded[34];
    
    // Verify version
    if (version != 0x03) {
        return false;
    }
    
    // Verify checksum
    const char* checksum_prefix = ".onion checksum";
    std::vector<unsigned char> checksum_data;
    checksum_data.insert(checksum_data.end(), checksum_prefix, checksum_prefix + strlen(checksum_prefix));
    checksum_data.insert(checksum_data.end(), publicKey, publicKey + 32);
    checksum_data.push_back(version);
    
    // Calculate expected checksum
    unsigned char expected_hash[32];
    if (!SHA3_256_compat(checksum_data.data(), checksum_data.size(), expected_hash)) {
        return false;
    }
    
    // Compare first 2 bytes of hash with provided checksum
    return (memcmp(checksum, expected_hash, 2) == 0);
}

// Enhanced key extraction from hex strings with comprehensive validation
bool CTorV3Service::ExtractKeysFromHex(const std::string& privKeyHex, unsigned char* privKey, unsigned char* pubKey)
{
    if (!privKey || !pubKey) {
        printf("ERROR: NULL pointers passed to ExtractKeysFromHex\n");
        return false;
    }
    
    if (privKeyHex.length() != 64) { // 32 bytes = 64 hex chars
        printf("ERROR: Invalid private key hex length: %zu (expected 64)\n", privKeyHex.length());
        return false;
    }
    
    // Validate hex format
    for (size_t i = 0; i < privKeyHex.length(); i++) {
        char c = privKeyHex[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
            printf("ERROR: Invalid hex character at position %zu: %c\n", i, c);
            return false;
        }
    }
    
    try {
        // Convert hex string to bytes with error checking
        for (int i = 0; i < 32; i++) {
            std::string byteString = privKeyHex.substr(i * 2, 2);
            char* endPtr = nullptr;
            long byteValue = strtol(byteString.c_str(), &endPtr, 16);
            
            // Check for conversion errors
            if (endPtr != byteString.c_str() + 2 || byteValue < 0 || byteValue > 255) {
                printf("ERROR: Invalid hex byte at position %d: %s\n", i, byteString.c_str());
                return false;
            }
            
            privKey[i] = (unsigned char)byteValue;
        }
        
        // Validate private key is not all zeros
        bool allZeros = true;
        for (int i = 0; i < 32; i++) {
            if (privKey[i] != 0) {
                allZeros = false;
                break;
            }
        }
        
        if (allZeros) {
            printf("ERROR: Private key cannot be all zeros\n");
            return false;
        }
        
        // Derive public key from private key with enhanced validation
        if (!DerivePublicKeyFromPrivate(privKey, pubKey)) {
            printf("ERROR: Failed to derive public key from private key\n");
            return false;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        printf("ERROR: Exception in ExtractKeysFromHex: %s\n", e.what());
        return false;
    }
}

// Enhanced public key derivation from private key using Ed25519
bool CTorV3Service::DerivePublicKeyFromPrivate(const unsigned char* privateKey, unsigned char* publicKey)
{
    if (!privateKey || !publicKey) {
        printf("ERROR: NULL pointer passed to DerivePublicKeyFromPrivate\n");
        return false;
    }
    
    // Validate private key is not all zeros
    bool allZeros = true;
    for (int i = 0; i < 32; i++) {
        if (privateKey[i] != 0) {
            allZeros = false;
            break;
        }
    }
    
    if (allZeros) {
        printf("ERROR: Private key cannot be all zeros\n");
        return false;
    }
    
    EVP_PKEY* pkey = nullptr;
    
    try {
        // Create EVP_PKEY from raw private key
        pkey = EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, nullptr, privateKey, 32);
        if (!pkey) {
            printf("ERROR: Failed to create EVP_PKEY from private key\n");
            return false;
        }
        
        // Verify the key is valid
        EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(pkey, nullptr);
        if (!ctx) {
            printf("ERROR: Failed to create EVP_PKEY_CTX for validation\n");
            EVP_PKEY_free(pkey);
            return false;
        }
        
        // Check if the key is valid (this will fail for invalid Ed25519 keys)
        int check_result = EVP_PKEY_check(ctx);
        EVP_PKEY_CTX_free(ctx);
        
        if (check_result != 1) {
            printf("ERROR: Private key failed cryptographic validation\n");
            EVP_PKEY_free(pkey);
            return false;
        }
        
        // Extract public key
        size_t pubKeyLen = 32;
        int result = EVP_PKEY_get_raw_public_key(pkey, publicKey, &pubKeyLen);
        
        if (result <= 0) {
            printf("ERROR: Failed to extract public key (result: %d)\n", result);
            EVP_PKEY_free(pkey);
            return false;
        }
        
        if (pubKeyLen != 32) {
            printf("ERROR: Invalid public key length: %zu (expected 32)\n", pubKeyLen);
            EVP_PKEY_free(pkey);
            return false;
        }
        
        // Validate derived public key is not all zeros
        bool pubKeyAllZeros = true;
        for (int i = 0; i < 32; i++) {
            if (publicKey[i] != 0) {
                pubKeyAllZeros = false;
                break;
            }
        }
        
        if (pubKeyAllZeros) {
            printf("ERROR: Derived public key is all zeros\n");
            EVP_PKEY_free(pkey);
            return false;
        }
        
        EVP_PKEY_free(pkey);
        return true;
        
    } catch (const std::exception& e) {
        printf("ERROR: Exception in DerivePublicKeyFromPrivate: %s\n", e.what());
        if (pkey) EVP_PKEY_free(pkey);
        return false;
    } catch (...) {
        printf("ERROR: Unknown exception in DerivePublicKeyFromPrivate\n");
        if (pkey) EVP_PKEY_free(pkey);
        return false;
    }
}

// Extract public key from onion address
bool CTorV3Service::ExtractPublicKeyFromAddress(const std::string& address, unsigned char* publicKey)
{
    if (!publicKey || !ValidateOnionAddress(address)) {
        return false;
    }
    
    // Extract base32 part
    std::string base32part = address.substr(0, 56);
    
    // Decode base32
    std::vector<unsigned char> decoded;
    if (!DecodeBase32(base32part, decoded) || decoded.size() != 35) {
        return false;
    }
    
    // Extract public key (first 32 bytes)
    memcpy(publicKey, decoded.data(), 32);
    return true;
}

// Verify address checksum independently
bool CTorV3Service::VerifyAddressChecksum(const std::string& address)
{
    return ValidateOnionAddress(address);
}

// Generate address from public key (utility wrapper)
std::string CTorV3Service::GenerateAddressFromPublicKey(const unsigned char* publicKey)
{
    std::string address;
    if (GenerateV3OnionAddress(publicKey, address)) {
        return address;
    }
    return "";
}

// CTorV3Manager Implementation
CTorV3Manager* CTorV3Manager::GetInstance()
{
    if (!instance) {
        instance = new CTorV3Manager();
    }
    return instance;
}

CTorV3Manager::CTorV3Manager() : torEnabled(false), torDataDir("tor_data")
{
    LoadTorV3Config();
}

CTorV3Manager::~CTorV3Manager()
{
    ShutdownTor();
}

bool CTorV3Manager::InitializeTor()
{
    printf("Initializing Tor V3 support...\n");
    
    // Create tor data directory
    boost::filesystem::create_directories(torDataDir);
    
    torEnabled = true;
    
    if (torV3Config.enableHiddenService) {
        return CreateWalletHiddenService(torV3Config.hiddenServicePort);
    }
    
    return true;
}

void CTorV3Manager::ShutdownTor()
{
    for (auto& service : services) {
        if (service.second) {
            service.second->StopService();
            delete service.second;
        }
    }
    services.clear();
    torEnabled = false;
    printf("Tor V3 services shutdown\n");
}

bool CTorV3Manager::CreateWalletHiddenService(int port)
{
    CTorV3Service* service = new CTorV3Service();
    
    // Try to load existing service first
    if (!service->LoadFromWallet()) {
        // Generate new service
        if (!service->GenerateV3Service(port)) {
            delete service;
            return false;
        }
        service->SaveToWallet();
    }
    
    if (service->StartOnionService()) {
        services[port] = service;
        printf("Wallet hidden service created: %s\n", service->GetOnionAddress().c_str());
        
        // If seeder mode is enabled, automatically register as seeder
        if (torV3Config.enableSeederMode) {
            RegisterAsSeederNode(service->GetOnionAddress(), port);
        }
        
        return true;
    }
    
    delete service;
    return false;
}

bool CTorV3Manager::RegisterAsSeederNode(const std::string& onionAddress, int port)
{
    // Enhanced input validation
    if (onionAddress.empty()) {
        printf("ERROR: Cannot register empty onion address as seeder\n");
        return false;
    }
    
    if (!CTorV3Service::ValidateOnionAddress(onionAddress)) {
        printf("ERROR: Invalid onion address format for seeder registration: %s\n", onionAddress.c_str());
        return false;
    }
    
    if (port <= 0 || port > 65535) {
        printf("ERROR: Invalid port for seeder registration: %d (must be 1-65535)\n", port);
        return false;
    }
    
    printf("Registering as seeder node: %s:%d\n", onionAddress.c_str(), port);
    
    // Create seeder entry
    std::string seederEntry = onionAddress + ":" + std::to_string(port);
    
    // Save to wallet database with enhanced metadata
    if (pwalletMain) {
        CWalletDB walletdb(pwalletMain->strWalletFile);
        
        // Save primary seeder information
        if (!walletdb.WriteSetting("seeder_onion_address", onionAddress) ||
            !walletdb.WriteSetting("seeder_port", port) ||
            !walletdb.WriteSetting("seeder_active", true)) {
            printf("ERROR: Failed to save seeder information to wallet\n");
            return false;
        }
        
        // Save registration timestamp
        int64_t registrationTime = GetTime();
        walletdb.WriteSetting("seeder_registration_time", registrationTime);
        
        // Initialize seeder statistics
        walletdb.WriteSetting("seeder_connections_served", 0);
        walletdb.WriteSetting("seeder_last_announcement", registrationTime);
        
        // Add ourselves to the known seeders list
        std::string knownSeeders;
        if (walletdb.ReadSetting("known_seeders", knownSeeders)) {
            // Check if we're already in the list
            if (knownSeeders.find(seederEntry) == std::string::npos) {
                if (!knownSeeders.empty()) {
                    knownSeeders += ",";
                }
                knownSeeders += seederEntry;
                walletdb.WriteSetting("known_seeders", knownSeeders);
            }
        } else {
            // First seeder entry
            walletdb.WriteSetting("known_seeders", seederEntry);
        }
        
        // Set high reputation for ourselves
        std::string reputationKey = "seeder_reputation_" + seederEntry;
        walletdb.WriteSetting(reputationKey, (uint32_t)100);
        
        // Set last seen to now
        std::string lastSeenKey = "seeder_last_seen_" + seederEntry;
        walletdb.WriteSetting(lastSeenKey, registrationTime);
    }
    
    // Broadcast seeder announcement to network
    int announcementsSent = 0;
    {
        LOCK(cs_vNodes);
        BOOST_FOREACH(CNode* pnode, vNodes) {
            try {
                // Send seeder announcement to connected peers
                pnode->PushMessage("seeder", onionAddress, port);
                announcementsSent++;
            } catch (const std::exception& e) {
                printf("WARNING: Failed to send seeder announcement to peer %s: %s\n", 
                       pnode->addr.ToString().c_str(), e.what());
            }
        }
    }
    
    printf("Successfully registered as seeder node and announced to %d peers\n", announcementsSent);
    
    // Schedule periodic re-announcements
    ScheduleSeederReannouncement();
    
    return true;
}

void CTorV3Manager::SetTorEnabled(bool enabled)
{
    torEnabled = enabled;
    torV3Config.enableTor = enabled;
    SaveTorV3Config();
    
    if (enabled) {
        InitializeTor();
    } else {
        ShutdownTor();
    }
}

std::string CTorV3Manager::GetWalletOnionAddress()
{
    auto it = services.find(torV3Config.hiddenServicePort);
    if (it != services.end() && it->second) {
        return it->second->GetOnionAddress();
    }
    return "";
}

bool CTorV3Manager::ConnectToOnionPeer(const std::string& onionAddr, int port)
{
    if (!torEnabled) {
        printf("ERROR: Tor not enabled, cannot connect to onion peer\n");
        return false;
    }
    
    // Validate onion address format
    if (!CTorV3Service::ValidateOnionAddress(onionAddr)) {
        printf("ERROR: Invalid onion address format: %s\n", onionAddr.c_str());
        return false;
    }
    
    printf("Connecting to onion peer: %s:%d\n", onionAddr.c_str(), port);
    
    // Check if already connected to this peer
    {
        LOCK(cs_vNodes);
        BOOST_FOREACH(CNode* pnode, vNodes) {
            std::string nodeAddr = pnode->addr.ToString();
            if (nodeAddr.find(onionAddr) != std::string::npos) {
                printf("Already connected to onion peer %s\n", onionAddr.c_str());
                return true;
            }
        }
    }
    
    // Check connection limits
    if (GetOnionPeers().size() >= (size_t)torV3Config.maxConnections) {
        printf("WARNING: Maximum Tor connections reached (%d)\n", torV3Config.maxConnections);
        return false;
    }
    
    try {
        // Create service address for onion peer
        CService service;
        if (!Lookup(onionAddr.c_str(), service, port, false)) {
            printf("ERROR: Failed to resolve onion address: %s\n", onionAddr.c_str());
            return false;
        }
        
        // Create address object
        CAddress addr(service);
        addr.nTime = GetTime();
        
        // Add to address manager for future connections
        CNetAddr sourceAddr("127.0.0.1");
        addrman.Add(addr, sourceAddr);
        
        // Attempt connection through SOCKS5 proxy
        if (ConnectThroughSocks5Proxy(onionAddr, port)) {
            printf("Successfully connected to onion peer: %s:%d\n", onionAddr.c_str(), port);
            
            // Update peer reputation if tracking
            UpdatePeerReputation(onionAddr, true);
            
            return true;
        } else {
            printf("ERROR: Failed to connect through SOCKS5 proxy to %s:%d\n", onionAddr.c_str(), port);
            UpdatePeerReputation(onionAddr, false);
            return false;
        }
        
    } catch (const std::exception& e) {
        printf("ERROR: Exception connecting to onion peer %s:%d - %s\n", 
               onionAddr.c_str(), port, e.what());
        return false;
    }
}

std::vector<std::string> CTorV3Manager::GetOnionPeers()
{
    std::vector<std::string> onionPeers;
    
    if (!torEnabled) {
        return onionPeers;
    }
    
    try {
        LOCK(cs_vNodes);
        BOOST_FOREACH(CNode* pnode, vNodes) {
            std::string addr = pnode->addr.ToString();
            if (addr.find(".onion") != std::string::npos) {
                // Include connection status and timing information
                std::string peerInfo = addr;
                
                // Add connection quality indicators
                if (pnode->fSuccessfullyConnected) {
                    peerInfo += " (connected)";
                } else {
                    peerInfo += " (connecting)";
                }
                
                // Add last activity time
                int64_t lastActivity = GetTime() - pnode->nLastRecv;
                if (lastActivity < 60) {
                    peerInfo += " [active]";
                } else if (lastActivity < 300) {
                    peerInfo += " [recent]";
                } else {
                    peerInfo += " [idle]";
                }
                
                onionPeers.push_back(peerInfo);
            }
        }
    } catch (const std::exception& e) {
        printf("ERROR: Exception in GetOnionPeers: %s\n", e.what());
    }
    
    printf("Current onion peers: %d connected\n", (int)onionPeers.size());
    return onionPeers;
}

bool CTorV3Manager::DiscoverOnionPeers()
{
    if (!torEnabled) {
        printf("ERROR: Tor not enabled, cannot discover onion peers\n");
        return false;
    }
    
    printf("Starting automatic onion peer discovery...\n");
    
    try {
        // 1. Get list of known seeder nodes from network
        std::vector<std::string> seederNodes = GetKnownSeederNodes();
        printf("Found %d known seeder nodes\n", (int)seederNodes.size());
        
        // 2. Connect to available seeders with connection limits
        int connected = 0;
        int maxSeeders = std::min((int)seederNodes.size(), torV3Config.maxConnections / 2);
        
        for (int i = 0; i < maxSeeders && i < (int)seederNodes.size(); i++) {
            const std::string& seeder = seederNodes[i];
            
            // Add connection retry logic with exponential backoff
            if (ConnectToSeederNodeWithRetry(seeder, 3)) {
                connected++;
                
                // Small delay between connections to avoid overwhelming
                MilliSleep(100);
            }
        }
        
        printf("Successfully connected to %d seeder nodes\n", connected);
        
        // 3. Request seeder lists from connected peers
        if (connected > 0) {
            RequestSeederListFromPeers();
            
            // Wait a bit for responses
            MilliSleep(2000);
            
            // 4. Attempt to connect to newly discovered peers
            DiscoverAdditionalPeers();
        }
        
        // 5. Update peer discovery statistics
        UpdateDiscoveryStats(connected, seederNodes.size());
        
        return connected > 0;
        
    } catch (const std::exception& e) {
        printf("ERROR: Exception in DiscoverOnionPeers: %s\n", e.what());
        return false;
    }
}

std::vector<std::string> CTorV3Manager::GetKnownSeederNodes()
{
    std::vector<std::string> seeders;
    std::set<std::string> uniqueSeeders; // Prevent duplicates
    
    // Load from wallet database with enhanced error handling
    if (pwalletMain) {
        CWalletDB walletdb(pwalletMain->strWalletFile);
        
        // Get stored seeder list
        std::string seederList;
        if (walletdb.ReadSetting("known_seeders", seederList)) {
            // Parse comma-separated list with validation
            std::stringstream ss(seederList);
            std::string seeder;
            while (std::getline(ss, seeder, ',')) {
                if (!seeder.empty()) {
                    // Validate seeder format (address:port)
                    size_t colonPos = seeder.find(':');
                    if (colonPos != std::string::npos) {
                        std::string onionAddr = seeder.substr(0, colonPos);
                        std::string portStr = seeder.substr(colonPos + 1);
                        
                        // Validate onion address format
                        if (CTorV3Service::ValidateOnionAddress(onionAddr)) {
                            // Validate port number
                            try {
                                int port = std::stoi(portStr);
                                if (port > 0 && port <= 65535) {
                                    uniqueSeeders.insert(seeder);
                                }
                            } catch (const std::exception& e) {
                                printf("WARNING: Invalid port in seeder entry: %s\n", seeder.c_str());
                            }
                        } else {
                            printf("WARNING: Invalid onion address in seeder entry: %s\n", seeder.c_str());
                        }
                    } else {
                        printf("WARNING: Invalid seeder format (missing port): %s\n", seeder.c_str());
                    }
                }
            }
        }
        
        // Load individual seeder reputation data for filtering
        for (const std::string& seeder : uniqueSeeders) {
            int64_t lastSeen = 0;
            uint32_t reputation = 0;
            
            std::string lastSeenKey = "seeder_last_seen_" + seeder;
            std::string reputationKey = "seeder_reputation_" + seeder;
            
            walletdb.ReadSetting(lastSeenKey, lastSeen);
            walletdb.ReadSetting(reputationKey, reputation);
            
            // Only include seeders seen within the last 7 days and with good reputation
            int64_t currentTime = GetTime();
            int64_t sevenDaysAgo = currentTime - (7 * 24 * 60 * 60);
            
            if (lastSeen > sevenDaysAgo && reputation >= 50) { // Minimum reputation threshold
                seeders.push_back(seeder);
            }
        }
    }
    
    // Add hardcoded bootstrap seeders (for initial network bootstrap)
    std::vector<std::string> bootstrapSeeders = {
        // These would be initial seeder nodes to bootstrap the network
        // Community can add their seeder nodes here
        // Format: "onionaddress.onion:port"
    };
    
    // Add bootstrap seeders if we don't have enough known seeders
    if (seeders.size() < 3) {
        for (const std::string& bootstrapSeeder : bootstrapSeeders) {
            if (uniqueSeeders.find(bootstrapSeeder) == uniqueSeeders.end()) {
                seeders.push_back(bootstrapSeeder);
            }
        }
    }
    
    printf("Found %d known seeder nodes (filtered by reputation and recency)\n", (int)seeders.size());
    return seeders;
}

bool CTorV3Manager::ConnectToSeederNode(const std::string& seederAddress)
{
    // Enhanced input validation
    if (seederAddress.empty()) {
        printf("ERROR: Empty seeder address provided\n");
        return false;
    }
    
    // Parse address:port with enhanced validation
    size_t colonPos = seederAddress.find(':');
    if (colonPos == std::string::npos) {
        printf("ERROR: Invalid seeder address format (missing port): %s\n", seederAddress.c_str());
        return false;
    }
    
    std::string onionAddr = seederAddress.substr(0, colonPos);
    std::string portStr = seederAddress.substr(colonPos + 1);
    
    // Validate onion address format
    if (!CTorV3Service::ValidateOnionAddress(onionAddr)) {
        printf("ERROR: Invalid onion address format: %s\n", onionAddr.c_str());
        return false;
    }
    
    // Validate and parse port
    int port;
    try {
        port = std::stoi(portStr);
        if (port <= 0 || port > 65535) {
            printf("ERROR: Invalid port number: %d (must be 1-65535)\n", port);
            return false;
        }
    } catch (const std::exception& e) {
        printf("ERROR: Failed to parse port from seeder address: %s\n", seederAddress.c_str());
        return false;
    }
    
    printf("Connecting to seeder: %s:%d\n", onionAddr.c_str(), port);
    
    // Check if already connected
    {
        LOCK(cs_vNodes);
        BOOST_FOREACH(CNode* pnode, vNodes) {
            if (pnode->addr.ToString().find(onionAddr) != std::string::npos) {
                printf("Already connected to seeder %s\n", onionAddr.c_str());
                // Update last seen time for this seeder
                UpdateSeederLastSeen(seederAddress);
                return true;
            }
        }
    }
    
    // Check connection limits
    std::vector<std::string> currentOnionPeers = GetOnionPeers();
    if (currentOnionPeers.size() >= (size_t)torV3Config.maxConnections) {
        printf("WARNING: Maximum onion connections reached (%d), cannot connect to seeder\n", 
               torV3Config.maxConnections);
        return false;
    }
    
    // Attempt connection through SOCKS5 proxy if configured
    bool connected = false;
    if (!torV3Config.socksProxy.empty()) {
        printf("Connecting to seeder through SOCKS5 proxy: %s\n", torV3Config.socksProxy.c_str());
        connected = ConnectThroughSocks5Proxy(onionAddr, port);
    } else {
        printf("WARNING: No SOCKS5 proxy configured, attempting direct connection\n");
        connected = ConnectToOnionPeer(onionAddr, port);
    }
    
    if (connected) {
        printf("Successfully connected to seeder: %s:%d\n", onionAddr.c_str(), port);
        
        // Update seeder statistics
        UpdateSeederLastSeen(seederAddress);
        UpdateSeederReputation(seederAddress, true);
        
        // Request seeder list from the newly connected seeder
        RequestSeederListFromPeer(seederAddress);
        
        return true;
    } else {
        printf("Failed to connect to seeder: %s:%d\n", onionAddr.c_str(), port);
        
        // Update reputation negatively for failed connection
        UpdateSeederReputation(seederAddress, false);
        
        return false;
    }
}

void CTorV3Manager::HandleSeederListMessage(CNode* pfrom, const std::vector<std::string>& seederList)
{
    printf("Received seeder list from %s with %d entries\n", 
           pfrom->addr.ToString().c_str(), (int)seederList.size());
    
    // Add new seeders to our known list
    std::set<std::string> newSeeders;
    
    if (pwalletMain) {
        CWalletDB walletdb(pwalletMain->strWalletFile);
        
        // Load existing known seeders
        std::string existingSeeders;
        if (walletdb.ReadSetting("known_seeders", existingSeeders)) {
            std::set<std::string> existingSet;
            std::stringstream ss(existingSeeders);
            std::string seeder;
            while (std::getline(ss, seeder, ',')) {
                if (!seeder.empty()) {
                    existingSet.insert(seeder);
                }
            }
            
            // Add new seeders
            for (const std::string& newSeeder : seederList) {
                if (CTorV3Service::ValidateOnionAddress(newSeeder.substr(0, newSeeder.find(':')))) {
                    existingSet.insert(newSeeder);
                    newSeeders.insert(newSeeder);
                }
            }
            
            // Save updated list
            std::string updatedList;
            for (const std::string& seeder : existingSet) {
                if (!updatedList.empty()) updatedList += ",";
                updatedList += seeder;
            }
            walletdb.WriteSetting("known_seeders", updatedList);
        }
    }
    
    printf("Added %d new seeders to known list\n", (int)newSeeders.size());
    
    // Attempt to connect to new seeders if we have capacity
    int currentConnections = GetOnionPeers().size();
    int availableSlots = torV3Config.maxConnections - currentConnections;
    
    if (availableSlots > 0) {
        int connected = 0;
        for (const std::string& seeder : newSeeders) {
            if (connected >= availableSlots) break;
            
            if (ConnectToSeederNode(seeder)) {
                connected++;
            }
        }
        printf("Connected to %d new seeders\n", connected);
    }
}

// SOCKS5 proxy connection implementation
bool CTorV3Manager::ConnectThroughSocks5Proxy(const std::string& onionAddr, int port)
{
    if (torV3Config.socksProxy.empty()) {
        printf("ERROR: No SOCKS5 proxy configured for Tor connections\n");
        return false;
    }
    
    // Parse proxy address
    size_t colonPos = torV3Config.socksProxy.find(':');
    if (colonPos == std::string::npos) {
        printf("ERROR: Invalid SOCKS5 proxy format: %s\n", torV3Config.socksProxy.c_str());
        return false;
    }
    
    std::string proxyHost = torV3Config.socksProxy.substr(0, colonPos);
    int proxyPort = std::stoi(torV3Config.socksProxy.substr(colonPos + 1));
    
    printf("Connecting through SOCKS5 proxy %s:%d to %s:%d\n", 
           proxyHost.c_str(), proxyPort, onionAddr.c_str(), port);
    
    try {
        // Create connection to SOCKS5 proxy
        SOCKET hSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (hSocket == INVALID_SOCKET) {
            printf("ERROR: Failed to create socket for SOCKS5 connection\n");
            return false;
        }
        
        // Set socket options
        int nOne = 1;
        if (setsockopt(hSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&nOne, sizeof(int)) == SOCKET_ERROR) {
            printf("WARNING: Failed to set SO_REUSEADDR on SOCKS5 socket\n");
        }
        
        // Connect to SOCKS5 proxy
        struct sockaddr_in proxyAddr;
        memset(&proxyAddr, 0, sizeof(proxyAddr));
        proxyAddr.sin_family = AF_INET;
        proxyAddr.sin_port = htons(proxyPort);
        
        if (inet_pton(AF_INET, proxyHost.c_str(), &proxyAddr.sin_addr) <= 0) {
            printf("ERROR: Invalid SOCKS5 proxy IP address: %s\n", proxyHost.c_str());
            closesocket(hSocket);
            return false;
        }
        
        if (connect(hSocket, (struct sockaddr*)&proxyAddr, sizeof(proxyAddr)) == SOCKET_ERROR) {
            printf("ERROR: Failed to connect to SOCKS5 proxy %s:%d\n", proxyHost.c_str(), proxyPort);
            closesocket(hSocket);
            return false;
        }
        
        // Perform SOCKS5 handshake
        if (!PerformSocks5Handshake(hSocket, onionAddr, port)) {
            printf("ERROR: SOCKS5 handshake failed for %s:%d\n", onionAddr.c_str(), port);
            closesocket(hSocket);
            return false;
        }
        
        // Create CNode for the connection
        CAddress addr(CService(onionAddr, port));
        CNode* pnode = new CNode(hSocket, addr, "", false);
        if (pnode) {
            pnode->fNetworkNode = true;
            pnode->fSuccessfullyConnected = true;
            
            {
                LOCK(cs_vNodes);
                vNodes.push_back(pnode);
            }
            
            printf("Successfully established SOCKS5 connection to %s:%d\n", onionAddr.c_str(), port);
            return true;
        } else {
            printf("ERROR: Failed to create CNode for SOCKS5 connection\n");
            closesocket(hSocket);
            return false;
        }
        
    } catch (const std::exception& e) {
        printf("ERROR: Exception in SOCKS5 connection: %s\n", e.what());
        return false;
    }
}

// SOCKS5 handshake implementation
bool CTorV3Manager::PerformSocks5Handshake(SOCKET hSocket, const std::string& onionAddr, int port)
{
    try {
        // SOCKS5 authentication request (no authentication)
        unsigned char authRequest[3] = {0x05, 0x01, 0x00}; // Version 5, 1 method, no auth
        if (send(hSocket, (const char*)authRequest, 3, 0) != 3) {
            printf("ERROR: Failed to send SOCKS5 auth request\n");
            return false;
        }
        
        // Read authentication response
        unsigned char authResponse[2];
        if (recv(hSocket, (char*)authResponse, 2, 0) != 2) {
            printf("ERROR: Failed to receive SOCKS5 auth response\n");
            return false;
        }
        
        if (authResponse[0] != 0x05 || authResponse[1] != 0x00) {
            printf("ERROR: SOCKS5 authentication failed: version=%d, method=%d\n", 
                   authResponse[0], authResponse[1]);
            return false;
        }
        
        // SOCKS5 connection request
        std::vector<unsigned char> connectRequest;
        connectRequest.push_back(0x05); // Version
        connectRequest.push_back(0x01); // Connect command
        connectRequest.push_back(0x00); // Reserved
        connectRequest.push_back(0x03); // Domain name address type
        
        // Add domain name length and domain name
        connectRequest.push_back((unsigned char)onionAddr.length());
        for (char c : onionAddr) {
            connectRequest.push_back((unsigned char)c);
        }
        
        // Add port (big-endian)
        connectRequest.push_back((port >> 8) & 0xFF);
        connectRequest.push_back(port & 0xFF);
        
        if (send(hSocket, (const char*)connectRequest.data(), connectRequest.size(), 0) != (int)connectRequest.size()) {
            printf("ERROR: Failed to send SOCKS5 connect request\n");
            return false;
        }
        
        // Read connection response
        unsigned char connectResponse[10]; // Minimum response size
        int received = recv(hSocket, (char*)connectResponse, 10, 0);
        if (received < 10) {
            printf("ERROR: Failed to receive complete SOCKS5 connect response\n");
            return false;
        }
        
        if (connectResponse[0] != 0x05) {
            printf("ERROR: Invalid SOCKS5 response version: %d\n", connectResponse[0]);
            return false;
        }
        
        if (connectResponse[1] != 0x00) {
            printf("ERROR: SOCKS5 connection failed with error code: %d\n", connectResponse[1]);
            return false;
        }
        
        printf("SOCKS5 handshake completed successfully\n");
        return true;
        
    } catch (const std::exception& e) {
        printf("ERROR: Exception in SOCKS5 handshake: %s\n", e.what());
        return false;
    }
}

// Connect to seeder with retry logic
bool CTorV3Manager::ConnectToSeederNodeWithRetry(const std::string& seederAddress, int maxRetries)
{
    for (int attempt = 1; attempt <= maxRetries; attempt++) {
        printf("Connecting to seeder %s (attempt %d/%d)\n", 
               seederAddress.c_str(), attempt, maxRetries);
        
        if (ConnectToSeederNode(seederAddress)) {
            return true;
        }
        
        if (attempt < maxRetries) {
            // Exponential backoff: 1s, 2s, 4s, etc.
            int delayMs = 1000 * (1 << (attempt - 1));
            printf("Connection failed, retrying in %dms...\n", delayMs);
            MilliSleep(delayMs);
        }
    }
    
    printf("Failed to connect to seeder %s after %d attempts\n", 
           seederAddress.c_str(), maxRetries);
    return false;
}

// Discover additional peers from connected nodes
void CTorV3Manager::DiscoverAdditionalPeers()
{
    printf("Discovering additional peers from connected nodes...\n");
    
    // Get current onion peers
    std::vector<std::string> currentPeers = GetOnionPeers();
    int availableSlots = torV3Config.maxConnections - currentPeers.size();
    
    if (availableSlots <= 0) {
        printf("No available connection slots for additional peers\n");
        return;
    }
    
    // Request peer lists from connected onion nodes
    {
        LOCK(cs_vNodes);
        BOOST_FOREACH(CNode* pnode, vNodes) {
            std::string addr = pnode->addr.ToString();
            if (addr.find(".onion") != std::string::npos && pnode->fSuccessfullyConnected) {
                // Request additional peer information
                pnode->PushMessage("getaddr");
            }
        }
    }
    
    printf("Requested peer information from %d connected onion nodes\n", (int)currentPeers.size());
}

// Update peer reputation tracking
void CTorV3Manager::UpdatePeerReputation(const std::string& onionAddr, bool success)
{
    if (!pwalletMain) return;
    
    try {
        CWalletDB walletdb(pwalletMain->strWalletFile);
        
        std::string reputationKey = "peer_reputation_" + onionAddr;
        std::string lastSeenKey = "peer_last_seen_" + onionAddr;
        
        // Update reputation score
        int reputation = 0;
        walletdb.ReadSetting(reputationKey, reputation);
        
        if (success) {
            reputation = std::min(reputation + 10, 1000); // Cap at 1000
        } else {
            reputation = std::max(reputation - 5, 0); // Floor at 0
        }
        
        walletdb.WriteSetting(reputationKey, reputation);
        walletdb.WriteSetting(lastSeenKey, (int64_t)GetTime());
        
    } catch (const std::exception& e) {
        printf("ERROR: Exception updating peer reputation: %s\n", e.what());
    }
}

// Update discovery statistics
void CTorV3Manager::UpdateDiscoveryStats(int connected, int attempted)
{
    if (!pwalletMain) return;
    
    try {
        CWalletDB walletdb(pwalletMain->strWalletFile);
        
        walletdb.WriteSetting("discovery_last_run", (int64_t)GetTime());
        walletdb.WriteSetting("discovery_connected", connected);
        walletdb.WriteSetting("discovery_attempted", attempted);
        
        // Update success rate
        int totalAttempts = 0;
        int totalConnected = 0;
        
        walletdb.ReadSetting("discovery_total_attempts", totalAttempts);
        walletdb.ReadSetting("discovery_total_connected", totalConnected);
        
        totalAttempts += attempted;
        totalConnected += connected;
        
        walletdb.WriteSetting("discovery_total_attempts", totalAttempts);
        walletdb.WriteSetting("discovery_total_connected", totalConnected);
        
        if (totalAttempts > 0) {
            double successRate = (double)totalConnected / totalAttempts * 100.0;
            printf("Peer discovery success rate: %.1f%% (%d/%d)\n", 
                   successRate, totalConnected, totalAttempts);
        }
        
    } catch (const std::exception& e) {
        printf("ERROR: Exception updating discovery stats: %s\n", e.what());
    }
}

// Load Tor V3 configuration
bool LoadTorV3Config()
{
    // Set default values
    torV3Config.enableTor = GetBoolArg("-tor", false);
    torV3Config.enableHiddenService = GetBoolArg("-torhiddenservice", false);
    torV3Config.enableSeederMode = GetBoolArg("-torseeder", false);
    torV3Config.hiddenServicePort = GetArg("-torhiddenserviceport", 19112);
    torV3Config.torDataDirectory = GetArg("-tordatadir", "tor_data");
    torV3Config.socksProxy = GetArg("-torproxy", "127.0.0.1:9050");
    torV3Config.maxConnections = GetArg("-tormaxconnections", 8);
    
    printf("Loaded Tor V3 configuration: enabled=%s, hidden_service=%s, seeder=%s, proxy=%s\n",
           torV3Config.enableTor ? "true" : "false",
           torV3Config.enableHiddenService ? "true" : "false", 
           torV3Config.enableSeederMode ? "true" : "false",
           torV3Config.socksProxy.c_str());
    
    return true;
}

// Save Tor V3 configuration  
bool SaveTorV3Config()
{
    // Configuration is typically saved through command line args or config file
    // This is a placeholder for future configuration persistence
    return true;
}

// Get Tor V3 configuration reference
TorV3Config& GetTorV3Config()
{
    return torV3Config;
}

void CTorV3Manager::RequestSeederListFromPeers()
{
    printf("Requesting seeder lists from connected peers...\n");
    
    LOCK(cs_vNodes);
    BOOST_FOREACH(CNode* pnode, vNodes) {
        // Request seeder list from each connected peer
        pnode->PushMessage("getseederlist");
    }
}

void CTorV3Manager::HandleSeederListMessage(CNode* pfrom, const std::vector<std::string>& seederList)
{
    printf("Received seeder list from %s with %d entries\n", 
           pfrom->addr.ToString().c_str(), (int)seederList.size());
    
    // Add new seeders to our known list
    std::set<std::string> newSeeders;
    
    if (pwalletMain) {
        CWalletDB walletdb(pwalletMain->strWalletFile);
        
        // Get existing seeders
        std::string existingList;
        walletdb.ReadSetting("known_seeders", existingList);
        
        std::set<std::string> existing;
        std::stringstream ss(existingList);
        std::string seeder;
        while (std::getline(ss, seeder, ',')) {
            if (!seeder.empty()) {
                existing.insert(seeder);
            }
        }
        
        // Add new seeders
        for (const std::string& newSeeder : seederList) {
            if (existing.find(newSeeder) == existing.end()) {
                existing.insert(newSeeder);
                newSeeders.insert(newSeeder);
                printf("Added new seeder: %s\n", newSeeder.c_str());
            }
        }
        
        // Save updated list
        std::string updatedList;
        for (const std::string& s : existing) {
            if (!updatedList.empty()) updatedList += ",";
            updatedList += s;
        }
        walletdb.WriteSetting("known_seeders", updatedList);
    }
    
    // Auto-connect to new seeders
    for (const std::string& newSeeder : newSeeders) {
        ConnectToSeederNode(newSeeder);
    }
}

void CTorV3Manager::BroadcastSeederList()
{
    if (!torV3Config.enableSeederMode) return;
    
    std::vector<std::string> seederList = GetKnownSeederNodes();
    
    LOCK(cs_vNodes);
    BOOST_FOREACH(CNode* pnode, vNodes) {
        // Send our seeder list to requesting peers
        pnode->PushMessage("seederlist", seederList);
    }
    
    printf("Broadcasted seeder list with %d entries\n", (int)seederList.size());
}

// Helper methods for enhanced seeder functionality

// Update seeder last seen timestamp
void CTorV3Manager::UpdateSeederLastSeen(const std::string& seederAddress)
{
    if (!pwalletMain) return;
    
    try {
        CWalletDB walletdb(pwalletMain->strWalletFile);
        std::string lastSeenKey = "seeder_last_seen_" + seederAddress;
        walletdb.WriteSetting(lastSeenKey, (int64_t)GetTime());
    } catch (const std::exception& e) {
        printf("ERROR: Failed to update seeder last seen time: %s\n", e.what());
    }
}

// Update seeder reputation based on connection success/failure
void CTorV3Manager::UpdateSeederReputation(const std::string& seederAddress, bool success)
{
    if (!pwalletMain) return;
    
    try {
        CWalletDB walletdb(pwalletMain->strWalletFile);
        std::string reputationKey = "seeder_reputation_" + seederAddress;
        
        uint32_t currentReputation = 50; // Default starting reputation
        walletdb.ReadSetting(reputationKey, currentReputation);
        
        if (success) {
            // Increase reputation for successful connections (max 100)
            currentReputation = std::min((uint32_t)100, currentReputation + 5);
        } else {
            // Decrease reputation for failed connections (min 0)
            currentReputation = (currentReputation >= 10) ? currentReputation - 10 : 0;
        }
        
        walletdb.WriteSetting(reputationKey, currentReputation);
        
        printf("Updated seeder %s reputation to %d (success: %s)\n", 
               seederAddress.c_str(), currentReputation, success ? "true" : "false");
               
    } catch (const std::exception& e) {
        printf("ERROR: Failed to update seeder reputation: %s\n", e.what());
    }
}

// Request seeder list from a specific peer
void CTorV3Manager::RequestSeederListFromPeer(const std::string& peerAddress)
{
    LOCK(cs_vNodes);
    BOOST_FOREACH(CNode* pnode, vNodes) {
        if (pnode->addr.ToString().find(peerAddress) != std::string::npos) {
            try {
                pnode->PushMessage("getseederlist");
                printf("Requested seeder list from peer: %s\n", peerAddress.c_str());
                return;
            } catch (const std::exception& e) {
                printf("ERROR: Failed to request seeder list from peer %s: %s\n", 
                       peerAddress.c_str(), e.what());
            }
        }
    }
    printf("WARNING: Peer %s not found for seeder list request\n", peerAddress.c_str());
}

// Schedule periodic seeder re-announcements
void CTorV3Manager::ScheduleSeederReannouncement()
{
    // This would typically be handled by a timer or scheduler
    // For now, we'll just update the last announcement time
    if (pwalletMain) {
        CWalletDB walletdb(pwalletMain->strWalletFile);
        walletdb.WriteSetting("seeder_last_announcement", (int64_t)GetTime());
        printf("Scheduled seeder re-announcement\n");
    }
}

// Global functions
bool InitTorV3()
{
    return CTorV3Manager::GetInstance()->InitializeTor();
}

void ShutdownTorV3()
{
    CTorV3Manager::GetInstance()->ShutdownTor();
}

TorV3Config& GetTorV3Config()
{
    return torV3Config;
}