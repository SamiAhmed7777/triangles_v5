// Copyright (c) 2024 Triangles developers
// I2P Router Process Manager - launches and manages a bundled i2pd binary
// Distributed under the MIT/X11 software license
//
// Mirrors tor_process.cpp: locate an i2pd executable shipped alongside the
// wallet (or installed on the system), write an auto-generated config that
// enables the SAM bridge, launch it as a managed child process, and shut it
// down when the wallet exits. The SAM session in i2p.cpp then connects to it,
// so the user does not have to install or run a separate I2P router.

#ifndef TRIANGLES_I2P_PROCESS_H
#define TRIANGLES_I2P_PROCESS_H

#include <string>

#ifdef WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

class CI2PProcess
{
public:
    static CI2PProcess* GetInstance();

    CI2PProcess();
    ~CI2PProcess();

    // Bring up the router. If something is already listening on the SAM port we
    // assume an external router and do not launch our own (fExternal=true).
    // Returns true if a SAM bridge is (or will shortly be) reachable.
    bool Start(const std::string& dataDir, int samPort = 7656);

    // Terminate the launched router (no-op for an external one).
    void Stop();

    bool IsRunning();
    bool IsExternal() const { return fExternal; }
    std::string GetLastError() const { return lastError; }
    std::string GetBinaryPath() const { return binaryPath; }

private:
    std::string FindI2pdBinary();
    bool WriteConfig();
    static bool CanConnect(const std::string& host, int port);

    int samPort;
    bool running;
    bool fExternal;
    std::string dataDir;
    std::string binaryPath;
    std::string confPath;
    std::string lastError;

#ifdef WIN32
    HANDLE hProcess;
    HANDLE hJob;
    DWORD processId;
#else
    int processId;
#endif
};

// Convenience wrappers for init.cpp.
bool StartEmbeddedI2P(const std::string& dataDir, int samPort);
void StopEmbeddedI2P();

#endif // TRIANGLES_I2P_PROCESS_H
