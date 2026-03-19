// Copyright (c) 2025-2026 Triangles developers
// Embedded Tor integration - runs Tor in-process as a library
// Distributed under the MIT/X11 software license
//
// BUILD REQUIREMENT: Link against libtor.a built from the official Tor source.
// See CODEX-TOR-GUIDE.md for submodule setup and build instructions.
//
// This file compiles in two modes:
//   1. ENABLE_TOR_EMBEDDED defined: full embedded Tor via tor_api.h
//   2. ENABLE_TOR_EMBEDDED not defined: stubs that fall back to external tor_process

#include "tor_embedded.h"
#include "../util.h"
#include "../net.h"

#include <boost/filesystem.hpp>
#include <boost/thread/thread.hpp>
#include <fstream>
#include <cstring>
#include <vector>
#include <string>

#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#endif

#ifdef ENABLE_TOR_EMBEDDED
// Official Tor C API (tor >= 0.4.5)
extern "C" {
#include <tor_api.h>
}
#endif

namespace fs = boost::filesystem;

// Singleton
CTorEmbedded* CTorEmbedded::instance = nullptr;

CTorEmbedded* CTorEmbedded::GetInstance()
{
    if (!instance)
        instance = new CTorEmbedded();
    return instance;
}

CTorEmbedded::CTorEmbedded()
    : running(false)
    , socksPort(19099)
    , hiddenServicePort(24112)
{
}

CTorEmbedded::~CTorEmbedded()
{
    Stop();
}

std::string CTorEmbedded::GetSocksProxy() const
{
    return "127.0.0.1:" + std::to_string(socksPort);
}

#ifdef ENABLE_TOR_EMBEDDED

// ========================================================================
//  Embedded mode: Tor runs in-process via libtor.a / tor_api.h
// ========================================================================

// The Tor thread entry point.  tor_run_main() blocks until Tor shuts down.
static void TorThreadFunc(std::vector<std::string> argv_strings)
{
    // Build a C argv array that tor_run_main expects.
    std::vector<char*> argv_ptrs;
    for (auto& s : argv_strings)
        argv_ptrs.push_back(&s[0]);
    argv_ptrs.push_back(nullptr);

    tor_main_configuration_t* cfg = tor_main_configuration_new();
    if (!cfg) {
        printf("ERROR: tor_main_configuration_new() failed\n");
        CTorEmbedded::GetInstance()->SetRunning(false);
        return;
    }

    // argc does not count the trailing nullptr
    int rc = tor_main_configuration_set_command_line(
        cfg, (int)(argv_ptrs.size() - 1), argv_ptrs.data());
    if (rc != 0) {
        printf("ERROR: tor_main_configuration_set_command_line() returned %d\n", rc);
        tor_main_configuration_free(cfg);
        CTorEmbedded::GetInstance()->SetRunning(false);
        return;
    }

    printf("Embedded Tor starting (SOCKS %d, HS port %d)...\n",
           CTorEmbedded::GetInstance()->GetSocksPort(),
           CTorEmbedded::GetInstance()->GetHiddenServicePort());

    // This blocks until Tor exits
    rc = tor_run_main(cfg);
    tor_main_configuration_free(cfg);

    printf("Embedded Tor exited with code %d\n", rc);
    CTorEmbedded::GetInstance()->SetRunning(false);
}

