// Copyright (c) 2024 Triangles developers
// Network Bootstrap Implementation for Triangles v5.0.0.0
// Distributed under the MIT/X11 software license

#include "net_bootstrap.h"
#include "net.h"
#include "util.h"
#include "init.h"
#include "tor/onion_v3.h"

namespace NetBootstrap {

    // Forward declarations
    bool InitializeLegacyBootstrap();
    bool InitializeMixedBootstrap();
    bool InitializeTorOnlyBootstrap();

    // Global variable for tracking network activity
    int64_t nTimeBestReceived = 0;

    BootstrapMode GetBootstrapMode() {
        // Determine bootstrap mode based on configuration
        bool torEnabled = GetBoolArg("-tor", false) || GetBoolArg("-proxy", false);
        bool onlyTor = GetBoolArg("-onlynet", false) && GetArg("-onlynet", "") == "tor";
        
        if (onlyTor) {
            printf("Network Bootstrap: Tor-only mode selected\n");
            return BOOTSTRAP_TOR_ONLY;
        } else if (torEnabled) {
            printf("Network Bootstrap: Mixed Tor + clearnet mode selected\n");
            return BOOTSTRAP_TOR_MIXED;
        } else {
            printf("Network Bootstrap: Legacy clearnet mode selected\n");
            return BOOTSTRAP_LEGACY;
        }
    }

    bool InitializeNetworkBootstrap(BootstrapMode mode) {
        printf("Initializing network bootstrap (mode: %d)...\n", mode);
        
        try {
            switch (mode) {
                case BOOTSTRAP_LEGACY:
                    return InitializeLegacyBootstrap();
                    
                case BOOTSTRAP_TOR_MIXED:
                    return InitializeMixedBootstrap();
                    
                case BOOTSTRAP_TOR_ONLY:
                    return InitializeTorOnlyBootstrap();
                    
                default:
                    printf("ERROR: Unknown bootstrap mode: %d\n", mode);
                    return false;
            }
        } catch (const std::exception& e) {
            printf("ERROR: Exception in network bootstrap: %s\n", e.what());
            return false;
        }
    }

    bool InitializeLegacyBootstrap() {
        printf("Initializing legacy network bootstrap for old wallet compatibility...\n");
        
        // Use DNS seeds and hardcoded IP addresses
        // This ensures old wallets can still connect
        
        // Add DNS seed nodes
        for (int i = 0; strDNSSeed[i] != NULL; i++) {
            printf("Adding DNS seed: %s\n", strDNSSeed[i]);
            // DNS resolution will be handled by the existing network code
        }
        
        // Add hardcoded IP seed nodes
        for (int i = 0; i < ARRAYLEN(pnSeed); i++) {
            if (pnSeed[i] != 0) {
                printf("Adding IP seed: 0x%08x\n", pnSeed[i]);
                // IP seeds will be processed by existing network code
            }
        }
        
        printf("Legacy bootstrap initialized successfully\n");
        return true;
    }

    bool InitializeMixedBootstrap() {
        printf("Initializing mixed Tor + clearnet bootstrap...\n");
        
        // Initialize legacy bootstrap first
        if (!InitializeLegacyBootstrap()) {
            printf("ERROR: Failed to initialize legacy bootstrap\n");
            return false;
        }
        
        // Initialize Tor v3 functionality
        if (!InitTorV3()) {
            printf("WARNING: Failed to initialize Tor v3, continuing with clearnet only\n");
            return true; // Don't fail completely, just continue without Tor
        }
        
        // Get Tor manager instance
        CTorV3Manager* torManager = CTorV3Manager::GetInstance();
        if (!torManager) {
            printf("WARNING: Could not get Tor manager instance\n");
            return true;
        }
        
        // Connect to Tor seed nodes
        std::vector<std::string> torSeeds = torManager->GetKnownSeederNodes();
        printf("Found %d Tor seed nodes\n", (int)torSeeds.size());
        
        for (const std::string& seed : torSeeds) {
            printf("Attempting to connect to Tor seed: %s\n", seed.c_str());
            torManager->ConnectToSeederNode(seed);
        }
        
        printf("Mixed bootstrap initialized successfully\n");
        return true;
    }

