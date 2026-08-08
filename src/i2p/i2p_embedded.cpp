// Copyright (c) 2025-2026 Triangles developers
// Embedded I2P (i2pd) integration - runs an I2P router in-process
// Distributed under the MIT/X11 software license
//
// BUILD REQUIREMENT: Link against libi2pd.a + libi2pd_client.a built from
// the PurpleI2P/i2pd source tree (src/i2p/i2pd-src).
//
// This file compiles in two modes:
//   1. ENABLE_I2P_EMBEDDED defined: full embedded i2pd via i2p::api
//   2. ENABLE_I2P_EMBEDDED not defined: stubs that report I2P unavailable

#include "i2p_embedded.h"
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

namespace fs = std::filesystem;

// ===========================================================================
//  CI2PSamSocket — SAM v3 direct streaming implementation
// ===========================================================================
//
//  Protocol reference: https://geti2p.net/en/docs/api/samv3
//
//  The SAM bridge is a simple line-oriented text protocol over TCP.  After
//  HELLO + SESSION CREATE + STREAM CONNECT succeed, the socket becomes a
//  raw bidirectional byte stream to the I2P destination — no further SAM
//  framing is needed and there is zero SOCKS overhead.

static std::atomic<unsigned int> g_samSessionSeq{0};

CI2PSamSocket::CI2PSamSocket()
    : rawSocket(I2P_INVALID_SOCKET)
{
}

CI2PSamSocket::~CI2PSamSocket()
{
    CloseSocket();
}

void CI2PSamSocket::CloseSocket()
{
    if (rawSocket != I2P_INVALID_SOCKET) {
#ifdef WIN32
        closesocket(rawSocket);
#else
        close(rawSocket);
#endif
        rawSocket = I2P_INVALID_SOCKET;
    }
}

I2pSocket_t CI2PSamSocket::GetRawSocket()
{
    I2pSocket_t fd = rawSocket;
    rawSocket = I2P_INVALID_SOCKET;   // transfer ownership
    return fd;
}

