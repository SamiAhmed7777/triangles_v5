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

    lastError.clear();
    socksPort = socks;
    samPort = sam;
    serverPort = server;
    i2pHostname.clear();

    // Prepare i2pd data directory under the wallet's data dir
    i2pDataDir = (::GetDataDir() / "i2p_data").string();
    fs::create_directories(i2pDataDir);
    fs::permissions(i2pDataDir, fs::perms::owner_all, fs::perm_options::replace);

    printf("Embedded I2P: starting i2pd router...\n");

    // Write an i2pd.conf configuration file that enables SAM + SOCKS proxy.
    // i2pd's config system reads from a file; programmatic option setting is
    // fragile across i2pd versions. Writing a minimal conf is robust.
    {
        fs::path confPath = fs::path(i2pDataDir) / "i2pd.conf";
        std::ofstream conf(confPath.string());
        if (!conf.is_open()) {
            lastError = "Failed to write i2pd.conf";
            return false;
        }
        conf << "# Auto-generated by Triangles embedded I2P\n";
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
        if (tunnelConf.is_open()) {
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
            printf("Embedded I2P: server tunnel configured on port %d\n", serverPort);
        }
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

    try {
        // Initialize i2pd: config parse, filesystem, crypto, router context
        i2p::api::InitI2P((int)(argvPtrs.size() - 1), argvPtrs.data(), "triangles-i2pd");

        // Start the I2P router: netdb, transports, tunnels, router context
        // Redirect i2pd logs to our stdout/stderr
        auto logStream = std::make_shared<std::ostream>(std::cout.rdbuf());
        i2p::api::StartI2P(logStream);

        printf("Embedded I2P: router started, starting client services...\n");

        // Start the client context — this initializes SAM bridge, SOCKS proxy,
        // and tunnels based on config. The client context reads the conf we
        // wrote above to determine which services to start.
        i2p::client::context.Start();

        running.store(true);
        printf("Embedded I2P: SOCKS proxy at 127.0.0.1:%d, SAM at 127.0.0.1:%d\n",
               socksPort, samPort);

        // Wait for i2pd's SOCKS proxy AND SAM bridge to become available
        // (up to 120s — I2P bootstrap is slower than Tor due to floodfill
        // lookup and tunnel build).
        printf("Embedded I2P: waiting for SOCKS proxy and SAM bridge...\n");
        bool socksReady = false;
        bool samReady = false;

        for (int i = 0; i < 120; i++) {
            MilliSleep(1000);
            if (fShutdown) {
                Stop();
                return false;
            }

            // --- Check SOCKS proxy readiness ---
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

            // --- Check SAM bridge readiness ---
            if (!samReady) {
                samReady = IsSamAvailable();
                if (samReady) {
                    printf("Embedded I2P: SAM v3 bridge ready on port %d (took %ds)\n",
                           samPort, i + 1);
                }
            }

            // Both endpoints are up — router is fully bootstrapped
            if (socksReady && samReady) {
                printf("Embedded I2P: all I2P endpoints ready (SOCKS %d + SAM %d)\n",
                       socksPort, samPort);
                return true;
            }

            if (i > 0 && i % 30 == 0) {
                printf("Embedded I2P: still bootstrapping (%ds elapsed, SOCKS:%s SAM:%s)...\n",
                       i, socksReady ? "ready" : "wait",
                       samReady ? "ready" : "wait");
            }
        }

        // Not everything ready after 120s — I2P may still be building tunnels.
        // We return true anyway; connections will retry once tunnels are up.
        printf("Embedded I2P: bootstrap incomplete after 120s"
               " (SOCKS:%s SAM:%s) — will retry on demand.\n",
               socksReady ? "ready" : "pending",
               samReady ? "ready" : "pending");
        return true;

    } catch (const std::exception& e) {
        lastError = std::string("i2pd initialization failed: ") + e.what();
        printf("ERROR: Embedded I2P startup failed: %s\n", e.what());
        return false;
    }
}

void CI2PEmbedded::Stop()
{
    if (!running.load()) return;
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

    running.store(false);
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
