// Copyright (c) 2024 Triangles developers
// I2P (SAM v3) transport support
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "i2p.h"

#include "util.h"
#include "netbase.h"
#include "protocol.h"   // CAddress
#include "net.h"        // AddI2PInboundNode(), GetListenPort()

#include <openssl/sha.h>

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#ifndef closesocket
#define closesocket close
#endif
#endif

// I2P uses a base64 variant where '+' -> '-' and '/' -> '~'.
static const char* pI2PBase64 =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-~";

static std::vector<unsigned char> DecodeI2PBase64(const std::string& str)
{
    int table[256];
    for (int i = 0; i < 256; i++) table[i] = -1;
    for (int i = 0; i < 64; i++) table[(unsigned char)pI2PBase64[i]] = i;

    std::vector<unsigned char> out;
    int bits = 0; uint32_t buf = 0;
    for (char c : str) {
        if (c == '=' || c == '\r' || c == '\n') continue;
        int v = table[(unsigned char)c];
        if (v < 0) continue; // skip anything unexpected
        buf = (buf << 6) | v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back((unsigned char)((buf >> bits) & 0xFF));
        }
    }
    return out;
}

CI2PSession* CI2PSession::GetInstance()
{
    static CI2PSession instance;
    return &instance;
}

CI2PSession::CI2PSession()
    : samHost(I2P_DEFAULT_SAM_HOST), samPort(I2P_DEFAULT_SAM_PORT),
      hSession(INVALID_SOCKET), fEnabled(false), fActive(false), fShutdown(false)
{
}

CI2PSession::~CI2PSession()
{
    Stop();
}

std::string CI2PSession::GetB32Address()
{
    std::lock_guard<std::mutex> lock(cs);
    return b32Address;
}

// --- low level SAM helpers -------------------------------------------------

bool CI2PSession::SamConnect(SOCKET& hSocketRet)
{
    SOCKET hSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (hSocket == INVALID_SOCKET)
        return false;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)samPort);
    addr.sin_addr.s_addr = inet_addr(samHost.c_str());

    if (connect(hSocket, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        closesocket(hSocket);
        return false;
    }

    hSocketRet = hSocket;
    return true;
}

bool CI2PSession::SamSendLine(SOCKET hSocket, const std::string& strLine)
{
    std::string out = strLine + "\n";
    const char* p = out.c_str();
    size_t left = out.size();
    while (left > 0) {
        int n = send(hSocket, p, (int)left, MSG_NOSIGNAL);
        if (n <= 0)
            return false;
        p += n;
        left -= n;
    }
    return true;
}

bool CI2PSession::SamRecvLine(SOCKET hSocket, std::string& strLineRet)
{
    strLineRet.clear();
    char c;
    // SAM replies are newline terminated; read one byte at a time so we stop
    // exactly at the boundary and leave any following stream data untouched.
    for (int i = 0; i < 16384; i++) {
        int n = recv(hSocket, &c, 1, 0);
        if (n <= 0)
            return false;
        if (c == '\n')
            return true;
        if (c != '\r')
            strLineRet += c;
    }
    return false;
}

std::string CI2PSession::SamGetValue(const std::string& strReply, const std::string& strKey)
{
    // Tokens are space separated KEY=VALUE pairs.  VALUE runs to the next space.
    std::string needle = strKey + "=";
    size_t pos = strReply.find(needle);
    if (pos == std::string::npos)
        return "";
    pos += needle.size();
    size_t end = strReply.find(' ', pos);
    if (end == std::string::npos)
        end = strReply.size();
    return strReply.substr(pos, end - pos);
}

bool CI2PSession::SamHandshake(SOCKET hSocket)
{
    if (!SamSendLine(hSocket, "HELLO VERSION MIN=3.1 MAX=3.3"))
        return false;
    std::string reply;
    if (!SamRecvLine(hSocket, reply))
        return false;
    if (SamGetValue(reply, "RESULT") != "OK") {
        printf("I2P: SAM handshake failed: %s\n", reply.c_str());
        return false;
    }
    return true;
}

std::string CI2PSession::DestToB32(const std::string& strB64Dest)
{
    std::vector<unsigned char> dest = DecodeI2PBase64(strB64Dest);
    if (dest.empty())
        return "";
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(dest.data(), dest.size(), hash);
    std::string b32 = EncodeBase32(hash, SHA256_DIGEST_LENGTH);
    // I2P b32 addresses are unpadded.
    while (!b32.empty() && b32[b32.size() - 1] == '=')
        b32.erase(b32.size() - 1);
    return b32 + ".b32.i2p";
}