bool CI2PSamSocket::SamConnect(const std::string& host, int port)
{
    CloseSocket();

#ifdef WIN32
    rawSocket = (I2pSocket_t)::socket(AF_INET, SOCK_STREAM, 0);
    if (rawSocket == INVALID_SOCKET) {
#else
    rawSocket = ::socket(AF_INET, SOCK_STREAM, 0);
    if (rawSocket < 0) {
#endif
        lastError = "SAM: failed to create socket";
        return false;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);   // SAM is always local
    addr.sin_port = htons((uint16_t)port);

    if (::connect(rawSocket, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        lastError = "SAM: cannot connect to bridge at 127.0.0.1:" + std::to_string(port);
        CloseSocket();
        return false;
    }

    return true;
}

bool CI2PSamSocket::SendLine(const std::string& line)
{
    std::string msg = line + "\n";
    const char* data = msg.data();
    size_t remaining = msg.size();

    while (remaining > 0) {
#ifdef WIN32
        int n = ::send(rawSocket, data, (int)remaining, 0);
#else
        ssize_t n = ::send(rawSocket, data, remaining, MSG_NOSIGNAL);
#endif
        if (n <= 0) {
            lastError = "SAM: send failed";
            return false;
        }
        data += n;
        remaining -= (size_t)n;
    }
    return true;
}

bool CI2PSamSocket::ReadLine(std::string& lineOut)
{
    // Look for a complete line (terminated by \n) in recvBuffer first.
    for (;;) {
        size_t nl = recvBuffer.find('\n');
        if (nl != std::string::npos) {
            lineOut = recvBuffer.substr(0, nl);
            // Strip trailing \r (SAM bridge always uses \n, but be tolerant)
            if (!lineOut.empty() && lineOut.back() == '\r')
                lineOut.pop_back();
            recvBuffer.erase(0, nl + 1);
            return true;
        }

        char buf[4096];
#ifdef WIN32
        int n = ::recv(rawSocket, buf, sizeof(buf), 0);
#else
        ssize_t n = ::recv(rawSocket, buf, sizeof(buf), 0);
#endif
        if (n <= 0) {
            lastError = "SAM: connection closed while waiting for reply";
            return false;
        }
        recvBuffer.append(buf, (size_t)n);
    }
}

std::string CI2PSamSocket::ParseValue(const std::string& line, const std::string& key)
{
    // Find KEY=VALUE token within a space-separated SAM response line.
    std::string needle = key + "=";
    size_t pos = line.find(needle);
    if (pos == std::string::npos)
        return {};

    pos += needle.size();
    size_t end = line.find(' ', pos);
    if (end == std::string::npos)
        return line.substr(pos);
    return line.substr(pos, end - pos);
}

bool CI2PSamSocket::Connect(const std::string& dest_b32, int port,
                            const std::string& samHost, int samPort)
{
    CloseSocket();
    lastError.clear();
    recvBuffer.clear();

    if (dest_b32.empty()) {
        lastError = "SAM: empty destination";
        return false;
    }

    // Generate a unique session ID for this connection.
    unsigned int seq = ++g_samSessionSeq;
    sessionId = "triangles-" + std::to_string(seq) + "-" +
                std::to_string((unsigned long)std::time(nullptr));

    // ----------------------------------------------------------------
    //  Step 0: TCP connect to the SAM bridge
    // ----------------------------------------------------------------
    if (!SamConnect(samHost, samPort)) {
        // lastError already set by SamConnect
        return false;
    }

    // ----------------------------------------------------------------
    //  Step 1: HELLO handshake
    //    C → S: HELLO VERSION MIN=3.1 MAX=3.1
    //    S → C: HELLO REPLY RESULT=OK VERSION=3.1
    // ----------------------------------------------------------------
    if (!SendLine("HELLO VERSION MIN=3.1 MAX=3.1")) {
        return false;
    }

    {
        std::string reply;
        if (!ReadLine(reply)) {
            return false;
        }
        std::string result = ParseValue(reply, "RESULT");
        if (result != "OK") {
            lastError = "SAM HELLO failed: " + reply;
            CloseSocket();
            return false;
        }
    }

    // ----------------------------------------------------------------
    //  Step 2: SESSION CREATE (transient destination)
    //    C → S: SESSION CREATE STYLE=STREAM ID=<id> DESTINATION=TRANSIENT
    //    S → C: SESSION STATUS RESULT=OK DESTINATION=<base64>
    // ----------------------------------------------------------------
    if (!SendLine("SESSION CREATE STYLE=STREAM ID=" + sessionId +
                  " DESTINATION=TRANSIENT")) {
        return false;
    }

    {
        std::string reply;
        if (!ReadLine(reply)) {
            return false;
        }
        std::string result = ParseValue(reply, "RESULT");
        if (result != "OK") {
            lastError = "SAM SESSION CREATE failed: " + reply;
            CloseSocket();
            return false;
        }
        // Save the transient local destination (base64) for diagnostics.
        localDestination = ParseValue(reply, "DESTINATION");
    }

    // ----------------------------------------------------------------
    //  Step 3: STREAM CONNECT to the remote destination
    //    C → S: STREAM CONNECT ID=<id> DESTINATION=<b32>.i2p
    //    S → C: STREAM STATUS RESULT=OK
    //
    //  After RESULT=OK the socket is a raw byte stream — no more SAM
    //  framing is needed.
    // ----------------------------------------------------------------
    // Ensure destination has the .b32.i2p suffix (accept bare b32 hash too)
    std::string dest = dest_b32;
    if (dest.find(".i2p") == std::string::npos && dest.find(".b32") == std::string::npos) {
        // Looks like a bare b32 hash — append the standard suffix
        dest += ".b32.i2p";
    }

    if (!SendLine("STREAM CONNECT ID=" + sessionId + " DESTINATION=" + dest)) {
        return false;
    }

    {
        std::string reply;
        if (!ReadLine(reply)) {
            return false;
        }
        std::string result = ParseValue(reply, "RESULT");
        if (result != "OK") {
            lastError = "SAM STREAM CONNECT to " + dest + " failed: " + reply;
            CloseSocket();
            return false;
        }
    }

    // Socket is now a raw I2P stream.  Any residual bytes in recvBuffer
    // belong to the application layer — leave them for the caller.
    return true;
}

// ===========================================================================
//  CI2PEmbedded — singleton router management
// ===========================================================================

// Singleton
CI2PEmbedded* CI2PEmbedded::instance = nullptr;

CI2PEmbedded* CI2PEmbedded::GetInstance()
{
    if (!instance)
        instance = new CI2PEmbedded();
    return instance;
}

CI2PEmbedded::CI2PEmbedded()
    : running(false)
    , socksPort(19100)
    , samPort(7656)
    , serverPort(0)
{
}

CI2PEmbedded::~CI2PEmbedded()
{
    Stop();
}

std::string CI2PEmbedded::GetSocksProxy() const
{
    return "127.0.0.1:" + std::to_string(socksPort);
}

// ---------------------------------------------------------------------------
//  IsSamAvailable — quick TCP probe of the SAM bridge port
// ---------------------------------------------------------------------------
bool CI2PEmbedded::IsSamAvailable() const
{
#ifdef WIN32
    SOCKET sock = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET)
        return false;
#else
    int sock = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
        return false;
#endif

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons((uint16_t)samPort);

    bool ok = (::connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == 0);

#ifdef WIN32
    closesocket(sock);
#else
    close(sock);
#endif
    return ok;
}

// ---------------------------------------------------------------------------
//  CreateConnection — factory for SAM v3 direct streaming connections
// ---------------------------------------------------------------------------
CI2PSamSocket* CI2PEmbedded::CreateConnection(const std::string& dest_b32, int port)
{
    if (!running.load()) {
        return nullptr;
    }

    auto* sam = new CI2PSamSocket();
    if (!sam->Connect(dest_b32, port, "127.0.0.1", samPort)) {
        // Caller can inspect via the object — but they don't have it yet,
        // so log the error and clean up.
        printf("I2P SAM connect failed: %s\n", sam->GetLastError().c_str());
        delete sam;
        return nullptr;
    }

    printf("I2P SAM stream connected to %s (raw socket, no SOCKS overhead)\n",
           dest_b32.c_str());
    return sam;
}

#ifdef ENABLE_I2P_EMBEDDED

// ========================================================================
//  Embedded mode: i2pd runs in-process via libi2pd / i2p::api
// ========================================================================

#ifdef WIN32
// MinGW's rpcndr.h (pulled in by winsock2.h/windows.h) #defines
// 'interface' as 'struct' for COM support. i2pd's I2CP.h uses it as a
// parameter name (I2CPServer(const std::string& interface, ...)),
// causing a parse error. Undef before including any i2pd headers.
#undef interface
#endif

// i2pd C++ API
#include "Config.h"
#include "Log.h"
#include "FS.h"
#include "Crypto.h"
#include "NetDb.hpp"
#include "Transports.h"
#include "Tunnel.h"
#include "RouterContext.h"
#include "Streaming.h"
#include "Destination.h"
#include "ClientContext.h"
#include "I2PTunnel.h"
#include "api.h"

static std::unique_ptr<i2p::client::I2PServerTunnel> g_i2pServerTunnel;
static std::shared_ptr<i2p::client::ClientDestination> g_i2pServerDestination;

bool CI2PEmbedded::Start(int socks, int sam, int server)
{
    if (running.load()) return true;

    // ----------------------------------------------------------------
    // PHASE 0: validate input BEFORE any state mutation.
    // If validation fails, we must leave the system in a clean state
    // (running=false, no i2p data dir side effects, no InitI2P call).
    // ----------------------------------------------------------------
    if (socks < 1 || socks > 65535) {
        lastError = strprintf("SOCKS proxy port %d out of range (1-65535)", socks);
        return false;
    }
    if (sam < 1 || sam > 65535) {
        lastError = strprintf("SAM bridge port %d out of range (1-65535)", sam);
        return false;
    }
    if (server < 0 || server > 65535) {
        lastError = strprintf("server tunnel port %d out of range (0-65535, 0=disable)", server);
        return false;
    }

    lastError.clear();
    socksPort = socks;
    samPort = sam;
    serverPort = server;
    {
        std::lock_guard<std::mutex> lock(hostnameMutex);
        i2pHostname.clear();
    }

    // Prepare i2pd data directory under the wallet's data dir
    i2pDataDir = (::GetDataDir() / "i2p_data").string();
    try {
        fs::create_directories(i2pDataDir);
        fs::permissions(i2pDataDir, fs::perms::owner_all, fs::perm_options::replace);
    } catch (const fs::filesystem_error& e) {
        lastError = strprintf("Cannot create i2p data dir %s: %s", i2pDataDir.c_str(), e.what());
        return false;
    }

    printf("Embedded I2P: starting i2pd router...\n");

    // Write an i2pd.conf configuration file that enables SAM + SOCKS proxy.
    // NOTE: As of i2pd 2.60.0, the embedded library API (i2p::api::InitI2P)
    // never calls ParseConfig, so this file is NOT read at runtime. It is
    // written for documentation/debugging purposes only — operators can
    // inspect it to see what ports the daemon intends to use. The actual
    // port bindings are applied programmatically via i2p::config::SetOption
    // below (before the background thread starts). Keep the file in sync
    // with the SetOption calls.
    {
        fs::path confPath = fs::path(i2pDataDir) / "i2pd.conf";
        std::ofstream conf(confPath.string());
        if (!conf.is_open()) {
            lastError = "Failed to write i2pd.conf";
            return false;
        }
        conf << "# Auto-generated by Triangles embedded I2P\n";
        conf << "# NOTE: i2pd 2.60.0 library API does NOT read this file.\n";
        conf << "# Actual port bindings come from i2p::config::SetOption in i2p_embedded.cpp.\n";
        conf << "datadir = " << i2pDataDir << "\n";
        conf << "loglevel = info\n";
        conf << "\n";
        // SOCKS proxy for outbound .i2p connections (P2P transport)
        conf << "[socksproxy]\n";
        conf << "enabled = true\n";
        conf << "address = 127.0.0.1\n";
        conf << "port = " << socksPort << "\n";
        conf << "keys = socks-proxy.dat\n";
        conf << "\n";
        // SAM bridge for SAM v3 direct streaming API
        conf << "[sam]\n";
        conf << "enabled = true\n";
        conf << "address = 127.0.0.1\n";
        conf << "port = " << samPort << "\n";
        conf << "\n";
        // Disable HTTP proxy (port 4444). Cycle-13 fix: the HTTPProxy runs by
        // default in i2pd 2.60.0 and any HTTP request to its port causes a
        // nullptr deref in i2p::i18n::Locale::GetString. Conf is dead code in
        // the embedded library path; SetOption in InitI2P is the real override.
        conf << "[httpproxy]\n";
        conf << "enabled = false\n";
        conf << "\n";
        // Disable HTTP webconsole (not needed for embedded use)
        conf << "[http]\n";
        conf << "enabled = false\n";
        conf << "\n";
        // Disable I2P control protocol
        conf << "[i2pcontrol]\n";
        conf << "enabled = false\n";
        conf << "\n";
        // Disable BOB
        conf << "[bob]\n";
        conf << "enabled = false\n";
        conf << "\n";
        conf.close();
    }

    // Write tunnels.conf BEFORE Start() — ClientContext::Start() reads this
    // file to create server/client tunnels. The server tunnel is the I2P
    // equivalent of a Tor hidden service: it forwards inbound I2P connections
    // to the Triangles P2P listen port.
    if (serverPort > 0) {
        fs::path tunnelConfPath = fs::path(i2pDataDir) / "tunnels.conf";
        std::ofstream tunnelConf(tunnelConfPath.string());
        if (!tunnelConf.is_open()) {
            lastError = strprintf("Failed to write %s for server tunnel configuration",
                                  tunnelConfPath.string().c_str());
            printf("ERROR: %s\n", lastError.c_str());
            return false;
        }
        tunnelConf << "# Auto-generated by Triangles embedded I2P\n";
        tunnelConf << "[triangles-p2p]\n";
        tunnelConf << "type = server\n";
        tunnelConf << "host = 127.0.0.1\n";
        tunnelConf << "port = " << serverPort << "\n";
        tunnelConf << "keys = triangles-p2p-keys.dat\n";
        tunnelConf << "inbound.length = 3\n";
        tunnelConf << "outbound.length = 3\n";
        tunnelConf << "inbound.quantity = 5\n";
        tunnelConf << "outbound.quantity = 5\n";
        tunnelConf.close();
        if (tunnelConf.fail()) {
            lastError = strprintf("Write failed for %s", tunnelConfPath.string().c_str());
            printf("ERROR: %s\n", lastError.c_str());
            return false;
        }
        printf("Embedded I2P: server tunnel configured on port %d\n", serverPort);
    }

    // Build argv for i2pd initialization. Pass --datadir and --conf on the
    // command line (not just in the conf file) because i2pd's ParseCmdline
    // runs BEFORE ParseConfig, and DetectDataDir needs the datadir early.
    std::vector<std::string> argvStrings;
    argvStrings.push_back("i2pd");
    argvStrings.push_back("--datadir");
    argvStrings.push_back(i2pDataDir);
    argvStrings.push_back("--conf");
    argvStrings.push_back((fs::path(i2pDataDir) / "i2pd.conf").string());

    std::vector<char*> argvPtrs;
    for (auto& s : argvStrings)
        argvPtrs.push_back(&s[0]);
    argvPtrs.push_back(nullptr);

    // The whole post-InitI2P section is wrapped in try/catch so that ANY
    // failure after i2pd is initialized triggers TerminateI2P. Without this
    // an exception from SetOption or std::thread construction would leave
    // running=true but with no router thread to clean up — a leaked i2pd.
    try {
        // ----------------------------------------------------------------
        // Phase 1 (synchronous, < 1s): config parse, crypto, router context.
        // InitI2P is wrapped in try/catch so a partial-init failure does
        // not leave i2pd in a half-initialized state with running=true.
        // ----------------------------------------------------------------
        try {
            i2p::api::InitI2P((int)(argvPtrs.size() - 1), argvPtrs.data(), "triangles-i2pd");
        } catch (const std::exception& e) {
            lastError = strprintf("InitI2P failed: %s", e.what());
            // Best-effort cleanup: i2pd's InitI2P may have partially
            // initialized global state. TerminateI2P is a no-op if no
            // init happened; it cleans up otherwise.
            try { i2p::api::TerminateI2P(); } catch (...) {}
            return false;
        } catch (...) {
            lastError = "InitI2P failed: unknown exception";
            try { i2p::api::TerminateI2P(); } catch (...) {}
            return false;
        }
        fflush(stdout);

        // Mark running immediately so Qt UI shows I2P as active.
        // From this point on, any exception thrown by the code below is
        // caught by the outer try/catch, which calls TerminateI2P to
        // release the partially-initialized i2pd state.
        running.store(true);

        // ----------------------------------------------------------------
        // Phase 2 (background thread): StartI2P + client context + bootstrap
        //
        // i2p::api::StartI2P() → NetDb::Start() → Reseed() can block for
        // up to 180s on first run (empty netDb → HTTPS download from public
        // I2P reseed servers). Running this on the main init thread freezes
        // the GUI splash screen ("Starting embedded I2P router...").
        //
        // The background thread handles:
        //   1. StartI2P (router, netdb, transports, tunnels, reseed)
        //   2. client::context.Start (SAM bridge, SOCKS proxy, server tunnel)
        //   3. Polling for SOCKS/SAM port readiness (up to 300s)
        //   4. .b32.i2p address population
        //
        // Meanwhile, the main init proceeds immediately. Tor-only mode
        // works in the meantime; I2P connectivity comes up asynchronously.
        // ----------------------------------------------------------------
        printf("Embedded I2P: launching router in background thread...\n");
        fflush(stdout);

        // ----------------------------------------------------------------
        // PROGRAMMATIC OVERRIDE OF SOCKS/SAM PORTS
        //
        // i2pd 2.60.0's library API (i2p::api::InitI2P) only calls ParseCmdline
        // — it never calls ParseConfig. The i2pd.conf file we just wrote is
        // NEVER READ by the embedded library path. SOCKS proxy falls back to
        // its built-in default port (4447) regardless of what we put in the
        // conf file. This is verified by the library's bundled ParseConfig
        // (called only by the standalone daemon binary at
        // src/i2p/i2pd-src/daemon/Daemon.cpp:107) — the library API
        // deliberately omits it.
        //
        // The fix: override socksproxy.port + sam.port + socksproxy.address
        // AFTER i2p::api::InitI2P returns (so all defaults are in m_Options)
        // but BEFORE the background thread calls i2p::client::context.Start
        // which calls ReadSocksProxy + ReadSAMBridge. SetOption calls
        // notify() internally, so the new values are visible to GetOption.
        //
        // NOTE: SOCKS/SAM port range validation happens in Start() Phase 0
        // before any state mutation, so by this point socksPort and samPort
        // are already known to be 1..65535. No re-validation needed here.
        // ----------------------------------------------------------------
        printf("Embedded I2P: overriding socksproxy.port=%d sam.port=%d via SetOption\n",
               socksPort, samPort);
        fflush(stdout);
        {
            bool socksEnabled = true;
            std::string socksAddr = "127.0.0.1";
            uint16_t socksPortVal = (uint16_t)socksPort;
            std::string socksKeys = "socks-proxy.dat";
            bool samEnabled = true;
            std::string samAddr = "127.0.0.1";
            uint16_t samPortVal = (uint16_t)samPort;
            // Cycle-13 fix: HTTPProxy runs by default in i2pd 2.60.0 on
            // port 4444 and any inbound HTTP request crashes the daemon via
            // nullptr deref in i2p::i18n::Locale::GetString (m_Language is
            // never initialized). The previous SetOption("http.enabled",...)
            // targeted the i2pd WEBCONSOLE, not the HTTPProxy. Correct key
            // is "httpproxy.enabled".
            bool httpproxyEnabled = false;
            bool httpWebconsoleEnabled = false;
            bool i2pcontrolEnabled = false;
            bool bobEnabled = false;

            i2p::config::SetOption("socksproxy.enabled", socksEnabled);
            i2p::config::SetOption("socksproxy.address", socksAddr);
            i2p::config::SetOption("socksproxy.port", socksPortVal);
            i2p::config::SetOption("socksproxy.keys", socksKeys);
            i2p::config::SetOption("sam.enabled", samEnabled);
            i2p::config::SetOption("sam.address", samAddr);
            i2p::config::SetOption("sam.port", samPortVal);
            // Cycle-13 fix: was "http.enabled" which targeted webconsole.
            i2p::config::SetOption("httpproxy.enabled", httpproxyEnabled);
            i2p::config::SetOption("http.enabled", httpWebconsoleEnabled);
            i2p::config::SetOption("i2pcontrol.enabled", i2pcontrolEnabled);
            i2p::config::SetOption("bob.enabled", bobEnabled);
        }

        // Keep the thread handle so Stop() can join it. A detached thread
        // that is still running would block the wallet from exiting.
        routerThread = std::thread([this]() {
            try {
                // Start the I2P router (netdb, transports, tunnels, reseed)
                auto logStream = std::make_shared<std::ostream>(std::cout.rdbuf());
                i2p::api::StartI2P(logStream);
                fflush(stdout);

                printf("Embedded I2P: router started, starting client services...\n");
                fflush(stdout);

                // Start SAM bridge, SOCKS proxy, and server tunnel
                i2p::client::context.Start();

                printf("Embedded I2P: SOCKS proxy at 127.0.0.1:%d, SAM at 127.0.0.1:%d\n",
                       socksPort, samPort);
                fflush(stdout);

                // Wait for SOCKS proxy + SAM bridge to become available
                printf("Embedded I2P: waiting for SOCKS proxy and SAM bridge...\n");
                bool socksReady = false;
                bool samReady = false;

                for (int i = 0; i < 300; i++) {
                    MilliSleep(1000);
                    if (fShutdown) {
                        Stop();
                        return;
                    }

                    if (!socksReady) {
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
                                socksReady = true;
                                printf("Embedded I2P: SOCKS proxy ready on port %d (took %ds)\n",
                                       socksPort, i + 1);
                            }
                        }
                    }

                    if (!samReady) {
                        samReady = IsSamAvailable();
                        if (samReady) {
                            printf("Embedded I2P: SAM v3 bridge ready on port %d (took %ds)\n",
                                   samPort, i + 1);
                        }
                    }

                    if (socksReady && samReady) {
                        printf("Embedded I2P: all I2P endpoints ready (SOCKS %d + SAM %d)\n",
                               socksPort, samPort);
                        break;
                    }

                    if (i > 0 && i % 30 == 0) {
                        printf("Embedded I2P: still bootstrapping (%ds elapsed, SOCKS:%s SAM:%s)...\n",
                               i, socksReady ? "ready" : "wait",
                               samReady ? "ready" : "wait");
                    }
                }

                // ----------------------------------------------------------------
                // Populate .b32.i2p address — use the SERVER TUNNEL destination,
                // NOT the embedded router identity. Delegates to
                // DiscoverServerTunnelDestination() which is also callable
                // from GetI2PAddress() so the Qt timerI2P retry path picks
                // up the result once the tunnel registers.
                // ----------------------------------------------------------------
                DiscoverServerTunnelDestination();
                fflush(stdout);

            } catch (const std::exception& e) {
                // Background init failure: i2pd router context may be partially
                // alive (transports listening, netDb half-built). Tear it down,
                // reset running, and surface the error in lastError so callers
                // can see the failure rather than seeing running=true forever.
                printf("ERROR: Embedded I2P background init failed: %s\n", e.what());
                fflush(stdout);
                lastError = std::string("i2pd background init failed: ") + e.what();
                try { i2p::api::TerminateI2P(); } catch (...) {}
                running.store(false);
            } catch (...) {
                // Catch-all: any non-std::exception (e.g. structured exception
                // on Windows) would otherwise invoke std::terminate, killing
                // the daemon with no useful diagnostic.
                printf("ERROR: Embedded I2P background init failed: unknown exception\n");
                fflush(stdout);
                lastError = "i2pd background init failed: unknown exception";
                try { i2p::api::TerminateI2P(); } catch (...) {}
                running.store(false);
            }
        });

        printf("Embedded I2P: router init delegated to background thread\n");
        fflush(stdout);

        return true;

    } catch (const std::exception& e) {
        lastError = std::string("i2pd initialization failed: ") + e.what();
        printf("ERROR: Embedded I2P startup failed: %s\n", e.what());
        // i2pd may be partially or fully initialized by the time we got here.
        // TerminateI2P is a no-op if InitI2P never ran; otherwise it cleans
        // up router context, transports, and netDb.
        try { i2p::api::TerminateI2P(); } catch (...) {}
        running.store(false);
        return false;
    } catch (...) {
        lastError = "i2pd initialization failed: unknown exception";
        printf("ERROR: Embedded I2P startup failed: unknown exception\n");
        try { i2p::api::TerminateI2P(); } catch (...) {}
        running.store(false);
        return false;
    }
}

