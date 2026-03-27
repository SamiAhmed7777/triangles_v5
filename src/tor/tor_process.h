// Copyright (c) 2024-2025 Triangles developers
// Tor Process Manager - launches and manages an external Tor binary
// Distributed under the MIT/X11 software license

#ifndef TRIANGLES_TOR_PROCESS_H
#define TRIANGLES_TOR_PROCESS_H

#include <string>

#ifdef WIN32
#include <windows.h>
#endif

// Manages starting/stopping an external Tor process
// Provides SOCKS5 proxy for .onion connectivity and v3 hidden service
class CTorProcess
{
private:
    std::string torBinaryPath;
    std::string torDataDir;
    std::string torrcPath;
    int socksPort;
    int hiddenServicePort;
    bool hiddenServiceEnabled;
    bool running;

#ifdef WIN32
    HANDLE hProcess;
    DWORD processId;
#else
    pid_t processId;
#endif

    // Find the Tor binary in standard locations
    std::string FindTorBinary();

    // Generate torrc configuration file
    bool WriteTorrc();

    // Check if SOCKS port is already in use (another Tor running)
    bool IsPortInUse(int port);

public:
    CTorProcess();
    ~CTorProcess();

    // Start the Tor process
    // Returns true if Tor was started or is already running
    bool Start(const std::string& dataDir, int socksPort = 19099, int hsPort = 24112, bool enableHiddenService = true);

    // Stop the Tor process
    void Stop();

    // Check if Tor is running
    bool IsRunning();

    // Get the SOCKS proxy address
    std::string GetSocksProxy() const;

    // Get the Tor binary path (for diagnostics)
    std::string GetBinaryPath() const { return torBinaryPath; }

    // Singleton access
    static CTorProcess* GetInstance();
};

// Global convenience functions
bool StartTorProcess(const std::string& dataDir, int socksPort = 19099, int hsPort = 24112, bool enableHiddenService = true);
void StopTorProcess();

#endif // TRIANGLES_TOR_PROCESS_H
