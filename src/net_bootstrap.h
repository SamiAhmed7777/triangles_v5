// Copyright (c) 2024 Triangles developers
// Network Bootstrap Configuration for Triangles v5.0.0.0
// Distributed under the MIT/X11 software license

#ifndef TRIANGLES_NET_BOOTSTRAP_H
#define TRIANGLES_NET_BOOTSTRAP_H

#include <cstdint>
#include <string>
#include <vector>

// Network bootstrap configuration for preserving old wallets
// while enabling modern Tor v3 functionality

namespace NetBootstrap {

    // DNS seed nodes for initial bootstrap (clearnet fallback)
    // Unused attribute prevents warnings
    static const char* strDNSSeed[] __attribute__((unused)) = {
        "seed1.cryptographic-triangles.org",
        "seed2.cryptographic-triangles.org",
        "seed3.cryptographic-triangles.org",
        "backup-seed.cryptographic-triangles.org",
        NULL
    };

    // Legacy IP seed nodes for old wallet compatibility
    // These should be actual IP addresses of stable nodes
    static const unsigned int pnSeed[] __attribute__((unused)) = {
        // Format: IP addresses in network byte order (little-endian)
        // For IP a.b.c.d: (d << 24) | (c << 16) | (b << 8) | a
        0xCE58E9C2, // DNS2-OpenClaw: 194.233.88.206
    };

    // Network protocol compatibility settings
    struct NetworkConfig {
        // Preserve compatibility with old wallets
        static const int MIN_PROTOCOL_VERSION_OLD_WALLET = 70200;
        static const int CURRENT_PROTOCOL_VERSION = 70205;
        
        // Network ports (preserved from existing configuration)
        static const int DEFAULT_PORT_MAINNET = 24112;
        static const int DEFAULT_PORT_TESTNET = 24111;
        static const int DEFAULT_RPC_PORT_MAINNET = 19112;
        static const int DEFAULT_RPC_PORT_TESTNET = 19111;
        
        // Connection limits for different node types
        static const int MAX_CONNECTIONS_DEFAULT = 125;
        static const int MAX_CONNECTIONS_TOR_ONLY = 64;
        static const int MAX_OUTBOUND_CONNECTIONS = 8;
        
        // Tor-specific configuration
        static const int TOR_HIDDEN_SERVICE_PORT = 24112;
        static const char* TOR_PROXY_DEFAULT = "127.0.0.1:9050";
        
        // Network timeouts and retry settings
        static const int CONNECTION_TIMEOUT_SECONDS = 30;
        static const int PEER_DISCOVERY_INTERVAL_SECONDS = 3600; // 1 hour
        static const int SEEDER_ANNOUNCEMENT_INTERVAL_SECONDS = 1800; // 30 minutes
    };

    // Bootstrap sequence for different wallet types
    enum BootstrapMode {
        BOOTSTRAP_LEGACY,      // Old wallets without Tor
        BOOTSTRAP_TOR_MIXED,   // Mixed clearnet + Tor
        BOOTSTRAP_TOR_ONLY     // Tor-only mode
    };

    // Get bootstrap configuration based on wallet capabilities
    BootstrapMode GetBootstrapMode();
    
    // Initialize network bootstrap based on configuration
    bool InitializeNetworkBootstrap(BootstrapMode mode);
    
    // Backward compatibility helpers
    bool IsOldWalletCompatible(int protocolVersion);
    std::vector<std::string> GetCompatibleSeedNodes(int protocolVersion);
    
    // Network health monitoring
    struct NetworkHealth {
        int connectedPeers;
        int torPeers;
        int clearnetPeers;
        bool isBootstrapped;
        bool isSyncing;
        int64_t lastBlockTime;
    };
    
    NetworkHealth GetNetworkHealth();
    
} // namespace NetBootstrap

#endif // TRIANGLES_NET_BOOTSTRAP_H