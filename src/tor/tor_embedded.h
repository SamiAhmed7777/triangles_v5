// Copyright (c) 2025-2026 Triangles developers
// Embedded Tor integration - runs Tor in-process as a library
// Distributed under the MIT/X11 software license

#ifndef TRIANGLES_TOR_EMBEDDED_H
#define TRIANGLES_TOR_EMBEDDED_H

#include <string>
#include <atomic>

// Embedded Tor state
class CTorEmbedded
{
private:
    static CTorEmbedded* instance;
    std::atomic<bool> running;
    int socksPort;
    int hiddenServicePort;
    bool hiddenServiceEnabled;
    std::string torDataDir;
    std::string onionHostname;
    std::string lastError;

public:
    static CTorEmbedded* GetInstance();

    CTorEmbedded();
    ~CTorEmbedded();

    // Start Tor in a background thread (blocks that thread until shutdown)
    bool Start(int socksPort = 19099, int hsPort = 24112, bool enableHiddenService = true);

    // Request Tor to shut down
    void Stop();

    // Check if Tor is running and bootstrapped
    bool IsRunning() const { return running.load(); }
    void SetRunning(bool value) { running.store(value); }

    // Get the SOCKS5 proxy address for outbound connections
    std::string GetSocksProxy() const;
    int GetSocksPort() const { return socksPort; }
    const std::string& GetDataDir() const { return torDataDir; }

    // Get our .onion address (available after bootstrap)
    std::string GetOnionAddress() const { return onionHostname; }
    std::string GetLastError() const { return lastError; }
    void SetLastError(const std::string& value) { lastError = value; }

    // Get the hidden service port
    int GetHiddenServicePort() const { return hiddenServicePort; }
    bool IsHiddenServiceEnabled() const { return hiddenServiceEnabled; }
};

// Global init/shutdown hooks (called from init.cpp)
bool StartEmbeddedTor();
void StopEmbeddedTor();

#endif // TRIANGLES_TOR_EMBEDDED_H
