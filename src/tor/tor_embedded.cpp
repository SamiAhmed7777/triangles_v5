// Copyright (c) 2025-2026 Triangles developers
// Embedded Tor integration - runs Tor in-process as a library
// Distributed under the MIT/X11 software license
//
// BUILD REQUIREMENT: Link against libtor.a built from the official Tor source.
//
// This file compiles in two modes:
//   1. ENABLE_TOR_EMBEDDED defined: full embedded Tor via tor_api.h
//   2. ENABLE_TOR_EMBEDDED not defined: stubs that fall back to external tor_process

#include "tor_embedded.h"
#include "../util.h"
#include "../net.h"

#include <filesystem>
#include <thread>
#include <fstream>
#include <cstring>
#include <chrono>
#include <ctime>
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

namespace fs = std::filesystem;

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
    , hiddenServiceEnabled(true)
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

bool CTorEmbedded::Start(int socks, int hsPort, bool enableHiddenService)
{
    if (running.load()) return true;

    lastError.clear();
    socksPort = socks;
    hiddenServiceEnabled = enableHiddenService;
    hiddenServicePort = hiddenServiceEnabled ? hsPort : 0;
    onionHostname.clear();

    // Prepare Tor data directory under the wallet's data dir
    torDataDir = (::GetDataDir() / "tor_data").string();
    fs::create_directories(torDataDir);
    // CRITICAL: Tor refuses to use a DataDirectory readable by other users.
    // Without 0700, tor_run_main() returns -1 and the embedded Tor never starts.
    fs::permissions(torDataDir, fs::perms::owner_all, fs::perm_options::replace);

    // triangles fix: auto-repair `state`-as-file corruption (pitfall #19).
    // Tor's atomic state-write pattern is: write `state.tmp` → rename to `state`.
    // If the daemon is killed or the process crashes mid-write, the rename can
    // fail and `state` may be left as a regular file (or a partial file). On
    // next start, Tor sees "State file ... is not a file? Failing." and dies
    // with code -1 ("Reading config failed"). This was hit on DNS3 on
    // 2026-05-24 and on the TRI-LAPTOP GUI wallet on 2026-06-15. The user-facing
    // symptom is "Tor failed to start. Triangles requires Tor to operate." and
    // the only fix was manually renaming the corrupt file. Detect this state
    // here and auto-rename so the daemon is self-healing.
    {
        fs::path statePath = fs::path(torDataDir) / "state";
        std::error_code ec;
        if (fs::exists(statePath, ec) && !fs::is_directory(statePath, ec)) {
            // state is a file (or symlink to one) — quarantine it
            auto now = std::chrono::system_clock::now();
            auto t = std::chrono::system_clock::to_time_t(now);
            char ts[32];
            std::strftime(ts, sizeof(ts), "%Y%m%d-%H%M%S", std::gmtime(&t));
            fs::path quarantine = fs::path(torDataDir) /
                (std::string("state.corrupt-") + ts);
            try {
                fs::rename(statePath, quarantine, ec);
                if (ec) {
                    // rename can fail on Windows if dest exists; remove then rename
                    fs::remove(quarantine, ec);
                    fs::rename(statePath, quarantine, ec);
                }
                printf("Tor state was a file (corrupt) — quarantined to %s for inspection. Tor will recreate state/ as a directory.\n",
                       quarantine.filename().string().c_str());
            } catch (const std::exception& e) {
                printf("WARNING: could not quarantine corrupt Tor state file %s: %s\n",
                       statePath.string().c_str(), e.what());
                // Last resort: try to remove it so Tor can proceed
                fs::remove(statePath, ec);
            }
        }
    }

    std::string hsDir;
    if (hiddenServiceEnabled) {
        hsDir = (fs::path(torDataDir) / "hidden_service").string();
        fs::create_directories(hsDir);
        // CRITICAL: Tor rejects hidden service directories that are not 0700
        // ("Permissions on directory ... are too permissive") and aborts config
        // validation with code -1. This was the root cause of "Embedded Tor
        // exited with code -1" — fs::create_directories honors umask (0022 on
        // most Linux systems), leaving the dir at 0755. Force 0700 after creation.
        fs::permissions(hsDir, fs::perms::owner_all, fs::perm_options::replace);
    }

    // Build the argv for tor_run_main
    std::vector<std::string> argv;
    argv.push_back("tor");  // program name (argv[0])
    argv.push_back("--SocksPort");
    argv.push_back(std::to_string(socksPort));
    argv.push_back("--DataDirectory");
    argv.push_back(torDataDir);
    if (hiddenServiceEnabled) {
        argv.push_back("--HiddenServiceDir");
        argv.push_back(hsDir);
        argv.push_back("--HiddenServiceVersion");
        argv.push_back("3");
        argv.push_back("--HiddenServicePort");
        argv.push_back(std::to_string(hiddenServicePort) + " 127.0.0.1:" + std::to_string(hiddenServicePort));
    }
    argv.push_back("--AvoidDiskWrites");
    argv.push_back("1");
    argv.push_back("--Log");
    argv.push_back("notice stderr");

    running.store(true);

    // Launch Tor on a dedicated thread (tor_run_main blocks)
    std::thread torThread(TorThreadFunc, argv);
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
#ifdef WIN32
        SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock != INVALID_SOCKET) {
#else
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock >= 0) {
#endif
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
                if (hiddenServiceEnabled) {
                    fs::path hostnameFile = fs::path(hsDir) / "hostname";
                    if (fs::exists(hostnameFile)) {
                        std::ifstream f(hostnameFile.string().c_str());
                        if (f.is_open())
                            std::getline(f, onionHostname);
                        if (!onionHostname.empty())
                            printf("Tor hidden service: %s\n", onionHostname.c_str());
                    }
                }
                return true;
            }
        }

        if (!running.load()) {
            lastError = "Embedded Tor thread exited during bootstrap before the SOCKS proxy became available.";
            printf("ERROR: Embedded Tor thread exited during bootstrap\n");
            return false;
        }
    }

    lastError = strprintf("Embedded Tor did not expose SOCKS port %d within 60 seconds.", socksPort);
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

bool CTorEmbedded::Start(int socks, int hsPort, bool enableHiddenService)
{
    printf("Embedded Tor not compiled in. Using external Tor process.\n");
    // Delegate to the external process manager
    socksPort = socks;
    hiddenServiceEnabled = enableHiddenService;
    hiddenServicePort = hiddenServiceEnabled ? hsPort : 0;
    onionHostname.clear();
    lastError.clear();
    torDataDir = (::GetDataDir() / "tor_data").string();
    running.store(StartTorProcess(torDataDir, socksPort, hiddenServicePort, hiddenServiceEnabled));
    if (!running.load()) {
        lastError = CTorProcess::GetInstance()->GetStartupError();
    }
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

    bool enableHiddenService = GetBoolArg("-torhiddenservice", true);
    int socksPort = GetArg("-torsocks", 19099);
    int hsPort    = enableHiddenService ? GetArg("-torhsport", GetListenPort()) : 0;

    return CTorEmbedded::GetInstance()->Start(socksPort, hsPort, enableHiddenService);
}

void StopEmbeddedTor()
{
    CTorEmbedded::GetInstance()->Stop();
}