    bool InitializeTorOnlyBootstrap() {
        printf("Initializing Tor-only bootstrap...\n");
        
        // Initialize Tor v3 functionality
        if (!InitTorV3()) {
            printf("ERROR: Failed to initialize Tor v3 for Tor-only mode\n");
            return false;
        }
        
        // Get Tor manager instance
        CTorV3Manager* torManager = CTorV3Manager::GetInstance();
        if (!torManager) {
            printf("ERROR: Could not get Tor manager instance for Tor-only mode\n");
            return false;
        }
        
        // Enable Tor
        torManager->SetTorEnabled(true);
        
        // Create hidden service if configured
        if (GetBoolArg("-torhiddenservice", false)) {
            int hiddenServicePort = GetArg("-torhiddenserviceport", NetworkConfig::TOR_HIDDEN_SERVICE_PORT);
            if (!torManager->CreateWalletHiddenService(hiddenServicePort)) {
                printf("WARNING: Failed to create hidden service\n");
            } else {
                printf("Created hidden service on port %d\n", hiddenServicePort);
                
                // Register as seeder if enabled
                if (GetBoolArg("-torseeder", false)) {
                    std::string onionAddress = torManager->GetWalletOnionAddress();
                    if (!onionAddress.empty()) {
                        torManager->RegisterAsSeederNode(onionAddress, hiddenServicePort);
                        printf("Registered as Tor seeder: %s:%d\n", onionAddress.c_str(), hiddenServicePort);
                    }
                }
            }
        }
        
        // Connect to Tor seed nodes
        std::vector<std::string> torSeeds = torManager->GetKnownSeederNodes();
        printf("Found %d Tor seed nodes for Tor-only mode\n", (int)torSeeds.size());
        
        if (torSeeds.empty()) {
            printf("WARNING: No Tor seed nodes available for Tor-only mode\n");
            return false;
        }
        
        int connected = 0;
        for (const std::string& seed : torSeeds) {
            printf("Attempting to connect to Tor seed: %s\n", seed.c_str());
            if (torManager->ConnectToSeederNode(seed)) {
                connected++;
            }
        }
        
        if (connected == 0) {
            printf("ERROR: Failed to connect to any Tor seed nodes\n");
            return false;
        }
        
        printf("Tor-only bootstrap initialized successfully (%d connections)\n", connected);
        return true;
    }

    bool IsOldWalletCompatible(int protocolVersion) {
        return protocolVersion >= NetworkConfig::MIN_PROTOCOL_VERSION_OLD_WALLET;
    }

    std::vector<std::string> GetCompatibleSeedNodes(int protocolVersion) {
        std::vector<std::string> seeds;
        
        if (protocolVersion >= NetworkConfig::CURRENT_PROTOCOL_VERSION) {
            // Modern wallet - can use all seed types
            
            // Add Tor v3 seeds
            CTorV3Manager* torManager = CTorV3Manager::GetInstance();
            if (torManager) {
                std::vector<std::string> torSeeds = torManager->GetKnownSeederNodes();
                seeds.insert(seeds.end(), torSeeds.begin(), torSeeds.end());
            }
            
            // Add DNS seeds
            for (int i = 0; strDNSSeed[i] != NULL; i++) {
                seeds.push_back(std::string(strDNSSeed[i]));
            }
            
        } else if (IsOldWalletCompatible(protocolVersion)) {
            // Old but compatible wallet - use DNS seeds only
            for (int i = 0; strDNSSeed[i] != NULL; i++) {
                seeds.push_back(std::string(strDNSSeed[i]));
            }
        }
        
        return seeds;
    }

    NetworkHealth GetNetworkHealth() {
        NetworkHealth health;
        health.connectedPeers = 0;
        health.torPeers = 0;
        health.clearnetPeers = 0;
        health.isBootstrapped = false;
        health.isSyncing = false;
        health.lastBlockTime = 0;
        
        // Count connected peers
        {
            LOCK(cs_vNodes);
            health.connectedPeers = vNodes.size();
            
            // Count Tor vs clearnet peers
            for (CNode* pnode : vNodes) {
                std::string addr = pnode->addr.ToString();
                if (addr.find(".onion") != std::string::npos) {
                    health.torPeers++;
                } else {
                    health.clearnetPeers++;
                }
            }
        }
        
        // Check if we're bootstrapped (have at least 3 connections)
        health.isBootstrapped = (health.connectedPeers >= 3);
        
        // Check sync status
        extern int nBestHeight;
        extern int64_t nTimeBestReceived;
        
        health.isSyncing = (nTimeBestReceived > 0 && 
                           GetTime() - nTimeBestReceived < 3600); // Synced within last hour
        health.lastBlockTime = nTimeBestReceived;
        
        return health;
    }

} // namespace NetBootstrap