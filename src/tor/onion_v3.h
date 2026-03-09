// Copyright (c) 2024 Triangles developers
// Tor V3 Onion Services Integration
// Distributed under the MIT/X11 software license

#ifndef TRIANGLES_TOR_ONION_V3_H
#define TRIANGLES_TOR_ONION_V3_H

#include <string>
#include <vector>
#include <map>
#include "compat.h"

// Forward declarations
class CNode;

// Tor V3 onion service management
class CTorV3Service
{
private:
    std::string privateKey;
    std::string publicKey;
    std::string onionAddress;
    int port;
    bool isActive;

public:
    CTorV3Service();
    ~CTorV3Service();

    // Generate new V3 onion service
    bool GenerateV3Service(int servicePort = 19112);
    
    // Load existing service from keys
    bool LoadFromPrivateKey(const std::string& privKey);
    
    // Start the onion service
    bool StartOnionService();
    
    // Stop the onion service
    bool StopService();
    
    // Get the .onion address (56 characters for V3)
    std::string GetOnionAddress() const { return onionAddress; }
    
    // Get service status
    bool IsActive() const { return isActive; }
    
    // Save keys to wallet
    bool SaveToWallet();
    
    // Load keys from wallet
    bool LoadFromWallet();
    
    // Encrypted key storage methods
    bool SaveEncryptedToWallet();
    bool LoadEncryptedFromWallet();
    bool EncryptTorKeys(std::vector<unsigned char>& vchCryptedPrivateKey);
    bool DecryptTorKeys(const std::vector<unsigned char>& vchCryptedPrivateKey, std::string& decryptedPrivateKey);
    
    // Ed25519 key management functions
    bool ValidateEd25519Keys(const unsigned char* privateKey, const unsigned char* publicKey);
    bool GenerateV3OnionAddress(const unsigned char* publicKey, std::string& address);
    static bool ValidateOnionAddress(const std::string& address);
    
    // Key extraction and validation
    bool ExtractKeysFromHex(const std::string& privKeyHex, unsigned char* privKey, unsigned char* pubKey);
    bool DerivePublicKeyFromPrivate(const unsigned char* privateKey, unsigned char* publicKey);
    
    // Advanced address validation and utilities
    bool ExtractPublicKeyFromAddress(const std::string& address, unsigned char* publicKey);
    bool VerifyAddressChecksum(const std::string& address);
    std::string GenerateAddressFromPublicKey(const unsigned char* publicKey);
};

// Tor V3 network manager
class CTorV3Manager
{
private:
    static CTorV3Manager* instance;
    std::map<int, CTorV3Service*> services;
    bool torEnabled;
    std::string torDataDir;

public:
    static CTorV3Manager* GetInstance();
    
    CTorV3Manager();
    ~CTorV3Manager();
    
    // Initialize Tor with V3 support
    bool InitializeTor();
    
    // Shutdown Tor
    void ShutdownTor();
    
    // Create new V3 hidden service for wallet
    bool CreateWalletHiddenService(int port = 19112);
    
    // Enable/disable Tor
    void SetTorEnabled(bool enabled);
    bool IsTorEnabled() const { return torEnabled; }
    
    // Get wallet's onion address
    std::string GetWalletOnionAddress();
    
    // Connect to peer via onion address
    bool ConnectToOnionPeer(const std::string& onionAddr, int port);
    
    // Get list of onion peers
    std::vector<std::string> GetOnionPeers();
    
    // Auto-discover onion peers and seeders
    bool DiscoverOnionPeers();
    
    // Seeder management functions
    std::vector<std::string> GetKnownSeederNodes();
    bool ConnectToSeederNode(const std::string& seederAddress);
    void RequestSeederListFromPeers();
    void HandleSeederListMessage(CNode* pfrom, const std::vector<std::string>& seederList);
    void BroadcastSeederList();
    bool RegisterAsSeederNode(const std::string& onionAddress, int port);
    
    // SOCKS5 proxy support methods
    bool ConnectThroughSocks5Proxy(const std::string& onionAddr, int port);
    bool PerformSocks5Handshake(SOCKET hSocket, const std::string& onionAddr, int port);
    
    // Enhanced peer connection methods
    bool ConnectToSeederNodeWithRetry(const std::string& seederAddress, int maxRetries);
    void DiscoverAdditionalPeers();
    void UpdatePeerReputation(const std::string& onionAddr, bool success);
    void UpdateDiscoveryStats(int connected, int attempted);
    
    // Enhanced seeder management helper methods
    void UpdateSeederLastSeen(const std::string& seederAddress);
    void UpdateSeederReputation(const std::string& seederAddress, bool success);
    void RequestSeederListFromPeer(const std::string& peerAddress);
    void ScheduleSeederReannouncement();
};

// Tor V3 configuration
struct TorV3Config
{
    bool enableTor;
    bool enableHiddenService;
    bool enableSeederMode;
    int hiddenServicePort;
    std::string torDataDirectory;
    std::string socksProxy;
    int maxConnections;
    std::vector<std::string> trustedOnionPeers;
    
    TorV3Config() : 
        enableTor(false),
        enableHiddenService(false), 
        enableSeederMode(false),
        hiddenServicePort(19112),
        torDataDirectory("tor_data"),
        socksProxy("127.0.0.1:9050"),
        maxConnections(8) {}
};

// Utility functions
std::string EncodeBase32(const unsigned char* data, size_t len);
std::string EncodeBase32Proper(const unsigned char* data, size_t len);
bool DecodeBase32(const std::string& encoded, std::vector<unsigned char>& decoded);

// Global functions
bool InitTorV3();
void ShutdownTorV3();
TorV3Config& GetTorV3Config();
bool SaveTorV3Config();
bool LoadTorV3Config();

#endif // TRIANGLES_TOR_ONION_V3_H