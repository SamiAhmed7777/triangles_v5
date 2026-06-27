// Copyright (c) 2025-2026 Triangles developers
// Embedded I2P (i2pd) integration - runs an I2P router in-process
// Distributed under the MIT/X11 software license

#ifndef TRIANGLES_I2P_EMBEDDED_H
#define TRIANGLES_I2P_EMBEDDED_H

#include <string>
#include <atomic>

// Embedded I2P router state
class CI2PEmbedded
{
private:
    static CI2PEmbedded* instance;
    std::atomic<bool> running;
    int socksPort;          // i2pd SOCKS proxy port (for outbound .i2p connections)
    int samPort;            // i2pd SAM bridge port (for SAM v3 protocol)
    int serverPort;         // Triangles P2P listen port (for incoming I2P connections)
    std::string i2pDataDir; // i2pd data directory (under wallet datadir)
    std::string i2pHostname; // Our .b32.i2p address (available after router startup)
    std::string lastError;

public:
    static CI2PEmbedded* GetInstance();

    CI2PEmbedded();
    ~CI2PEmbedded();

    // Start embedded i2pd router (blocks calling thread briefly during init)
    bool Start(int socksPort = 19100, int samPort = 7656, int serverPort = 0);

    // Request i2pd to shut down
    void Stop();

    // Check if i2pd is running
    bool IsRunning() const { return running.load(); }
    void SetRunning(bool value) { running.store(value); }

    // Get the SOCKS5 proxy address for outbound .i2p connections
    std::string GetSocksProxy() const;
    int GetSocksPort() const { return socksPort; }
    int GetSamPort() const { return samPort; }
    int GetServerPort() const { return serverPort; }
    const std::string& GetDataDir() const { return i2pDataDir; }

    // Get our .b32.i2p destination address
    std::string GetI2PAddress() const { return i2pHostname; }
    std::string GetStartupError() const { return lastError; }
    void SetStartupError(const std::string& value) { lastError = value; }
};

// Global init/shutdown hooks (called from init.cpp)
bool StartEmbeddedI2P();
void StopEmbeddedI2P();

#endif // TRIANGLES_I2P_EMBEDDED_H