// --- session bring-up ------------------------------------------------------

bool CI2PSession::LoadOrCreateDestination(std::string& strPrivKeyRet)
{
    fs::path keyPath = GetDataDir() / "i2p_private_key";

    // Reuse an existing persistent destination if we have one.
    {
        std::ifstream f(keyPath.string().c_str());
        if (f.is_open()) {
            std::string line;
            std::getline(f, line);
            while (!line.empty() &&
                   (line[line.size() - 1] == '\r' || line[line.size() - 1] == '\n'))
                line.erase(line.size() - 1);
            if (!line.empty()) {
                strPrivKeyRet = line;
                printf("I2P: loaded persistent destination from %s\n",
                       keyPath.string().c_str());
                return true;
            }
        }
    }

    // Generate a fresh destination via the bridge (Ed25519, SIGNATURE_TYPE=7).
    SOCKET hSocket = INVALID_SOCKET;
    if (!SamConnect(hSocket) || !SamHandshake(hSocket)) {
        if (hSocket != INVALID_SOCKET) closesocket(hSocket);
        return false;
    }

    bool ok = false;
    if (SamSendLine(hSocket, "DEST GENERATE SIGNATURE_TYPE=7")) {
        std::string reply;
        if (SamRecvLine(hSocket, reply)) {
            std::string priv = SamGetValue(reply, "PRIV");
            if (!priv.empty()) {
                strPrivKeyRet = priv;
                std::ofstream out(keyPath.string().c_str(), std::ios::trunc);
                if (out.is_open()) {
                    out << priv << std::endl;
                    out.close();
                    printf("I2P: generated and saved new persistent destination\n");
                    ok = true;
                } else {
                    printf("I2P: WARNING could not write %s\n", keyPath.string().c_str());
                    ok = true; // still usable for this run
                }
            }
        }
    }
    closesocket(hSocket);
    return ok;
}

bool CI2PSession::CreateSession()
{
    if (!SamConnect(hSession))
        return false;
    if (!SamHandshake(hSession))
        return false;

    std::ostringstream id;
    id << "triangles-" << (uint64_t)GetTime() << "-" << (uint64_t)(GetRand(1000000));
    sessionId = id.str();

    std::string cmd = "SESSION CREATE STYLE=STREAM ID=" + sessionId +
                      " DESTINATION=" + privateKey + " SIGNATURE_TYPE=7";
    if (!SamSendLine(hSession, cmd))
        return false;

    std::string reply;
    if (!SamRecvLine(hSession, reply))
        return false;

    if (SamGetValue(reply, "RESULT") != "OK") {
        printf("I2P: SESSION CREATE failed: %s\n", reply.c_str());
        return false;
    }

    // The bridge echoes the (possibly newly assigned) private key back.
    std::string echoed = SamGetValue(reply, "DESTINATION");
    if (!echoed.empty())
        privateKey = echoed;

    return true;
}

bool CI2PSession::ResolveMyB32()
{
    SOCKET hSocket = INVALID_SOCKET;
    if (!SamConnect(hSocket) || !SamHandshake(hSocket)) {
        if (hSocket != INVALID_SOCKET) closesocket(hSocket);
        return false;
    }

    bool ok = false;
    if (SamSendLine(hSocket, "NAMING LOOKUP NAME=ME")) {
        std::string reply;
        if (SamRecvLine(hSocket, reply) && SamGetValue(reply, "RESULT") == "OK") {
            std::string dest = SamGetValue(reply, "VALUE");
            std::string b32 = DestToB32(dest);
            if (!b32.empty()) {
                std::lock_guard<std::mutex> lock(cs);
                b32Address = b32;
                ok = true;
            }
        }
    }
    closesocket(hSocket);
    return ok;
}