bool CTorEmbedded::Start(int socks, int hsPort)
{
    if (running.load()) return true;

    socksPort = socks;
    hiddenServicePort = hsPort;

    // Prepare Tor data directory under the wallet's data dir
    torDataDir = (::GetDataDir() / "tor_data").string();
    fs::create_directories(torDataDir);

    // Hidden service directory
    std::string hsDir = (fs::path(torDataDir) / "hidden_service").string();
    fs::create_directories(hsDir);

    // Build the argv for tor_run_main
    std::vector<std::string> argv;
    argv.push_back("tor");  // program name (argv[0])
    argv.push_back("--SocksPort");
    argv.push_back(std::to_string(socksPort));
    argv.push_back("--DataDirectory");
    argv.push_back(torDataDir);
    argv.push_back("--HiddenServiceDir");
    argv.push_back(hsDir);
    argv.push_back("--HiddenServiceVersion");
    argv.push_back("3");
    argv.push_back("--HiddenServicePort");
    argv.push_back(std::to_string(hiddenServicePort) + " 127.0.0.1:" + std::to_string(hiddenServicePort));
    argv.push_back("--AvoidDiskWrites");
    argv.push_back("1");
    argv.push_back("--Log");
    argv.push_back("notice stderr");

    running.store(true);

    // Launch Tor on a dedicated thread (tor_run_main blocks)
    boost::thread torThread(TorThreadFunc, argv);
    torThread.detach();

    // Wait for SOCKS port to become available (up to 60s)
    printf("Waiting for embedded Tor to bootstrap...\n");
    for (int i = 0; i < 60; i++) {
        MilliSleep(1000);
        if (fShutdown) {
            Stop();
            return false;
        }

        // Quick port check
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock >= 0) {
            struct sockaddr_in addr;
            memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            addr.sin_port = htons(socksPort);
            bool up = (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == 0);
#ifdef WIN32
            closesocket(sock);
#else
            close(sock);
#endif
            if (up) {
                printf("Embedded Tor SOCKS proxy ready on port %d (took %ds)\n", socksPort, i + 1);

                // Read .onion hostname if available
                fs::path hostnameFile = fs::path(hsDir) / "hostname";
                if (fs::exists(hostnameFile)) {
                    std::ifstream f(hostnameFile.string().c_str());
                    if (f.is_open())
                        std::getline(f, onionHostname);
                    if (!onionHostname.empty())
                        printf("Tor hidden service: %s\n", onionHostname.c_str());
                }
                return true;
            }
        }

        if (!running.load()) {
            printf("ERROR: Embedded Tor thread exited during bootstrap\n");
            return false;
        }
    }

    printf("WARNING: Embedded Tor started but SOCKS not ready after 60s (still bootstrapping)\n");
    return true;
}

void CTorEmbedded::Stop()
{
    if (!running.load()) return;
    printf("Requesting embedded Tor shutdown...\n");
    // tor_run_main respects signals; raise SIGTERM to trigger graceful exit
#ifndef WIN32
    // On Unix we can signal our own process; the Tor thread handles it
    // Actually, tor_api doesn't provide a clean shutdown function in 0.4.x
    // For now, the thread will exit when the process exits.
    // TODO: Tor 0.4.9+ may add tor_api_shutdown(), use it when available
#endif
    running.store(false);
}

#else // !ENABLE_TOR_EMBEDDED

// ========================================================================
//  Fallback stubs: embedded Tor not compiled in, use external tor_process
// ========================================================================

#include "tor_process.h"

bool CTorEmbedded::Start(int socks, int hsPort)
{
    printf("Embedded Tor not compiled in. Using external Tor process.\n");
    // Delegate to the external process manager
    socksPort = socks;
    hiddenServicePort = hsPort;
    torDataDir = (::GetDataDir() / "tor_data").string();
    running.store(StartTorProcess(torDataDir, socksPort, hiddenServicePort));
    return running.load();
}

void CTorEmbedded::Stop()
{
    StopTorProcess();
    running.store(false);
}

#endif // ENABLE_TOR_EMBEDDED

// ========================================================================
//  Global hooks (called from init.cpp)
// ========================================================================

bool StartEmbeddedTor()
{
    if (GetBoolArg("-notor", false)) {
        printf("Tor disabled by -notor flag\n");
        return false;
    }

    int socksPort = GetArg("-torsocks", 19099);
    int hsPort    = GetArg("-torhsport", GetListenPort());

    return CTorEmbedded::GetInstance()->Start(socksPort, hsPort);
}

void StopEmbeddedTor()
{
    CTorEmbedded::GetInstance()->Stop();
}