void CI2PEmbedded::Stop()
{
    if (!running.load()) {
        if (routerThread.joinable()) routerThread.join();
        return;
    }
    printf("Requesting embedded I2P shutdown...\n");

    try {
        // Stop client context (SAM, SOCKS, tunnels)
        i2p::client::context.Stop();

        // Stop the router
        i2p::api::StopI2P();

        // Terminate crypto
        i2p::api::TerminateI2P();
    } catch (const std::exception& e) {
        printf("WARNING: error during I2P shutdown: %s\n", e.what());
    }

    // Wait for the bootstrap thread to finish (up to 5s).
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (routerThread.joinable() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        if (!running.load()) {
            // The thread observes fShutdown and exits its loop on its own
            // once running is set false by the API teardown above.
            routerThread.join();
            break;
        }
    }
    if (routerThread.joinable()) {
        printf("WARNING: embedded I2P did not exit within 5s; detaching thread\n");
        // Detach as a last resort — the process is about to exit and the OS
        // will reap the thread.
        routerThread.detach();
    }

    running.store(false);
}

// ----------------------------------------------------------------
// CI2PEmbedded::DiscoverServerTunnelDestination
//
// Reads triangles-p2p-keys.dat (binary PrivateKeys blob) and looks for
// a matching entry in i2p::client::context.GetServerTunnels(). On
// match, sets i2pHostname to the corresponding ".b32.i2p" address.
// On no match (or serverPort==0, or read failure), leaves i2pHostname
// empty. Idempotent and safe to call repeatedly from the Qt timerI2P
// path (qt/trianglesgui.cpp:384-387, default 5s interval).
//
// Why fail-closed: publishing the keys-file hash while the tunnel is
// not yet registered would mean advertising a destination with no
// published LeaseSet → peers hit SOCKS code 4 / "LeaseSet not found"
// on the floodfill network. Empty hostname → no wrong-identity
// connectivity. The Qt timer keeps retrying until the tunnel comes up.
// ----------------------------------------------------------------
void CI2PEmbedded::DiscoverServerTunnelDestination()
{
    std::lock_guard<std::mutex> lock(hostnameMutex);

    if (serverPort == 0) {
        // No P2P server tunnel configured. This is the -nolisten / no
        // -i2phsport case (pure outbound SOCKS I2P, no inbound service).
        if (!i2pHostname.empty()) {
            printf("Embedded I2P: serverPort=0, clearing previously "
                   "discovered destination\n");
            i2pHostname.clear();
        }
        return;
    }

    bool advertised = false;
    std::string serverKeysPath = (fs::path(i2pDataDir) / "triangles-p2p-keys.dat").string();

    // Step 1: read expected ident hash from the keys file.
    std::string keysFileIdentB32;
    try {
        std::ifstream ks(serverKeysPath, std::ifstream::binary);
        if (ks.is_open()) {
            ks.seekg(0, std::ios::end);
            size_t len = ks.tellg();
            ks.seekg(0, std::ios::beg);
            if (len == 0 || len > 65536) {
                throw std::runtime_error("implausible keys file size: " +
                                         std::to_string(len));
            }
            std::vector<uint8_t> buf(len);
            ks.read(reinterpret_cast<char*>(buf.data()), len);
            if (!ks) {
                throw std::runtime_error("short read on keys file");
            }
            i2p::data::PrivateKeys pk;
            if (!pk.FromBuffer(buf.data(), len)) {
                throw std::runtime_error("PrivateKeys::FromBuffer failed");
            }
            auto pub = pk.GetPublic();
            if (!pub) {
                throw std::runtime_error("PrivateKeys::GetPublic returned null");
            }
            keysFileIdentB32 = pub->GetIdentHash().ToBase32();
        } else {
            // Quiet on retry — file-not-found is expected before the
            // bootstrap thread writes it for the first time.
            if (i2pHostname.empty()) {
                // First failure: log at info level so the operator can
                // see why the hostname is still empty.
                printf("Embedded I2P: cannot open %s for server tunnel keys "
                       "(will retry on next timerI2P tick)\n",
                       serverKeysPath.c_str());
            }
        }
    } catch (const std::exception& e) {
        lastDiscoveryError = e.what();
        printf("Embedded I2P: keys-file ident hash load failed: %s\n", e.what());
    } catch (...) {
        lastDiscoveryError = "unknown exception";
        printf("Embedded I2P: keys-file ident hash load failed: unknown exception\n");
    }

    // Step 2: only publish if a LIVE registered server tunnel matches
    // the keys-file hash. Registry membership confirms the tunnel is
    // active; LeaseSet publication is i2pd's responsibility after that.
    //
    // Thread-safety: GetServerTunnels() returns a const reference to
    // i2pd's internal m_ServerTunnels map, which has NO internal lock.
    // VisitTunnels(true) (called from ReloadConfig / Stop) can erase
    // entries concurrently. We make a VALUE COPY of the map (not a
    // reference) so that iterator invalidation during the copy is a
    // narrow read-only window, and all string comparisons run on the
    // local snapshot with no live-map access. The copy constructor of
    // std::map is exception-safe; if it throws (bad_alloc), the catch
    // below handles it.
    if (!keysFileIdentB32.empty()) {
        try {
            auto tunnels = i2p::client::context.GetServerTunnels(); // value copy
            for (const auto& kv : tunnels) {
                const auto& dest = kv.first.first;
                if (dest.ToBase32() == keysFileIdentB32) {
                    if (i2pHostname != keysFileIdentB32 + ".b32.i2p") {
                        i2pHostname = keysFileIdentB32 + ".b32.i2p";
                        lastDiscoveryError.clear(); // success — clear stale
                        printf("Embedded I2P: server tunnel address "
                               "(live registry) = %s\n", i2pHostname.c_str());
                    }
                    advertised = true;
                    break;
                }
            }
        } catch (const std::exception& e) {
            lastDiscoveryError = e.what();
            printf("Embedded I2P: server tunnel registry read failed: %s\n", e.what());
        } catch (...) {
            lastDiscoveryError = "unknown exception";
            printf("Embedded I2P: server tunnel registry read failed: "
                   "unknown exception\n");
        }
    }

    if (!advertised) {
        // Not yet in live registry. Leave empty (or clear stale value).
        if (!i2pHostname.empty()) {
            printf("Embedded I2P: server tunnel left live registry, "
                   "clearing destination %s\n", i2pHostname.c_str());
            i2pHostname.clear();
        }
    }
}