bool CI2PSession::Start()
{
    if (!GetBoolArg("-i2p", true)) {
        printf("I2P: disabled (-i2p=0)\n");
        return false;
    }
    fEnabled.store(true);

    // -i2psam=host:port overrides the default SAM bridge endpoint.
    std::string sam = GetArg("-i2psam", "");
    if (!sam.empty()) {
        int port = I2P_DEFAULT_SAM_PORT;
        std::string host;
        SplitHostPort(sam, port, host);
        if (!host.empty()) samHost = host;
        if (port > 0) samPort = port;
    }

    printf("I2P: connecting to SAM bridge at %s:%d\n", samHost.c_str(), samPort);

    if (!LoadOrCreateDestination(privateKey)) {
        printf("I2P: ERROR could not obtain a destination. Is an I2P router with "
               "the SAM bridge enabled running at %s:%d?\n", samHost.c_str(), samPort);
        return false;
    }

    if (!CreateSession()) {
        printf("I2P: ERROR failed to create SAM STREAM session\n");
        if (hSession != INVALID_SOCKET) { closesocket(hSession); hSession = INVALID_SOCKET; }
        return false;
    }

    if (!ResolveMyB32())
        printf("I2P: WARNING could not resolve our own .b32.i2p address yet\n");

    fActive.store(true);
    fShutdown.store(false);

    printf("I2P: session active. Our address: %s\n", GetB32Address().c_str());

    // Register our I2P address as a local address so peers can learn it.
    CService meI2P;
    if (!b32Address.empty() && meI2P.SetSpecial(b32Address)) {
        meI2P.SetPort((unsigned short)GetListenPort());
        AddLocal(meI2P, LOCAL_MANUAL);
    }

    acceptThread = std::thread(&CI2PSession::AcceptLoop, this);
    return true;
}

void CI2PSession::Stop()
{
    if (!fEnabled.load())
        return;
    fShutdown.store(true);
    fActive.store(false);

    if (hSession != INVALID_SOCKET) {
        closesocket(hSession);
        hSession = INVALID_SOCKET;
    }
    if (acceptThread.joinable())
        acceptThread.join();
    fEnabled.store(false);
    printf("I2P: session stopped\n");
}

// --- inbound ---------------------------------------------------------------

void CI2PSession::AcceptLoop()
{
    while (!fShutdown.load()) {
        SOCKET hSocket = INVALID_SOCKET;
        if (!SamConnect(hSocket) || !SamHandshake(hSocket)) {
            if (hSocket != INVALID_SOCKET) closesocket(hSocket);
            if (fShutdown.load()) break;
            MilliSleep(2000);
            continue;
        }

        // Block here until a peer dials us; the router then streams the remote
        // destination on its own line, after which the socket carries data.
        if (!SamSendLine(hSocket, "STREAM ACCEPT ID=" + sessionId + " SILENT=false")) {
            closesocket(hSocket);
            MilliSleep(1000);
            continue;
        }

        std::string status;
        if (!SamRecvLine(hSocket, status) || SamGetValue(status, "RESULT") != "OK") {
            if (!fShutdown.load())
                printf("I2P: STREAM ACCEPT rejected: %s\n", status.c_str());
            closesocket(hSocket);
            MilliSleep(1000);
            continue;
        }

        std::string remoteDest;
        if (!SamRecvLine(hSocket, remoteDest)) {
            closesocket(hSocket);
            continue;
        }
        if (fShutdown.load()) {
            closesocket(hSocket);
            break;
        }

        // The first token is the remote full destination (base64).
        std::string destTok = remoteDest;
        size_t sp = destTok.find(' ');
        if (sp != std::string::npos)
            destTok = destTok.substr(0, sp);

        std::string b32 = DestToB32(destTok);
        CAddress addr;
        if (b32.empty() || !addr.SetSpecial(b32)) {
            printf("I2P: could not parse inbound remote destination\n");
            closesocket(hSocket);
            continue;
        }
        addr.nServices = 0;
        addr.nTime = GetTime();

        // Hand the live data socket to the net layer as an inbound peer.
        printf("I2P: inbound connection from %s\n", b32.c_str());
        AddI2PInboundNode(hSocket, addr);
    }
}

// --- outbound --------------------------------------------------------------

bool CI2PSession::Connect(const std::string& strDest, SOCKET& hSocketRet)
{
    if (!fActive.load())
        return false;

    SOCKET hSocket = INVALID_SOCKET;
    if (!SamConnect(hSocket) || !SamHandshake(hSocket)) {
        if (hSocket != INVALID_SOCKET) closesocket(hSocket);
        return false;
    }

    if (!SamSendLine(hSocket, "STREAM CONNECT ID=" + sessionId +
                              " DESTINATION=" + strDest + " SILENT=false")) {
        closesocket(hSocket);
        return false;
    }

    std::string status;
    if (!SamRecvLine(hSocket, status) || SamGetValue(status, "RESULT") != "OK") {
        printf("I2P: STREAM CONNECT to %s failed: %s\n", strDest.c_str(), status.c_str());
        closesocket(hSocket);
        return false;
    }

    // Socket is now a bidirectional stream to the peer.
    hSocketRet = hSocket;
    return true;
}

bool StartI2P()
{
    return CI2PSession::GetInstance()->Start();
}

void StopI2P()
{
    CI2PSession::GetInstance()->Stop();
}
