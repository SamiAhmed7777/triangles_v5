// Copyright (c) 2024 Triangles developers
// Network Bootstrap Implementation for Triangles
// Distributed under the MIT/X11 software license

#include "net_bootstrap.h"
#include "net.h"
#include "util.h"

namespace NetBootstrap {

    NetworkHealth GetNetworkHealth() {
        NetworkHealth health;
        health.connectedPeers = 0;
        health.torPeers = 0;
        health.isBootstrapped = false;
        health.isSyncing = false;
        health.lastBlockTime = 0;

        // Count connected peers
        {
            LOCK(cs_vNodes);
            health.connectedPeers = vNodes.size();

            for (CNode* pnode : vNodes) {
                std::string addr = pnode->addr.ToString();
                if (addr.find(".onion") != std::string::npos) {
                    health.torPeers++;
                }
            }
        }

        // Bootstrapped if at least 3 connections
        health.isBootstrapped = (health.connectedPeers >= 3);

        // Sync status
        extern int64_t nTimeBestReceived;
        health.isSyncing = (nTimeBestReceived > 0 &&
                           GetTime() - nTimeBestReceived < 3600);
        health.lastBlockTime = nTimeBestReceived;

        return health;
    }

} // namespace NetBootstrap
