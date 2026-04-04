// Copyright (c) 2024 Triangles developers
// Network Bootstrap Configuration for Triangles
// Distributed under the MIT/X11 software license

#ifndef TRIANGLES_NET_BOOTSTRAP_H
#define TRIANGLES_NET_BOOTSTRAP_H

#include <cstdint>
#include <string>
#include <vector>

// Tor-native network bootstrap configuration.
// All connections route through embedded Tor. Clearnet is disabled.

namespace NetBootstrap {

    // Network protocol compatibility settings
    struct NetworkConfig {
        static const int MIN_PROTOCOL_VERSION_OLD_WALLET = 70200;
        static const int CURRENT_PROTOCOL_VERSION = 70205;

        // Network ports
        static const int DEFAULT_PORT_MAINNET = 24112;
        static const int DEFAULT_PORT_TESTNET = 24111;
        static const int DEFAULT_RPC_PORT_MAINNET = 19112;
        static const int DEFAULT_RPC_PORT_TESTNET = 19111;

        // Connection limits
        static const int MAX_CONNECTIONS_DEFAULT = 64;
        static const int MAX_OUTBOUND_CONNECTIONS = 8;

        // Tor configuration
        static const int TOR_HIDDEN_SERVICE_PORT = 24112;

        // Timeouts
        static const int CONNECTION_TIMEOUT_SECONDS = 30;
        static const int PEER_DISCOVERY_INTERVAL_SECONDS = 3600;
    };

    // Network health monitoring
    struct NetworkHealth {
        int connectedPeers;
        int torPeers;
        bool isBootstrapped;
        bool isSyncing;
        int64_t lastBlockTime;
    };

    NetworkHealth GetNetworkHealth();

} // namespace NetBootstrap

#endif // TRIANGLES_NET_BOOTSTRAP_H