// CI2PEmbedded::GetI2PAddress — read the cached destination, retrying
// discovery if empty. Called from qt/trianglesgui.cpp:1875
// (updateI2PAddress) on every timerI2P tick.
//
// Threading: read-by-copy under hostnameMutex so concurrent writes by
// the bootstrap thread cannot tear the std::string.
std::string CI2PEmbedded::GetI2PAddress()
{
    bool needDiscovery = false;
    {
        std::lock_guard<std::mutex> lock(hostnameMutex);
        needDiscovery = i2pHostname.empty() && running.load();
    }

    if (needDiscovery) {
        // Tunnel may have registered since the bootstrap-thread scan.
        // Re-scans the registry and the keys file (does file I/O); not
        // "cheap" on retry, but bounded — single registry walk + one
        // small file read.
        DiscoverServerTunnelDestination();
    }

    std::lock_guard<std::mutex> lock(hostnameMutex);
    return i2pHostname;
}

#else // !ENABLE_I2P_EMBEDDED

// ========================================================================
//  Fallback stubs: embedded I2P not compiled in
// ========================================================================

bool CI2PEmbedded::Start(int socks, int sam, int server)
{
    printf("Embedded I2P not compiled in (ENABLE_I2P_EMBEDDED not defined).\n");
    socksPort = socks;
    samPort = sam;
    serverPort = server;
    i2pDataDir = (::GetDataDir() / "i2p_data").string();
    lastError = "I2P support not compiled in. Build with -DUSE_I2P_EMBEDDED=ON";
    return false;
}

void CI2PEmbedded::Stop()
{
    running.store(false);
}

// Stubs for the new methods (header declares them unconditionally)
void CI2PEmbedded::DiscoverServerTunnelDestination() {}
std::string CI2PEmbedded::GetI2PAddress()
{
    std::lock_guard<std::mutex> lock(hostnameMutex);
    return i2pHostname;
}

#endif // ENABLE_I2P_EMBEDDED

// ========================================================================
//  Global hooks (called from init.cpp)
// ========================================================================

bool StartEmbeddedI2P()
{
    bool enableI2P = GetBoolArg("-i2p", true);
    if (!enableI2P) {
        printf("I2P disabled by -i2p=0 flag\n");
        return false;
    }

    int socksPort = GetArg("-i2psocks", 19100);
    int samPort = GetArg("-i2psam", 7656);
    int serverPort = GetArg("-i2phsport", GetListenPort());

    return CI2PEmbedded::GetInstance()->Start(socksPort, samPort, serverPort);
}

void StopEmbeddedI2P()
{
    CI2PEmbedded::GetInstance()->Stop();
}
