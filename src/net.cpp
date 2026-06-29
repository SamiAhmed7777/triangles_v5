// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "db.h"
#include "net.h"
#include "main.h"
#include "init.h"
#include "strlcpy.h"
#include "addrman.h"
#include "ui_interface.h"
#include "onionseed.h"
#include "tor/onion_v3.h"
#include "snapshotnet.h"
#include "i2p/i2pseed.h"

#include <openssl/ssl.h>
#include <openssl/err.h>

#include <sstream>

#ifdef WIN32
#include <string.h>
#else
#include <sys/uio.h>
#endif

#ifdef USE_UPNP
#include <miniupnpc/miniwget.h>
#include <miniupnpc/miniupnpc.h>
#include <miniupnpc/upnpcommands.h>
#include <miniupnpc/upnperrors.h>
#endif

using namespace std;
using namespace boost;

extern "C" {
// Old embedded Tor v2 removed - using external Tor via SOCKS5 for v3
// int tor_main(int argc, char *argv[]);
}

// Configurable max outbound connections. Set from -maxoutboundconnections
// during network init (StartNode). Default 8, configurable range 4-32.
static int MAX_OUTBOUND_CONNECTIONS = 8;

void ThreadMessageHandler2(void* parg);
void ThreadSocketHandler2(void* parg);
void ThreadOpenConnections2(void* parg);
void ThreadOpenAddedConnections2(void* parg);
#ifdef USE_UPNP
void ThreadMapPort2(void* parg);
#endif
void ThreadHTTPSeedFetch(void* parg);
bool ThreadHTTPSeedFetch2(void* parg);
bool OpenNetworkConnection(const CAddress& addrConnect, CSemaphoreGrant *grantOutbound = nullptr, const char *strDest = nullptr, bool fOneShot = false);


struct LocalServiceInfo {
    int nScore;
    int nPort;
};

//
// Global state variables
//
bool fClient = false;


#ifdef USE_UPNP
bool fUseUPnP = GetBoolArg("-upnp", USE_UPNP);
#else
bool fUseUPnP = false;
#endif
uint64_t nLocalServices = (fClient ? 0 : NODE_NETWORK);
static CCriticalSection cs_mapLocalHost;
static map<CNetAddr, LocalServiceInfo> mapLocalHost;
static bool vfReachable[NET_MAX] = {};
static bool vfLimited[NET_MAX] = {};
static CNode* pnodeLocalHost = nullptr;
CAddress addrSeenByPeer(CService("0.0.0.0", 0), nLocalServices);
uint64_t nLocalHostNonce = 0;
std::array<int, THREAD_MAX> vnThreadsRunning;
static std::vector<SOCKET> vhListenSocket;
CAddrMan addrman;

vector<CNode*> vNodes;
CCriticalSection cs_vNodes;
map<CInv, CDataStream> mapRelay;
deque<pair<int64_t, CInv> > vRelayExpiration;
CCriticalSection cs_mapRelay;
map<CInv, int64_t> mapAlreadyAskedFor;

static deque<string> vOneShots;
CCriticalSection cs_vOneShots;

set<CNetAddr> setservAddNodeAddresses;
CCriticalSection cs_setservAddNodeAddresses;

static CSemaphore *semOutbound = nullptr;

void AddOneShot(string strDest)
{
    LOCK(cs_vOneShots);
    vOneShots.push_back(strDest);
}

unsigned short GetListenPort()
{
    return (unsigned short)(GetArg("-port", GetDefaultPort()));
}

void CNode::PushGetBlocks(CBlockIndex* pindexBegin, uint256 hashEnd)
{
    // Filter out duplicate requests
    if (pindexBegin == pindexLastGetBlocksBegin && hashEnd == hashLastGetBlocksEnd)
        return;
    pindexLastGetBlocksBegin = pindexBegin;
    hashLastGetBlocksEnd = hashEnd;

    PushMessage("getblocks", CBlockLocator(pindexBegin), hashEnd);
}

void CNode::PushGetHeaders(CBlockIndex* pindexBegin, uint256 hashEnd)
{
    if (pindexBegin == pindexLastGetHeadersBegin && hashEnd == hashLastGetHeadersEnd)
        return;
    pindexLastGetHeadersBegin = pindexBegin;
    hashLastGetHeadersEnd = hashEnd;

    PushMessage("getheaders", CBlockLocator(pindexBegin), hashEnd);
}

// find 'best' local address for a particular peer
bool GetLocal(CService& addr, const CNetAddr *paddrPeer)
{
    if (fNoListen)
        return false;

    int nBestScore = -1;
    int nBestReachability = -1;
    {
        LOCK(cs_mapLocalHost);
        for (map<CNetAddr, LocalServiceInfo>::iterator it = mapLocalHost.begin(); it != mapLocalHost.end(); it++)
        {
            int nScore = (*it).second.nScore;
            int nReachability = (*it).first.GetReachabilityFrom(paddrPeer);
            if (nReachability > nBestReachability || (nReachability == nBestReachability && nScore > nBestScore))
            {
                addr = CService((*it).first, (*it).second.nPort);
                nBestReachability = nReachability;
                nBestScore = nScore;
            }
        }
    }
    return nBestScore >= 0;
}

// get best local address for a particular peer as a CAddress
CAddress GetLocalAddress(const CNetAddr *paddrPeer)
{
    CAddress ret(CService("0.0.0.0",0),0);
    CService addr;
    if (GetLocal(addr, paddrPeer))
    {
        ret = CAddress(addr);
        ret.nServices = nLocalServices;
        ret.nTime = GetAdjustedTime();
    }
    return ret;
}

bool RecvLine(SOCKET hSocket, string& strLine)
{
    strLine = "";
    while (true)
    {
        char c;
        int nBytes = recv(hSocket, &c, 1, 0);
        if (nBytes > 0)
        {
            if (c == '\n')
                continue;
            if (c == '\r')
                return true;
            strLine += c;
            if (strLine.size() >= 9000)
                return true;
        }
        else if (nBytes <= 0)
        {
            if (fShutdown)
                return false;
            if (nBytes < 0)
            {
                int nErr = WSAGetLastError();
                if (nErr == WSAEMSGSIZE)
                    continue;
                if (nErr == WSAEWOULDBLOCK || nErr == WSAEINTR || nErr == WSAEINPROGRESS)
                {
                    MilliSleep(10);
                    continue;
                }
            }
            if (!strLine.empty())
                return true;
            if (nBytes == 0)
            {
                // socket closed
                printf("socket closed\n");
                return false;
            }
            else
            {
                // socket error
                int nErr = WSAGetLastError();
                printf("recv failed: %d\n", nErr);
                return false;
            }
        }
    }
}

// used when scores of local addresses may have changed
// pushes better local address to peers
void static AdvertizeLocal()
{
    LOCK(cs_vNodes);
    for (CNode* pnode : vNodes)
    {
        if (pnode->fSuccessfullyConnected)
        {
            CAddress addrLocal = GetLocalAddress(&pnode->addr);
            if (addrLocal.IsRoutable() && (CService)addrLocal != (CService)pnode->addrLocal)
            {
                pnode->PushAddress(addrLocal);
                pnode->addrLocal = addrLocal;
            }
        }
    }
}

void SetReachable(enum Network net, bool fFlag)
{
    LOCK(cs_mapLocalHost);
    vfReachable[net] = fFlag;
    if (net == NET_IPV6 && fFlag)
        vfReachable[NET_IPV4] = true;
}

// learn a new local address
bool AddLocal(const CService& addr, int nScore)
{
    if (!addr.IsRoutable())
        return false;

    if (nScore < LOCAL_MANUAL)
        return false;

    if (IsLimited(addr))
        return false;

    printf("AddLocal(%s,%i)\n", addr.ToString().c_str(), nScore);

    {
        LOCK(cs_mapLocalHost);
        bool fAlready = mapLocalHost.count(addr) > 0;
        LocalServiceInfo &info = mapLocalHost[addr];
        if (!fAlready || nScore >= info.nScore) {
            info.nScore = nScore + (fAlready ? 1 : 0);
            info.nPort = addr.GetPort();
        }
        SetReachable(addr.GetNetwork());
    }

    AdvertizeLocal();

    return true;
}

bool AddLocal(const CNetAddr &addr, int nScore)
{
    return AddLocal(CService(addr, GetListenPort()), nScore);
}

/** Make a particular network entirely off-limits (no automatic connects to it) */
void SetLimited(enum Network net, bool fLimited)
{
    if (net == NET_UNROUTABLE)
        return;
    LOCK(cs_mapLocalHost);
    vfLimited[net] = fLimited;
}

bool IsLimited(enum Network net)
{
    LOCK(cs_mapLocalHost);
    return vfLimited[net];
}

bool IsLimited(const CNetAddr &addr)
{
    return IsLimited(addr.GetNetwork());
}

/** vote for a local address */
bool SeenLocal(const CService& addr)
{
    {
        LOCK(cs_mapLocalHost);
        if (mapLocalHost.count(addr) == 0)
            return false;
        mapLocalHost[addr].nScore++;
    }

    AdvertizeLocal();

    return true;
}

/** check whether a given address is potentially local */
bool IsLocal(const CService& addr)
{
    LOCK(cs_mapLocalHost);
    return mapLocalHost.count(addr) > 0;
}

/** check whether a given address is in a network we can probably connect to */
bool IsReachable(const CNetAddr& addr)
{
    LOCK(cs_mapLocalHost);
    enum Network net = addr.GetNetwork();
    return vfReachable[net] && !vfLimited[net];
}

// ────────────────────────────────────────────────────────────────────────────
// Cross-network Tor ↔ I2P peer discovery helpers
// ────────────────────────────────────────────────────────────────────────────

/**
 * Check whether a CAddress refers to an I2P (.b32.i2p) endpoint.
 * Returns true if the string representation of the address contains ".i2p".
 */
bool IsI2PAddr(const CAddress& addr)
{
    std::string addrStr = addr.ToStringIP();
    return (addrStr.find(".i2p") != std::string::npos);
}

/**
 * Check whether a CAddress refers to a Tor (.onion) endpoint.
 */
static bool IsOnionAddr(const CAddress& addr)
{
    std::string addrStr = addr.ToStringIP();
    return (addrStr.find(".onion") != std::string::npos);
}

/**
 * Cross-network address relay: when an 'addr' message is received from a
 * peer on one anonymity network, this function bridges addresses belonging
 * to the *other* network to the appropriate peers.
 *
 *   - .b32.i2p addresses received from any peer → relay to I2P-connected peers
 *   - .onion addresses received from any peer    → relay to Tor-connected peers
 *
 * This breaks the isolation between Tor and I2P peer sets so that a Tor
 * node can learn about I2P peers and vice versa.
 */
void RelayCrossNetworkAddr(const std::vector<CAddress>& vAddr)
{
    bool hasI2P = false;
    bool hasOnion = false;
    for (const CAddress& addr : vAddr) {
        if (IsI2PAddr(addr))   hasI2P = true;
        if (IsOnionAddr(addr)) hasOnion = true;
    }
    if (!hasI2P && !hasOnion)
        return;

    LOCK(cs_vNodes);
    for (CNode* pnode : vNodes) {
        if (pnode->fDisconnect)
            continue;
        std::string peerAddr = pnode->addr.ToStringIP();
        bool peerIsI2P   = (peerAddr.find(".i2p")   != std::string::npos);
        bool peerIsOnion = (peerAddr.find(".onion") != std::string::npos);

        for (const CAddress& addr : vAddr) {
            // Bridge I2P addresses to I2P peers
            if (hasI2P && IsI2PAddr(addr) && peerIsI2P) {
                pnode->PushAddress(addr);
            }
            // Bridge .onion addresses to Tor peers
            if (hasOnion && IsOnionAddr(addr) && peerIsOnion) {
                pnode->PushAddress(addr);
            }
            // Cross-bridge: also push I2P addresses to Tor peers and
            // .onion addresses to I2P peers so each network learns about
            // the other's peers.
            if (hasI2P && IsI2PAddr(addr) && peerIsOnion) {
                pnode->PushAddress(addr);
            }
            if (hasOnion && IsOnionAddr(addr) && peerIsI2P) {
                pnode->PushAddress(addr);
            }
        }
    }

    if (fDebug && (hasI2P || hasOnion))
        printf("RelayCrossNetworkAddr: bridged %s%s%s addresses across networks\n",
               hasOnion ? ".onion " : "", hasI2P ? ".i2p " : "",
               (hasOnion && hasI2P) ? "(both)" : "");
}

bool GetMyExternalIP2(const CService& addrConnect, const char* pszGet, const char* pszKeyword, CNetAddr& ipRet)
{
    SOCKET hSocket;
    if (!ConnectSocket(addrConnect, hSocket))
        return error("GetMyExternalIP() : connection to %s failed", addrConnect.ToString().c_str());

    send(hSocket, pszGet, strlen(pszGet), MSG_NOSIGNAL);

    string strLine;
    while (RecvLine(hSocket, strLine))
    {
        if (strLine.empty()) // HTTP response is separated from headers by blank line
        {
            while (true)
            {
                if (!RecvLine(hSocket, strLine))
                {
                    closesocket(hSocket);
                    return false;
                }
                if (pszKeyword == nullptr)
                    break;
                if (strLine.find(pszKeyword) != string::npos)
                {
                    strLine = strLine.substr(strLine.find(pszKeyword) + strlen(pszKeyword));
                    break;
                }
            }
            closesocket(hSocket);
            if (strLine.find("<") != string::npos)
                strLine = strLine.substr(0, strLine.find("<"));
            strLine = strLine.substr(strspn(strLine.c_str(), " \t\n\r"));
            while (strLine.size() > 0 && isspace(strLine[strLine.size()-1]))
                strLine.resize(strLine.size()-1);
            CService addr(strLine,0,true);
            printf("GetMyExternalIP() received [%s] %s\n", strLine.c_str(), addr.ToString().c_str());
            if (!addr.IsValid() || !addr.IsRoutable())
                return false;
            ipRet.SetIP(addr);
            return true;
        }
    }
    closesocket(hSocket);
    return error("GetMyExternalIP() : connection closed");
}

// We now get our external IP from the IRC server first and only use this as a backup
bool GetMyExternalIP(CNetAddr& ipRet)
{
    CService addrConnect;
    const char* pszGet;
    const char* pszKeyword;

    for (int nLookup = 0; nLookup <= 1; nLookup++)
    for (int nHost = 1; nHost <= 2; nHost++)
    {
        // We should be phasing out our use of sites like these.  If we need
        // replacements, we should ask for volunteers to put this simple
        // php file on their web server that prints the client IP:
        //  <?php echo $_SERVER["REMOTE_ADDR"]; ?>
        if (nHost == 1)
        {
            addrConnect = CService("91.198.22.70",80); // checkip.dyndns.org

            if (nLookup == 1)
            {
                CService addrIP("checkip.dyndns.org", 80, true);
                if (addrIP.IsValid())
                    addrConnect = addrIP;
            }

            pszGet = "GET / HTTP/1.1\r\n"
                     "Host: checkip.dyndns.org\r\n"
                     "User-Agent: Triangles\r\n"
                     "Connection: close\r\n"
                     "\r\n";

            pszKeyword = "Address:";
        }
        else if (nHost == 2)
        {
            addrConnect = CService("74.208.43.192", 80); // www.showmyip.com

            if (nLookup == 1)
            {
                CService addrIP("www.showmyip.com", 80, true);
                if (addrIP.IsValid())
                    addrConnect = addrIP;
            }

            pszGet = "GET /simple/ HTTP/1.1\r\n"
                     "Host: www.showmyip.com\r\n"
                     "User-Agent: Triangles\r\n"
                     "Connection: close\r\n"
                     "\r\n";

            pszKeyword = nullptr; // Returns just IP address
        }

        if (GetMyExternalIP2(addrConnect, pszGet, pszKeyword, ipRet))
            return true;
    }

    return false;
}

void ThreadGetMyExternalIP(void* parg)
{
    // Make this thread recognisable as the external IP detection thread
    RenameThread("Triangles-ext-ip");

    CNetAddr addrLocalHost;
    if (GetMyExternalIP(addrLocalHost))
    {
        printf("GetMyExternalIP() returned %s\n", addrLocalHost.ToStringIP().c_str());
        AddLocal(addrLocalHost, LOCAL_HTTP);
    }
}





void AddressCurrentlyConnected(const CService& addr)
{
    addrman.Connected(addr);
}







CNode* FindNode(const CNetAddr& ip)
{
    {
        LOCK(cs_vNodes);
        for (CNode* pnode : vNodes)
            if ((CNetAddr)pnode->addr == ip)
                return (pnode);
    }
    return nullptr;
}

CNode* FindNode(std::string addrName)
{
    LOCK(cs_vNodes);
    for (CNode* pnode : vNodes)
        if (pnode->addrName == addrName)
            return (pnode);
    return nullptr;
}

CNode* FindNode(const CService& addr)
{
    {
        LOCK(cs_vNodes);
        for (CNode* pnode : vNodes)
            if ((CService)pnode->addr == addr)
                return (pnode);
    }
    return nullptr;
}

CNode* ConnectNode(CAddress addrConnect, const char *pszDest)
{
    // TOR+I2P NATIVE: Reject all clearnet (non-.onion, non-.b32.i2p) addresses
    std::string addrStr = pszDest ? std::string(pszDest) : addrConnect.ToStringIP();
    bool isOnion = (addrStr.find(".onion") != std::string::npos);
    bool isI2P = (addrStr.find(".i2p") != std::string::npos);
    if (!isOnion && !isI2P) {
        if (fDebug)
            printf("ConnectNode(): REJECTED clearnet address: %s (Tor/I2P native mode)\n", addrStr.c_str());
        return nullptr;
    }

    if (pszDest == nullptr) {
        if (IsLocal(addrConnect))
            return nullptr;

        // Look for an existing connection
        CNode* pnode = FindNode((CService)addrConnect);
        if (pnode)
        {
            pnode->AddRef();
            return pnode;
        }
    }

    if (fDebug) {
         printf("ConnectNode(): pszDest: %s\n", pszDest);
    }

    /// debug print
    printf("trying connection %s lastseen=%.1fhrs\n",
        pszDest ? pszDest : addrConnect.ToString().c_str(),
        pszDest ? 0 : (double)(GetAdjustedTime() - addrConnect.nTime)/3600.0);

    // Connect
    SOCKET hSocket;
    if (pszDest ? ConnectSocketByName(addrConnect, hSocket, pszDest, GetDefaultPort()) : ConnectSocket(addrConnect, hSocket))
    {
        addrman.Attempt(addrConnect);

        /// debug print
        printf("connected %s\n", pszDest ? pszDest : addrConnect.ToString().c_str());

        // Set to non-blocking
#ifdef WIN32
        u_long nOne = 1;
        if (ioctlsocket(hSocket, FIONBIO, &nOne) == SOCKET_ERROR)
            printf("ConnectSocket() : ioctlsocket non-blocking setting failed, error %d\n", WSAGetLastError());
#else
        if (fcntl(hSocket, F_SETFL, O_NONBLOCK) == SOCKET_ERROR)
            printf("ConnectSocket() : fcntl non-blocking setting failed, error %d\n", errno);
#endif

        // Add node
        CNode* pnode = new CNode(hSocket, addrConnect, pszDest ? pszDest : "", false);
        pnode->AddRef();

        {
            LOCK(cs_vNodes);
            vNodes.push_back(pnode);
        }

        pnode->nTimeConnected = GetTime();
        return pnode;
    }
    else
    {
        return nullptr;
    }
}

// Adopt a connected I2P SAM data socket (from the accept loop in i2p.cpp) as an
// inbound peer. The socket arrives in blocking mode; switch it to non-blocking
// to match the rest of the socket handler, then register the node.
void AddI2PInboundNode(SOCKET hSocket, const CAddress& addr)
{
    if (hSocket == INVALID_SOCKET)
        return;

    if (CNode::IsBanned(addr)) {
        printf("I2P inbound from %s dropped (banned)\n", addr.ToString().c_str());
        closesocket(hSocket);
        return;
    }

    // Honour the inbound connection limit.
    int nInbound = 0;
    {
        LOCK(cs_vNodes);
        for (CNode* pnode : vNodes)
            if (pnode->fInbound)
                nInbound++;
    }
    int nMaxInbound = GetArg("-maxconnections", 125) - MAX_OUTBOUND_CONNECTIONS;
    if (nInbound >= nMaxInbound) {
        printf("I2P inbound from %s dropped (too many inbound)\n", addr.ToString().c_str());
        closesocket(hSocket);
        return;
    }

#ifdef WIN32
    u_long nOne = 1;
    if (ioctlsocket(hSocket, FIONBIO, &nOne) == SOCKET_ERROR)
        printf("AddI2PInboundNode() : ioctlsocket non-blocking setting failed, error %d\n", WSAGetLastError());
#else
    if (fcntl(hSocket, F_SETFL, O_NONBLOCK) == SOCKET_ERROR)
        printf("AddI2PInboundNode() : fcntl non-blocking setting failed, error %d\n", errno);
#endif

    printf("accepted I2P connection %s\n", addr.ToString().c_str());
    CNode* pnode = new CNode(hSocket, addr, "", true);
    pnode->AddRef();
    pnode->nTimeConnected = GetTime();
    {
        LOCK(cs_vNodes);
        vNodes.push_back(pnode);
    }
}

void CNode::CloseSocketDisconnect()
{
    fDisconnect = true;
    // Option C: track this disconnect for the reliability score. We increment
    // BEFORE closing the socket so a flurry of disconnects from one peer is
    // visible to the next sync manager tick (which iterates cs_vNodes).
    ++nDisconnectCount;
    nLastDisconnectTime = GetTime();
    // Penalize the score by 25 per disconnect. Flapping peers (5+ in 5min) get
    // an extra 50 penalty applied in the score recompute.
    nReliabilityScore = std::max(0, nReliabilityScore - 25);
    if (hSocket != INVALID_SOCKET)
    {
        printf("disconnecting node %s\n", addrName.c_str());
        closesocket(hSocket);
        hSocket = INVALID_SOCKET;

        // in case this fails, we'll empty the recv buffer when the CNode is deleted
        TRY_LOCK(cs_vRecvMsg, lockRecv);
        if (lockRecv)
            vRecvMsg.clear();
    }
}

void CNode::Cleanup()
{
}

int CNode::RecomputeReliabilityScore()
{
    // Option C: compute reliability score from current counters.
    //
    // Base: 100
    // -10 per connect failure (host unreachable on attempt)
    // -25 per disconnect (also applied immediately in CloseSocketDisconnect,
    //   but we re-apply here so a fresh CNode that started with a low score
    //   can recover)
    // +5 per block delivered, capped at +200
    // -50 if the peer has flapped (5+ disconnects in the last 5 minutes)
    //
    // Floor: 0 (peer effectively banned from sync)
    // Ceiling: 500
    int score = 100;
    score -= 10 * nConnectFailures;
    score -= 25 * nDisconnectCount;
    int deliveryBonus = std::min(200, 5 * nBlocksDelivered);
    score += deliveryBonus;

    if (nDisconnectCount >= 5) {
        // Flapping detection: 5+ disconnects in the peer's lifetime.
        // We can't easily check "last 5 min" without history, so we use
        // total count as a proxy. A peer that connects/disconnects a lot
        // is unreliable regardless of timing.
        score -= 50;
    }

    if (score < 0) score = 0;
    if (score > 500) score = 500;
    nReliabilityScore = score;
    return score;
}


void CNode::PushVersion()
{
    /// when NTP implemented, change to just nTime = GetAdjustedTime()
    int64_t nTime = (fInbound ? GetAdjustedTime() : GetTime());
    CAddress addrYou = (addr.IsRoutable() && !IsProxy(addr) ? addr : CAddress(CService("0.0.0.0",0)));
    CAddress addrMe = GetLocalAddress(&addr);
    RAND_bytes((unsigned char*)&nLocalHostNonce, sizeof(nLocalHostNonce));
    printf("send version message: version %d, blocks=%d, us=%s, them=%s, peer=%s\n", PROTOCOL_VERSION, nBestHeight, addrMe.ToString().c_str(), addrYou.ToString().c_str(), addr.ToString().c_str());
    PushMessage("version", PROTOCOL_VERSION, nLocalServices, nTime, addrYou, addrMe,
                nLocalHostNonce, FormatSubVersion(CLIENT_NAME, CLIENT_VERSION, std::vector<string>()), nBestHeight);
}





std::map<CNetAddr, int64_t> CNode::setBanned;
CCriticalSection CNode::cs_setBanned;

void CNode::ClearBanned()
{
    setBanned.clear();
}

bool CNode::IsBanned(CNetAddr ip)
{
    bool fResult = false;
    {
        LOCK(cs_setBanned);
        std::map<CNetAddr, int64_t>::iterator i = setBanned.find(ip);
        if (i != setBanned.end())
        {
            int64_t t = (*i).second;
            if (GetTime() < t)
                fResult = true;
        }
    }
    return fResult;
}

bool CNode::Ban(CNetAddr ip, int64_t banTime)
{
    if (ip.IsLocal())
        return false;

    LOCK(cs_setBanned);
    std::map<CNetAddr, int64_t>::iterator it = setBanned.find(ip);
    if (it != setBanned.end() && it->second >= banTime)
        return false;

    setBanned[ip] = banTime;
    return true;
}

bool CNode::Unban(CNetAddr ip)
{
    LOCK(cs_setBanned);
    return setBanned.erase(ip) != 0;
}

void CNode::GetBanned(std::map<CNetAddr, int64_t>& mapBannedOut)
{
    LOCK(cs_setBanned);
    mapBannedOut = setBanned;
}

bool CNode::Misbehaving(int howmuch)
{
    if (addr.IsLocal())
    {
        printf("Warning: Local node %s misbehaving (delta: %d)!\n", addrName.c_str(), howmuch);
        return false;
    }

    nMisbehavior += howmuch;
    if (nMisbehavior >= GetArg("-banscore", 100))
    {
        int64_t banTime = GetTime()+GetArg("-bantime", 60*60*24);  // Default 24-hour ban
        printf("Misbehaving: %s (%d -> %d) DISCONNECTING\n", addr.ToString().c_str(), nMisbehavior-howmuch, nMisbehavior);
        {
            LOCK(cs_setBanned);
            if (setBanned[addr] < banTime)
                setBanned[addr] = banTime;
        }
        CloseSocketDisconnect();
        return true;
    } else
        printf("Misbehaving: %s (%d -> %d)\n", addr.ToString().c_str(), nMisbehavior-howmuch, nMisbehavior);
    return false;
}

#undef X
#define X(name) stats.name = name
void CNode::copyStats(CNodeStats &stats)
{
    X(nServices);
    X(nLastSend);
    X(nLastRecv);
    X(nSendBytes);
    X(nRecvBytes);
    X(nTimeConnected);
    X(addrName);
    X(nVersion);
    X(strSubVer);
    X(fInbound);
    X(nStartingHeight);
    X(nMisbehavior);
    X(nPingUsecTime);
    X(nBlocksDelivered);
    X(nAvgBlockLatencyUs);
}
#undef X


// requires LOCK(cs_vRecvMsg)
bool CNode::ReceiveMsgBytes(const char *pch, unsigned int nBytes)
{
    while (nBytes > 0) {

        // get current incomplete message, or create a new one
        if (vRecvMsg.empty() ||
            vRecvMsg.back().complete())
            vRecvMsg.push_back(CNetMessage(SER_NETWORK, nRecvVersion));

        CNetMessage& msg = vRecvMsg.back();

        // absorb network data
        int handled;
        if (!msg.in_data)
            handled = msg.readHeader(pch, nBytes);
        else
            handled = msg.readData(pch, nBytes);

        if (handled < 0)
                return false;

        pch += handled;
        nBytes -= handled;

        if (msg.complete())
            msg.nTime = GetTimeMicros();
    }

    return true;
}

int CNetMessage::readHeader(const char *pch, unsigned int nBytes)
{
    // copy data to temporary parsing buffer
    unsigned int nRemaining = 24 - nHdrPos;
    unsigned int nCopy = std::min(nRemaining, nBytes);

    memcpy(&hdrbuf[nHdrPos], pch, nCopy);
    nHdrPos += nCopy;

    // if header incomplete, exit
    if (nHdrPos < 24)
        return nCopy;

    // deserialize to CMessageHeader
    try {
        hdrbuf >> hdr;
    }
    catch (std::exception &e) {
        return -1;
    }

    // reject messages larger than MAX_SIZE
    if (hdr.nMessageSize > MAX_SIZE)
            return -1;

    // switch state to reading message data
    in_data = true;
    vRecv.resize(hdr.nMessageSize);

    return nCopy;
}

int CNetMessage::readData(const char *pch, unsigned int nBytes)
{
    unsigned int nRemaining = hdr.nMessageSize - nDataPos;
    unsigned int nCopy = std::min(nRemaining, nBytes);

    memcpy(&vRecv[nDataPos], pch, nCopy);
    nDataPos += nCopy;

    return nCopy;
}









// requires LOCK(cs_vSend)
void SocketSendData(CNode *pnode)
{
    std::deque<CSerializeData>::iterator it = pnode->vSendMsg.begin();

    while (it != pnode->vSendMsg.end()) {
#ifndef WIN32
        // Coalesce up to MAX_IOV queued messages into a single syscall using
        // scatter-gather I/O.  On Linux we use sendmsg() so we can pass
        // MSG_NOSIGNAL | MSG_DONTWAIT; on other POSIX systems (e.g. BSD where
        // SO_NOSIGPIPE is already set on the socket) we fall back to writev().
        static const int MAX_IOV = 16;
        struct iovec iov[MAX_IOV];
        int iovcnt = 0;
        std::deque<CSerializeData>::iterator batchEnd = it;

        for (; batchEnd != pnode->vSendMsg.end() && iovcnt < MAX_IOV; ++batchEnd, ++iovcnt) {
            const CSerializeData &data = *batchEnd;
            size_t off = (batchEnd == it) ? pnode->nSendOffset : 0;
            assert(data.size() > off);
            iov[iovcnt].iov_base = const_cast<char*>(&data[off]);
            iov[iovcnt].iov_len = data.size() - off;
        }

        if (iovcnt == 0)
            break;

        ssize_t nBytes;
#ifdef MSG_NOSIGNAL
        struct msghdr msg;
        memset(&msg, 0, sizeof(msg));
        msg.msg_iov = iov;
        msg.msg_iovlen = iovcnt;
        nBytes = sendmsg(pnode->hSocket, &msg, MSG_NOSIGNAL | MSG_DONTWAIT);
#else
        nBytes = writev(pnode->hSocket, iov, iovcnt);
#endif
        if (nBytes > 0) {
            pnode->nLastSend = GetTime();
            pnode->nSendBytes += nBytes;

            // Consume nBytes across the coalesced messages
            while (it != batchEnd && nBytes > 0) {
                const CSerializeData &data = *it;
                size_t remaining = data.size() - pnode->nSendOffset;
                if ((size_t)nBytes >= remaining) {
                    nBytes -= remaining;
                    pnode->nSendSize -= data.size();
                    pnode->nSendOffset = 0;
                    ++it;
                } else {
                    pnode->nSendOffset += nBytes;
                    nBytes = 0;
                }
            }
            // Socket buffer full mid-batch — wait for next cycle
            if (it != batchEnd)
                break;
        } else if (nBytes < 0) {
            int nErr = WSAGetLastError();
            if (nErr != WSAEWOULDBLOCK && nErr != WSAEMSGSIZE && nErr != WSAEINTR && nErr != WSAEINPROGRESS) {
                printf("socket send error %d\n", nErr);
                pnode->CloseSocketDisconnect();
            }
            break;
        } else {
            // nBytes == 0: peer closed
            break;
        }
#else
        // Windows: individual send() calls
        const CSerializeData &data = *it;
        assert(data.size() > pnode->nSendOffset);
        int nBytes = send(pnode->hSocket, &data[pnode->nSendOffset], data.size() - pnode->nSendOffset, MSG_NOSIGNAL | MSG_DONTWAIT);
        if (nBytes > 0) {
            pnode->nLastSend = GetTime();
            pnode->nSendOffset += nBytes;
            pnode->nSendBytes += nBytes;
            if (pnode->nSendOffset == data.size()) {
                pnode->nSendOffset = 0;
                pnode->nSendSize -= data.size();
                it++;
            } else {
                break;
            }
        } else {
            if (nBytes < 0) {
                int nErr = WSAGetLastError();
                if (nErr != WSAEWOULDBLOCK && nErr != WSAEMSGSIZE && nErr != WSAEINTR && nErr != WSAEINPROGRESS) {
                    printf("socket send error %d\n", nErr);
                    pnode->CloseSocketDisconnect();
                }
            }
            break;
        }
#endif
    }

    if (it == pnode->vSendMsg.end()) {
        assert(pnode->nSendOffset == 0);
        assert(pnode->nSendSize == 0);
    }
    pnode->vSendMsg.erase(pnode->vSendMsg.begin(), it);
}








void ThreadSocketHandler(void* parg)
{
    // Make this thread recognisable as the networking thread
    RenameThread("Triangles-net");

    try
    {
        vnThreadsRunning[THREAD_SOCKETHANDLER]++;
        ThreadSocketHandler2(parg);
        vnThreadsRunning[THREAD_SOCKETHANDLER]--;
    }
    catch (std::exception& e) {
        vnThreadsRunning[THREAD_SOCKETHANDLER]--;
        PrintException(&e, "ThreadSocketHandler()");
    } catch (...) {
        vnThreadsRunning[THREAD_SOCKETHANDLER]--;
        throw; // support pthread_cancel()
    }
    printf("ThreadSocketHandler exited\n");
}

void ThreadSocketHandler2(void* parg)
{
    printf("ThreadSocketHandler started\n");
    list<CNode*> vNodesDisconnected;
    unsigned int nPrevNodeCount = 0;

    while (true)
    {
        //
        // Disconnect nodes
        //
        {
            LOCK(cs_vNodes);
            // Disconnect unused nodes
            vector<CNode*> vNodesCopy = vNodes;
            for (CNode* pnode : vNodesCopy)
            {
                if (pnode->fDisconnect ||
                    (pnode->GetRefCount() <= 0 && pnode->vRecvMsg.empty() && pnode->nSendSize == 0 && pnode->ssSend.empty()))
                {
                    // remove from vNodes
                    vNodes.erase(remove(vNodes.begin(), vNodes.end(), pnode), vNodes.end());

                    // release outbound grant (if any)
                    pnode->grantOutbound.Release();

                    // close socket and cleanup
                    pnode->CloseSocketDisconnect();
                    pnode->Cleanup();

                    // hold in disconnected pool until all refs are released
                    if (pnode->fNetworkNode || pnode->fInbound)
                        pnode->Release();
                    vNodesDisconnected.push_back(pnode);
                }
            }

            // Delete disconnected nodes
            list<CNode*> vNodesDisconnectedCopy = vNodesDisconnected;
            for (CNode* pnode : vNodesDisconnectedCopy)
            {
                // wait until threads are done using it
                if (pnode->GetRefCount() <= 0)
                {
                    bool fDelete = false;
                    {
                        TRY_LOCK(pnode->cs_vSend, lockSend);
                        if (lockSend)
                        {
                            TRY_LOCK(pnode->cs_vRecvMsg, lockRecv);
                            if (lockRecv)
                            {
                                TRY_LOCK(pnode->cs_inventory, lockInv);
                                if (lockInv)
                                    fDelete = true;
                            }
                        }
                    }
                    if (fDelete)
                    {
                        vNodesDisconnected.remove(pnode);
                        delete pnode;
                    }
                }
            }
        }
        {
            // Read vNodes.size() under the lock to avoid data race
            unsigned int nNodeCount;
            {
                LOCK(cs_vNodes);
                nNodeCount = vNodes.size();
            }
            if (nNodeCount != nPrevNodeCount)
            {
                nPrevNodeCount = nNodeCount;
                if (!fShutdown)
                    uiInterface.NotifyNumConnectionsChanged(nNodeCount);
            }
        }


        //
        // Find which sockets have data to receive
        //
        struct timeval timeout;
        timeout.tv_sec  = 0;
        timeout.tv_usec = IsInitialBlockDownload() ? 1000 : 50000; // 1ms during IBD, 50ms normal

        fd_set fdsetRecv;
        fd_set fdsetSend;
        fd_set fdsetError;
        FD_ZERO(&fdsetRecv);
        FD_ZERO(&fdsetSend);
        FD_ZERO(&fdsetError);
        SOCKET hSocketMax = 0;
        bool have_fds = false;

        for (SOCKET hListenSocket : vhListenSocket) {
            FD_SET(hListenSocket, &fdsetRecv);
            hSocketMax = max(hSocketMax, hListenSocket);
            have_fds = true;
        }
        {
            LOCK(cs_vNodes);
            for (CNode* pnode : vNodes)
            {
                if (pnode->hSocket == INVALID_SOCKET)
                    continue;
                FD_SET(pnode->hSocket, &fdsetRecv);
                FD_SET(pnode->hSocket, &fdsetError);
                hSocketMax = max(hSocketMax, pnode->hSocket);
                have_fds = true;
                {
                    TRY_LOCK(pnode->cs_vSend, lockSend);
                    if (lockSend && !pnode->ssSend.empty())
                        FD_SET(pnode->hSocket, &fdsetSend);
                }
            }
        }

        vnThreadsRunning[THREAD_SOCKETHANDLER]--;
        int nSelect = select(have_fds ? hSocketMax + 1 : 0,
                             &fdsetRecv, &fdsetSend, &fdsetError, &timeout);
        vnThreadsRunning[THREAD_SOCKETHANDLER]++;
        if (fShutdown)
            return;
        if (nSelect == SOCKET_ERROR)
        {
            if (have_fds)
            {
                int nErr = WSAGetLastError();
                printf("socket select error %d\n", nErr);
                for (unsigned int i = 0; i <= hSocketMax; i++)
                    FD_SET(i, &fdsetRecv);
            }
            FD_ZERO(&fdsetSend);
            FD_ZERO(&fdsetError);
            MilliSleep(timeout.tv_usec/1000);
        }


        //
        // Accept new connections
        //
        for (SOCKET hListenSocket : vhListenSocket)
        if (hListenSocket != INVALID_SOCKET && FD_ISSET(hListenSocket, &fdsetRecv))
        {
#ifdef USE_IPV6
            struct sockaddr_storage sockaddr;
#else
            struct sockaddr sockaddr;
#endif
            socklen_t len = sizeof(sockaddr);
            SOCKET hSocket = accept(hListenSocket, (struct sockaddr*)&sockaddr, &len);
            CAddress addr;
            int nInbound = 0;

            if (hSocket != INVALID_SOCKET)
                if (!addr.SetSockAddr((const struct sockaddr*)&sockaddr))
                    printf("Warning: Unknown socket family\n");

            {
                LOCK(cs_vNodes);
                for (CNode* pnode : vNodes)
                    if (pnode->fInbound)
                        nInbound++;
            }

            if (hSocket == INVALID_SOCKET)
            {
                int nErr = WSAGetLastError();
                if (nErr != WSAEWOULDBLOCK)
                    printf("socket error accept failed: %d\n", nErr);
            }
            else if (CNode::IsBanned(addr))
            {
                printf("connection from %s dropped (banned)\n", addr.ToString().c_str());
                closesocket(hSocket);
            }
            else
            {
                int nMaxInbound = GetArg("-maxconnections", 125) - MAX_OUTBOUND_CONNECTIONS;
                bool fAccept = (nInbound < nMaxInbound);

                // Reserve 2 extra inbound slots for known seed nodes
                if (!fAccept) {
                    bool fIsSeed = false;
                    static const char *(*strOnionSeedCheck)[1] = fTestNet ? strTestNetOnionSeed : strMainNetOnionSeed;
                    std::string incomingAddr = addr.ToStringIP();
                    for (unsigned int si = 0; strOnionSeedCheck[si][0] != nullptr; si++) {
                        if (incomingAddr.find(strOnionSeedCheck[si][0]) != std::string::npos) {
                            fIsSeed = true;
                            break;
                        }
                    }
                    // Also check I2P seed addresses
                    if (!fIsSeed) {
                        static const char *(*strI2PSeedCheck)[1] = fTestNet ? strTestNetI2PSeed : strMainNetI2PSeed;
                        for (unsigned int si = 0; strI2PSeedCheck[si][0] != nullptr; si++) {
                            if (incomingAddr.find(strI2PSeedCheck[si][0]) != std::string::npos) {
                                fIsSeed = true;
                                break;
                            }
                        }
                    }
                    if (fIsSeed && nInbound < nMaxInbound + 2) {
                        fAccept = true;
                        printf("accepted seed node %s (reserved slot)\n", addr.ToString().c_str());
                    }
                }

                if (fAccept) {
                    printf("accepted connection %s\n", addr.ToString().c_str());
                    CNode* pnode = new CNode(hSocket, addr, "", true);
                    pnode->AddRef();
                    {
                        LOCK(cs_vNodes);
                        vNodes.push_back(pnode);
                    }
                } else {
                    closesocket(hSocket);
                }
            }
        }


        //
        // Service each socket
        //
        vector<CNode*> vNodesCopy;
        {
            LOCK(cs_vNodes);
            vNodesCopy = vNodes;
            for (CNode* pnode : vNodesCopy)
                pnode->AddRef();
        }
        for (CNode* pnode : vNodesCopy)
        {
            if (fShutdown)
                break;

            //
            // Receive
            //
            if (pnode->hSocket == INVALID_SOCKET)
                continue;
            if (FD_ISSET(pnode->hSocket, &fdsetRecv) || FD_ISSET(pnode->hSocket, &fdsetError))
            {
                TRY_LOCK(pnode->cs_vRecvMsg, lockRecv);
                if (lockRecv)
                {
                    if (pnode->GetTotalRecvSize() > ReceiveFloodSize()) {
                        if (!pnode->fDisconnect)
                            printf("socket recv flood control disconnect (%u bytes)\n", pnode->GetTotalRecvSize());
                        pnode->CloseSocketDisconnect();
                    }
                    else {
                        // typical socket buffer is 8K-64K
                        char pchBuf[0x10000];
                        int nBytes = recv(pnode->hSocket, pchBuf, sizeof(pchBuf), MSG_DONTWAIT);
                        if (nBytes > 0)
                        {
                            if (!pnode->ReceiveMsgBytes(pchBuf, nBytes))
                                pnode->CloseSocketDisconnect();
                            pnode->nLastRecv = GetTime();
                            pnode->nRecvBytes += nBytes;
                        }
                        else if (nBytes == 0)
                        {
                            // socket closed gracefully
                            if (!pnode->fDisconnect)
                                printf("socket closed\n");
                            pnode->CloseSocketDisconnect();
                        }
                        else if (nBytes < 0)
                        {
                            // error
                            int nErr = WSAGetLastError();
                            if (nErr != WSAEWOULDBLOCK && nErr != WSAEMSGSIZE && nErr != WSAEINTR && nErr != WSAEINPROGRESS)
                            {
                                if (!pnode->fDisconnect)
                                    printf("socket recv error %d\n", nErr);
                                pnode->CloseSocketDisconnect();
                            }
                        }
                    }
                }
            }

            //
            // Send
            //
            if (pnode->hSocket == INVALID_SOCKET)
                continue;
            if (FD_ISSET(pnode->hSocket, &fdsetSend))
            {
                TRY_LOCK(pnode->cs_vSend, lockSend);
                if (lockSend)
                    SocketSendData(pnode);
            }

            //
            // Inactivity checking
            //
            int64_t nTime = GetTime();
            if (nTime - pnode->nTimeConnected > 60)
            {
                if (pnode->nLastRecv == 0 || pnode->nLastSend == 0)
                {
                    printf("socket no message in first 60 seconds, %d %d\n", pnode->nLastRecv != 0, pnode->nLastSend != 0);
                    pnode->fDisconnect = true;
                }
                else if (GetTime() - pnode->nLastSend > 10*60 && GetTime() - pnode->nLastSendEmpty > 10*60)
                {
                    printf("socket not sending (10min timeout)\n");
                    pnode->fDisconnect = true;
                }
                else if (GetTime() - pnode->nLastRecv > 10*60)
                {
                    printf("socket inactivity timeout (10min)\n");
                    pnode->fDisconnect = true;
                }
            }
        }
        {
            LOCK(cs_vNodes);
            for (CNode* pnode : vNodesCopy)
                pnode->Release();
        }

        if (fShutdown)
            return;
        MilliSleep(IsInitialBlockDownload() ? 1 : 10);
    }
}









#ifdef USE_UPNP
void ThreadMapPort(void* parg)
{
    // Make this thread recognisable as the UPnP thread
    RenameThread("Triangles-UPnP");

    try
    {
        vnThreadsRunning[THREAD_UPNP]++;
        ThreadMapPort2(parg);
        vnThreadsRunning[THREAD_UPNP]--;
    }
    catch (std::exception& e) {
        vnThreadsRunning[THREAD_UPNP]--;
        PrintException(&e, "ThreadMapPort()");
    } catch (...) {
        vnThreadsRunning[THREAD_UPNP]--;
        PrintException(nullptr, "ThreadMapPort()");
    }
    printf("ThreadMapPort exited\n");
}

void ThreadMapPort2(void* parg)
{
    printf("ThreadMapPort started\n");

    std::string port = strprintf("%u", GetListenPort());
    const char * multicastif = 0;
    const char * minissdpdpath = 0;
    struct UPNPDev * devlist = 0;
    char lanaddr[64];

#ifndef UPNPDISCOVER_SUCCESS
    /* miniupnpc 1.5 */
    devlist = upnpDiscover(2000, multicastif, minissdpdpath, 0);
#else
    /* miniupnpc 1.6+ */
    int error = 0;
    devlist = upnpDiscover(2000, multicastif, minissdpdpath, 0, 0, 2, &error);
#endif

    struct UPNPUrls urls;
    struct IGDdatas data;
    int r;

    char wanaddr[64] = "";
#if MINIUPNPC_API_VERSION >= 18
    r = UPNP_GetValidIGD(devlist, &urls, &data, lanaddr, sizeof(lanaddr), wanaddr, sizeof(wanaddr));
#else
    r = UPNP_GetValidIGD(devlist, &urls, &data, lanaddr, sizeof(lanaddr));
#endif
    if (r == 1)
    {
        //if (fDiscover) {
            char externalIPAddress[40];
            r = UPNP_GetExternalIPAddress(urls.controlURL, data.first.servicetype, externalIPAddress);
            if(r != UPNPCOMMAND_SUCCESS)
                printf("UPnP: GetExternalIPAddress() returned %d\n", r);
            else
            {
                if(externalIPAddress[0])
                {
                    printf("UPnP: ExternalIPAddress = %s\n", externalIPAddress);
                    AddLocal(CNetAddr(externalIPAddress), LOCAL_UPNP);
                }
                else
                    printf("UPnP: GetExternalIPAddress failed.\n");
            }
        //}

        string strDesc = "Triangles " + FormatFullVersion();
#ifndef UPNPDISCOVER_SUCCESS
        /* miniupnpc 1.5 */
        r = UPNP_AddPortMapping(urls.controlURL, data.first.servicetype,
                            port.c_str(), port.c_str(), lanaddr, strDesc.c_str(), "TCP", 0);
#else
        /* miniupnpc 1.6 */
        r = UPNP_AddPortMapping(urls.controlURL, data.first.servicetype,
                            port.c_str(), port.c_str(), lanaddr, strDesc.c_str(), "TCP", 0, "0");
#endif

        if(r!=UPNPCOMMAND_SUCCESS)
            printf("AddPortMapping(%s, %s, %s) failed with code %d (%s)\n",
                port.c_str(), port.c_str(), lanaddr, r, strupnperror(r));
        else
            printf("UPnP Port Mapping successful.\n");
        int i = 1;
        while (true)
        {
            if (fShutdown || !fUseUPnP)
            {
                r = UPNP_DeletePortMapping(urls.controlURL, data.first.servicetype, port.c_str(), "TCP", 0);
                printf("UPNP_DeletePortMapping() returned : %d\n", r);
                freeUPNPDevlist(devlist); devlist = 0;
                FreeUPNPUrls(&urls);
                return;
            }
            if (i % 600 == 0) // Refresh every 20 minutes
            {
#ifndef UPNPDISCOVER_SUCCESS
                /* miniupnpc 1.5 */
                r = UPNP_AddPortMapping(urls.controlURL, data.first.servicetype,
                                    port.c_str(), port.c_str(), lanaddr, strDesc.c_str(), "TCP", 0);
#else
                /* miniupnpc 1.6 */
                r = UPNP_AddPortMapping(urls.controlURL, data.first.servicetype,
                                    port.c_str(), port.c_str(), lanaddr, strDesc.c_str(), "TCP", 0, "0");
#endif

                if(r!=UPNPCOMMAND_SUCCESS)
                    printf("AddPortMapping(%s, %s, %s) failed with code %d (%s)\n",
                        port.c_str(), port.c_str(), lanaddr, r, strupnperror(r));
                else
                    printf("UPnP Port Mapping successful.\n");;
            }
            MilliSleep(2000);
            i++;
        }
    } else {
        printf("No valid UPnP IGDs found\n");
        freeUPNPDevlist(devlist); devlist = 0;
        if (r != 0)
            FreeUPNPUrls(&urls);
        while (true)
        {
            if (fShutdown || !fUseUPnP)
                return;
            MilliSleep(2000);
        }
    }
    printf("ThreadMapPort2 exited\n");
}

void MapPort()
{
    printf("MapPort()...\n");
    if (fUseUPnP && vnThreadsRunning[THREAD_UPNP] < 1)
    {
        if (!NewThread(ThreadMapPort, nullptr))
            printf("Error: ThreadMapPort(ThreadMapPort) failed\n");
    }
}
#else
void MapPort()
{
    // Intentionally left blank.
}
#endif





void ThreadOnionSeed(void* parg)






































{



    // Make this thread recognisable as the tor thread
    RenameThread("Triangles-onionseed");














    // Load hardcoded .onion seeds and queue them for immediate direct connection
    static const char *(*strOnionSeed)[1] = fTestNet ? strTestNetOnionSeed : strMainNetOnionSeed;
    int found = 0;

    // Defense-in-depth (2026-06-22): Validate every hardcoded seed against the
    // v3 onion checksum BEFORE we hand it to Tor. The btb6/gtb6 incident
    // (4,842 "No more HSDir" errors over a 12h from-zero sync test) was caused
    // by a single-character corruption that Tor rejected with a cryptic
    // "ed25519 validation failed" warning. Catching it here gives the operator
    // a clear, actionable error at startup with no wasted network/CPU.
    // See references/onion-corruption-ci-defense.md (CI Layers 2-3) for the
    // static-analysis side of this defense.
    {
        int nInvalid = 0;
        int nTotal = 0;
        std::string strFirstBad;
        for (unsigned int si = 0; strOnionSeed[si][0] != nullptr; si++) {
            nTotal++;
            if (!CTorV3Service::ValidateOnionAddress(strOnionSeed[si][0])) {
                if (strFirstBad.empty()) strFirstBad = strOnionSeed[si][0];
                nInvalid++;
            }
        }
        if (nInvalid > 0) {
            std::string strErr = strprintf(
                "ThreadOnionSeed() : %d of %d hardcoded .onion seed(s) failed v3 "
                "checksum validation. First bad address: %s. "
                "This is the btb6/gtb6 class of bug (see references/onion-corruption-ci-defense.md). "
                "Fix src/onionseed.h before starting the daemon — Tor would "
                "have wasted hours producing cryptic 'ed25519 validation failed' "
                "warnings otherwise.",
                nInvalid, nTotal, strFirstBad.c_str());
            printf("ERROR: %s\n", strErr.c_str());
            throw runtime_error(strErr);
        }
    }

    for (unsigned int seed_idx = 0; strOnionSeed[seed_idx][0] != nullptr; seed_idx++) {
        CNetAddr parsed;
        if (!parsed.SetSpecial(strOnionSeed[seed_idx][0]))
            throw runtime_error("ThreadOnionSeed() : invalid .onion seed");

        int nOneDay = 24*3600;
        CAddress addr = CAddress(CService(parsed, GetDefaultPort()));
        addr.nTime = GetTime() - 3*nOneDay - GetRand(4*nOneDay);
        addrman.Add(addr, parsed);

        // Queue for immediate direct connection (OneShot) — don't wait for
        // addrman selection which deprioritizes stale timestamps
        std::string oneShotAddr = std::string(strOnionSeed[seed_idx][0])
                                + ":" + std::to_string(GetDefaultPort());
        AddOneShot(oneShotAddr);
        found++;
    }

    printf("%d addresses from hardcoded .onion seeds (queued as OneShot)\n", found);

    // Load hardcoded I2P (.b32.i2p) seeds for cross-network peer discovery.
    // These are added to the address manager so that I2P-connected peers can
    // be discovered. Unlike onion seeds, we don't queue them as OneShot
    // connections here — they're connected via the normal outbound connector
    // through the I2P SOCKS proxy.
    {
        static const char *(*strI2PSeed)[1] = fTestNet ? strTestNetI2PSeed : strMainNetI2PSeed;
        int i2pFound = 0;
        for (unsigned int si = 0; strI2PSeed[si][0] != nullptr; si++) {
            CNetAddr parsed;
            if (!parsed.SetSpecial(strI2PSeed[si][0])) {
                printf("WARNING: ThreadOnionSeed() : invalid .b32.i2p seed: %s\n",
                       strI2PSeed[si][0]);
                continue;
            }
            int nOneDay = 24*3600;
            CAddress addr = CAddress(CService(parsed, GetDefaultPort()));
            addr.nTime = GetTime() - 3*nOneDay - GetRand(4*nOneDay);
            addrman.Add(addr, parsed);
            i2pFound++;
        }
        if (i2pFound > 0)
            printf("%d addresses from hardcoded .b32.i2p seeds added to addrman\n", i2pFound);
    }

    // Wait for Tor to establish circuits before attempting HTTPS seed fetch.
    // The hardcoded OneShot connections can race ahead meanwhile.
    printf("ThreadOnionSeed: waiting 20s for Tor circuits before HTTPS seed fetch...\n");
    for (int i = 0; i < 20 && !fShutdown; i++)
        MilliSleep(1000);

    // Fetch dynamic seeds with retry — up to 4 attempts with increasing backoff.
    // This is the primary discovery mechanism — seeds.cryptographic-triangles.org
    {
        bool ok = false;
        int delays[] = {0, 30, 60, 120};
        for (int attempt = 0; attempt < 4 && !ok && !fShutdown; attempt++) {
            if (attempt > 0) {
                printf("ThreadOnionSeed: HTTPS seed fetch retry %d in %ds...\n", attempt, delays[attempt]);
                for (int i = 0; i < delays[attempt] && !fShutdown; i++)
                    MilliSleep(1000);
            }
            if (!fShutdown)
                ok = ThreadHTTPSeedFetch2(nullptr);
        }
        if (!ok && !fShutdown)
            printf("ThreadOnionSeed: all HTTPS seed fetch attempts failed\n");
    }

    printf("ThreadOnionSeed: initial seeding complete\n");

    // Periodic re-seeding for isolated or under-connected nodes.
    // EMERGENCY MODE: When 0 outbound peers, check every 15 seconds
    // NORMAL MODE: Check every 2 minutes, re-seed when < 2 outbound peers
    int64_t nLastReseed = GetTime();
    bool bFirstReseed = true;
    while (!fShutdown) {
        // Count outbound peers to determine check interval
        int nOutbound = 0;
        {
            LOCK(cs_vNodes);
            for (CNode* pnode : vNodes)
                if (!pnode->fInbound)
                    nOutbound++;
        }

        // Emergency mode: 0 peers = check every 15 seconds
        // Low mode: 1 peer = check every 30 seconds
        // Normal: 2+ peers = check every 2 minutes
        int nSleepSeconds = (nOutbound == 0) ? 15 : (nOutbound < 2) ? 30 : 120;
        for (int i = 0; i < nSleepSeconds && !fShutdown; i++)
            MilliSleep(1000);

        if (fShutdown) break;

        // Recount after sleep
        nOutbound = 0;
        {
            LOCK(cs_vNodes);
            for (CNode* pnode : vNodes)
                if (!pnode->fInbound)
                    nOutbound++;
        }

        // Emergency (0 peers): no cooldown, reseed immediately
        // Low (1 peer): 60 second cooldown
        // Normal (<2): 5 min first, 15 min subsequent
        int64_t nCooldown;
        if (nOutbound == 0)
            nCooldown = 0;  // immediate
        else if (nOutbound < 2)
            nCooldown = bFirstReseed ? 60 : 5 * 60;
        else
            nCooldown = bFirstReseed ? 5 * 60 : 15 * 60;

        if (nOutbound < 2 && GetTime() - nLastReseed > nCooldown) {
            if (nOutbound == 0)
                printf("ThreadOnionSeed: EMERGENCY - 0 outbound peers, re-seeding immediately!\n");
            else
                printf("ThreadOnionSeed: low outbound peers (%d), re-seeding...\n", nOutbound);

            ThreadHTTPSeedFetch2(nullptr);

            // Re-queue hardcoded seeds for direct connection
            for (unsigned int seed_idx = 0; strOnionSeed[seed_idx][0] != nullptr; seed_idx++) {
                std::string oneShotAddr = std::string(strOnionSeed[seed_idx][0])
                                        + ":" + std::to_string(GetDefaultPort());
                AddOneShot(oneShotAddr);
            }

            nLastReseed = GetTime();
            bFirstReseed = false;
        }
    }
}












// Hardcoded seeds removed - peer discovery is now fully dynamic via HTTP seed list.
// See: seeds.cryptographic-triangles.org
unsigned int pnSeed[] = {
};

void DumpAddresses()
{
    int64_t nStart = GetTimeMillis();

    CAddrDB adb;
    adb.Write(addrman);

    printf("Flushed %d addresses to peers.dat  %"PRId64"ms\n",
           addrman.size(), GetTimeMillis() - nStart);
}

void ThreadDumpAddress2(void* parg)
{
    vnThreadsRunning[THREAD_DUMPADDRESS]++;
    while (!fShutdown)
    {
        DumpAddresses();
        vnThreadsRunning[THREAD_DUMPADDRESS]--;
        MilliSleep(600000);
        vnThreadsRunning[THREAD_DUMPADDRESS]++;
    }
    vnThreadsRunning[THREAD_DUMPADDRESS]--;
}

void ThreadDumpAddress(void* parg)
{
    // Make this thread recognisable as the address dumping thread
    RenameThread("Triangles-adrdump");

    try
    {
        ThreadDumpAddress2(parg);
    }
    catch (std::exception& e) {
        PrintException(&e, "ThreadDumpAddress()");
    }
    printf("ThreadDumpAddress exited\n");
}

bool ThreadHTTPSeedFetch2(void* parg)
{
    static const char* DEFAULT_SEED_URL_HOST = "seeds.cryptographic-triangles.org";
    static const char* DEFAULT_SEED_URL_PATH = "/seeds.txt";
    static const int HTTPS_PORT = 443;

    std::string seedHost = GetArg("-seedurl", DEFAULT_SEED_URL_HOST);
    std::string seedPath = DEFAULT_SEED_URL_PATH;

    // Allow full URL override: -seedurl=myhost.com/path/seeds.txt
    size_t slashPos = seedHost.find('/');
    if (slashPos != std::string::npos) {
        seedPath = seedHost.substr(slashPos);
        seedHost = seedHost.substr(0, slashPos);
    }

    printf("Fetching seed list from https://%s%s (via Tor)...\n", seedHost.c_str(), seedPath.c_str());

    SSL_CTX* ctx = nullptr;
    SSL* ssl = nullptr;
    SOCKET hSocket = INVALID_SOCKET;

    try {
        // Connect through Tor SOCKS proxy using existing proxy-aware socket infrastructure
        CService addrResolved;
        std::string connectDest = seedHost + ":" + std::to_string(HTTPS_PORT);

        if (!ConnectSocketByName(addrResolved, hSocket, connectDest.c_str(), HTTPS_PORT, nConnectTimeout)) {
            printf("HTTPS seed fetch: cannot connect to %s through Tor proxy\n", seedHost.c_str());
            return false;
        }

        // Set up TLS over the connected socket
        ctx = SSL_CTX_new(TLS_client_method());
        if (!ctx) {
            printf("HTTPS seed fetch: SSL_CTX_new failed\n");
            closesocket(hSocket);
            return false;
        }

        // Use system default CA certificates for verification
        SSL_CTX_set_default_verify_paths(ctx);
        SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, nullptr);

        ssl = SSL_new(ctx);
        if (!ssl) {
            printf("HTTPS seed fetch: SSL_new failed\n");
            SSL_CTX_free(ctx);
            closesocket(hSocket);
            return false;
        }

        // Set SNI hostname (required for Caddy/Let's Encrypt)
        SSL_set_tlsext_host_name(ssl, seedHost.c_str());
        SSL_set_fd(ssl, (int)hSocket);

        int ret = SSL_connect(ssl);
        if (ret != 1) {
            int sslErr = SSL_get_error(ssl, ret);
            unsigned long errCode = ERR_get_error();
            char errBuf[256];
            ERR_error_string_n(errCode, errBuf, sizeof(errBuf));
            printf("HTTPS seed fetch: TLS handshake failed (ssl_err=%d): %s\n", sslErr, errBuf);
            SSL_free(ssl);
            SSL_CTX_free(ctx);
            closesocket(hSocket);
            return false;
        }

        printf("HTTPS seed fetch: TLS connection established to %s\n", seedHost.c_str());

        // Send HTTP request over TLS
        std::string request =
            "GET " + seedPath + " HTTP/1.1\r\n"
            "Host: " + seedHost + "\r\n"
            "Connection: close\r\n"
            "User-Agent: Triangles\r\n"
            "\r\n";

        int nSent = 0;
        int nLen = request.size();
        while (nSent < nLen) {
            int nBytes = SSL_write(ssl, request.c_str() + nSent, nLen - nSent);
            if (nBytes <= 0) {
                printf("HTTPS seed fetch: SSL_write failed\n");
                SSL_shutdown(ssl);
                SSL_free(ssl);
                SSL_CTX_free(ctx);
                closesocket(hSocket);
                return false;
            }
            nSent += nBytes;
        }

        // Read response over TLS
        std::string response;
        char buf[4096];
        while (true) {
            int nBytes = SSL_read(ssl, buf, sizeof(buf));
            if (nBytes <= 0)
                break;
            response.append(buf, nBytes);
        }

        SSL_shutdown(ssl);
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        closesocket(hSocket);
        ssl = nullptr;
        ctx = nullptr;
        hSocket = INVALID_SOCKET;

        if (response.empty()) {
            printf("HTTPS seed fetch: empty response from %s\n", seedHost.c_str());
            return false;
        }

        // Parse HTTP response - find end of headers
        size_t headerEnd = response.find("\r\n\r\n");
        if (headerEnd == std::string::npos) {
            printf("HTTPS seed fetch: malformed response (no header terminator)\n");
            return false;
        }

        // Check status code
        std::string statusLine = response.substr(0, response.find("\r\n"));
        if (statusLine.find("200") == std::string::npos) {
            printf("HTTPS seed fetch: %s from %s\n", statusLine.c_str(), seedHost.c_str());
            return false;
        }

        std::string headers = response.substr(0, headerEnd);
        std::string body = response.substr(headerEnd + 4);

        // Some servers (e.g. Caddy / Let's Encrypt fronting the seed list) reply
        // with Transfer-Encoding: chunked even on HTTP/1.1 + Connection: close. The
        // body then carries hex chunk-size lines interleaved with the data; parsing
        // it raw fuses a chunk marker onto an address and we lose most of the list
        // (the classic "only 1 address" symptom). De-chunk first when present.
        //
        // v5.9.22 hardening: the parser is now strict and reports a distinct
        // failure code for each kind of malformed framing. See DechunkResult in
        // netbase.h and the unit tests in src/test/http_seed_tests.cpp.
        {
            std::string h = headers;
            for (char& c : h) c = (char)tolower((unsigned char)c);
            if (h.find("transfer-encoding:") != std::string::npos &&
                h.find("chunked") != std::string::npos)
            {
                std::string decoded;
                int rc = DechunkTransferEncoding(body, decoded);
                if (rc != DECHUNK_OK) {
                    const char* reason = "unknown";
                    switch (rc) {
                        case DECHUNK_EMPTY:               reason = "empty body"; break;
                        case DECHUNK_NO_CHUNK_TERMINATOR: reason = "missing chunk terminator (CRLF)"; break;
                        case DECHUNK_INVALID_HEX:         reason = "malformed chunk-size (not valid hex)"; break;
                        case DECHUNK_OVERSIZE_CHUNK:      reason = "chunk size exceeds remaining input (truncated)"; break;
                        case DECHUNK_MISSING_DATA_CRLF:   reason = "missing CRLF after chunk data"; break;
                        default:                          reason = "unknown"; break;
                    }
                    printf("HTTPS seed fetch: malformed chunked transfer encoding (%s) from %s\n",
                           reason, seedHost.c_str());
                    return false;
                }
                body.swap(decoded);
            }
        }

        if (fDebug)
            printf("HTTPS seed fetch: %d body bytes to parse\n", (int)body.size());

        // Tolerant parse: accept one-per-line OR several addresses on one line
        // (whitespace / comma / semicolon separated), and ignore inline '#' comments.
        // v5.9.22: the splitting logic is now a pure function in netbase.cpp so
        // we can unit-test every line format. The CNetAddr/CService/addrman
        // validation stays here because it touches globals.
        int found = 0;
        int skipped = 0;

        auto addSeed = [&](std::string addrStr) -> void {
            while (!addrStr.empty() && (addrStr.back()=='\r' || addrStr.back()==' ' || addrStr.back()=='\t'))
                addrStr.pop_back();
            while (!addrStr.empty() && (addrStr.front()==' ' || addrStr.front()=='\t'))
                addrStr.erase(addrStr.begin());
            if (addrStr.empty())
                return;

            int port = GetDefaultPort();
            size_t onionPos = addrStr.find(".onion:");
            size_t i2pPos = addrStr.find(".i2p:");
            if (onionPos != std::string::npos) {
                port = atoi(addrStr.substr(onionPos + 7).c_str());
                addrStr = addrStr.substr(0, onionPos + 6); // keep ".onion"
            } else if (i2pPos != std::string::npos) {
                port = atoi(addrStr.substr(i2pPos + 5).c_str());
                // keep the ".i2p" suffix
            } else if (addrStr.find(".onion") == std::string::npos &&
                       addrStr.find(".i2p") == std::string::npos) {
                return; // Tor/I2P-native: skip clearnet addresses
            }
            if (port <= 0 || port > 65535)
                port = GetDefaultPort();

            CService service(addrStr, port);
            if (service.IsValid()) {
                CAddress addr(service);
                addr.nTime = GetTime() - 3*24*60*60; // 3 days ago
                addrman.Add(addr, service);
                printf("HTTPS seed: added %s:%d\n", addrStr.c_str(), port);
                found++;
            } else {
                skipped++;
            }
        };

        // Use the pure helper to split the body. If it returns nothing, that
        // means the body was entirely comments / blank lines / whitespace —
        // distinct failure mode worth logging separately from "no valid
        // addresses after parsing".
        std::vector<std::string> tokens = ParseSeedListBody(body);
        if (tokens.empty()) {
            printf("HTTPS seed fetch: parsed response contained zero valid addresses from %s\n", seedHost.c_str());
            return false;
        }

        for (const std::string& tok : tokens)
        {
            if (fShutdown)
                return false;
            addSeed(tok);
        }

        printf("%d addresses found from HTTPS seed list (%s)\n", found, seedHost.c_str());
        if (found == 0) {
            printf("HTTPS seed fetch: parsed response contained zero valid addresses from %s\n", seedHost.c_str());
            return false;
        }
        return true;

    } catch (std::exception& e) {
        printf("HTTPS seed fetch failed: %s\n", e.what());
        if (ssl) { SSL_shutdown(ssl); SSL_free(ssl); }
        if (ctx) SSL_CTX_free(ctx);
        if (hSocket != INVALID_SOCKET) closesocket(hSocket);
        return false;
    }
}

void ThreadHTTPSeedFetch(void* parg)
{
    RenameThread("Triangles-httpseed");
    try
    {
        vnThreadsRunning[THREAD_HTTPSEED]++;
        ThreadHTTPSeedFetch2(parg);
        vnThreadsRunning[THREAD_HTTPSEED]--;
    }
    catch (std::exception& e) {
        vnThreadsRunning[THREAD_HTTPSEED]--;
        PrintException(&e, "ThreadHTTPSeedFetch()");
    } catch (...) {
        vnThreadsRunning[THREAD_HTTPSEED]--;
        PrintException(nullptr, "ThreadHTTPSeedFetch()");
    }
    printf("ThreadHTTPSeedFetch exited\n");
}

void ThreadOpenConnections(void* parg)
{
    // Make this thread recognisable as the connection opening thread
    RenameThread("Triangles-opencon");

    try
    {
        vnThreadsRunning[THREAD_OPENCONNECTIONS]++;
        ThreadOpenConnections2(parg);
        vnThreadsRunning[THREAD_OPENCONNECTIONS]--;
    }
    catch (std::exception& e) {
        vnThreadsRunning[THREAD_OPENCONNECTIONS]--;
        PrintException(&e, "ThreadOpenConnections()");
    } catch (...) {
        vnThreadsRunning[THREAD_OPENCONNECTIONS]--;
        PrintException(nullptr, "ThreadOpenConnections()");
    }
    printf("ThreadOpenConnections exited\n");
}

void static ProcessOneShot()
{
    string strDest;
    {
        LOCK(cs_vOneShots);
        if (vOneShots.empty())
            return;
        strDest = vOneShots.front();
        vOneShots.pop_front();
    }
    CAddress addr;
    CSemaphoreGrant grant(*semOutbound, true);
    if (grant) {
        if (!OpenNetworkConnection(addr, &grant, strDest.c_str(), true))
            AddOneShot(strDest);
    }
}

// triangles: stake miner thread
void static ThreadStakeMiner(void* parg)
{
    printf("ThreadStakeMiner started\n");
    CWallet* pwallet = (CWallet*)parg;
    int nConsecutiveErrors = 0;
    while (!fShutdown)
    {
        try
        {
            vnThreadsRunning[THREAD_STAKE_MINER]++;
            StakeMiner(pwallet);
            vnThreadsRunning[THREAD_STAKE_MINER]--;
            break; // normal exit
        }
        catch (std::exception& e) {
            vnThreadsRunning[THREAD_STAKE_MINER]--;
            nConsecutiveErrors++;
            printf("ThreadStakeMiner() exception: %s (attempt %d)\n", e.what(), nConsecutiveErrors);
            if (nConsecutiveErrors >= 10) {
                printf("ThreadStakeMiner() too many consecutive errors, giving up\n");
                break;
            }
            MilliSleep(5000); // wait 5 seconds before retrying
        } catch (...) {
            vnThreadsRunning[THREAD_STAKE_MINER]--;
            nConsecutiveErrors++;
            printf("ThreadStakeMiner() unknown exception (attempt %d)\n", nConsecutiveErrors);
            if (nConsecutiveErrors >= 10) {
                printf("ThreadStakeMiner() too many consecutive errors, giving up\n");
                break;
            }
            MilliSleep(5000);
        }
    }
    printf("ThreadStakeMiner exiting, %d threads remaining\n", vnThreadsRunning[THREAD_STAKE_MINER]);
}

void ThreadOpenConnections2(void* parg)
{
    printf("ThreadOpenConnections started\n");

    // Connect to specific addresses
    if (mapArgs.count("-connect") && mapMultiArgs["-connect"].size() > 0)
    {
        for (int64_t nLoop = 0;; nLoop++)
        {
            ProcessOneShot();
            for (string strAddr : mapMultiArgs["-connect"])
            {
                CAddress addr;
                OpenNetworkConnection(addr, nullptr, strAddr.c_str());
                for (int i = 0; i < 10 && i < nLoop; i++)
                {
                    MilliSleep(500);
                    if (fShutdown)
                        return;
                }
            }
            MilliSleep(500);
        }
    }

    // Initiate network connections
    int64_t nStart = GetTime();
    int64_t nLastDiscoveryRound = 0;  // signed peer discovery: re-trigger getaddr+getseederlist+getwalletaddr
    const int64_t DISCOVERY_COOLDOWN = 300; // 5min between rounds (peer count < threshold)
    const int DISCOVERY_THRESHOLD = 4;       // if we have fewer than this many connected peers, re-trigger
    while (true)
    {
        ProcessOneShot();

        // Signed peer discovery: when our connected-peer count drops, re-trigger
        // the full signing + discovery round on every peer. Triangles already has
        // getaddr / getseederlist / getwalletaddr in onion_v3.cpp — this just
        // re-fires them periodically instead of only at startup.
        int nConnectedOnion = 0;
        int nSignedPeers = 0;
        int64_t nNow = GetTime();
        {
            LOCK(cs_vNodes);
            for (CNode* pnode : vNodes) {
                if (!pnode->fInbound && pnode->fSuccessfullyConnected) {
                    std::string ip = pnode->addr.ToStringIP();
                    if (ip.find(".onion") != std::string::npos) {
                        nConnectedOnion++;
                        if (pnode->nSignedPeerBonus > 0) nSignedPeers++;
                    }
                }
            }
        }
        if (nConnectedOnion < DISCOVERY_THRESHOLD &&
            nNow - nLastDiscoveryRound > DISCOVERY_COOLDOWN)
        {
            nLastDiscoveryRound = nNow;
            printf("SYNC-SIGN: low peer count (%d < %d), re-firing discovery round on all peers\n",
                   nConnectedOnion, DISCOVERY_THRESHOLD);
            LOCK(cs_vNodes);
            for (CNode* pnode : vNodes) {
                if (!pnode->fInbound && pnode->fSuccessfullyConnected) {
                    std::string ip = pnode->addr.ToStringIP();
                    if (ip.find(".onion") != std::string::npos &&
                        nNow - pnode->nLastGetaddrTrigger > DISCOVERY_COOLDOWN)
                    {
                        pnode->nLastGetaddrTrigger = nNow;
                        pnode->PushMessage("getaddr");
                        pnode->PushMessage("getseederlist");
                        // getwalletaddr is only sent on version handshake (main.cpp:3941);
                        // we don't re-fire it here because it generates a new receiving
                        // key on the peer each call, which is wasteful. Signed peers
                        // are cached for 24h (onion_v3.cpp:2308) so they'll be reused.
                    }
                }
            }
        }

        vnThreadsRunning[THREAD_OPENCONNECTIONS]--;
        MilliSleep(500);
        vnThreadsRunning[THREAD_OPENCONNECTIONS]++;
        if (fShutdown)
            return;


        vnThreadsRunning[THREAD_OPENCONNECTIONS]--;
        CSemaphoreGrant grant(*semOutbound);
        vnThreadsRunning[THREAD_OPENCONNECTIONS]++;
        if (fShutdown)
            return;

        // Hardcoded seed fallback removed - peer discovery is now fully dynamic
        // via HTTP seed list from seeds.cryptographic-triangles.org

        //
        // Choose an address to connect to based on most recently seen
        //
        CAddress addrConnect;

        // Only connect out to one peer per network group (/16 for IPv4).
        // Do this here so we don't have to critsect vNodes inside mapAddresses critsect.
        int nOutbound = 0;
        set<vector<unsigned char> > setConnected;
        {
            LOCK(cs_vNodes);
            for (CNode* pnode : vNodes) {
                if (!pnode->fInbound) {
                    setConnected.insert(pnode->addr.GetGroup());
                    nOutbound++;
                }
            }
        }

        int64_t nANow = GetAdjustedTime();

        int nTries = 0;
        while (true)
        {
            // use an nUnkBias between 10 (no outgoing connections) and 90 (8 outgoing connections)
            CAddress addr = addrman.Select(10 + min(nOutbound,8)*10);

            // if we selected an invalid address, restart
            if (!addr.IsValid() || setConnected.count(addr.GetGroup()) || IsLocal(addr))
                break;

            // If we didn't find an appropriate destination after trying 100 addresses fetched from addrman,
            // stop this loop, and let the outer loop run again (which sleeps, adds seed nodes, recalculates
            // already-connected network ranges, ...) before trying new addrman addresses.
            nTries++;
            if (nTries > 100)
                break;

            if (IsLimited(addr))
                continue;

            // only consider very recently tried nodes after 30 failed attempts
            if (nANow - addr.nLastTry < 120 && nTries < 30)
                continue;

            // do not allow non-default ports, unless after 50 invalid addresses selected already
            if (addr.GetPort() != GetDefaultPort() && nTries < 50)
                continue;

            addrConnect = addr;
            break;
        }

        if (addrConnect.IsValid())
            OpenNetworkConnection(addrConnect, &grant);
    }
}

void ThreadOpenAddedConnections(void* parg)
{
    // Make this thread recognisable as the connection opening thread
    RenameThread("Triangles-opencon");

    try
    {
        vnThreadsRunning[THREAD_ADDEDCONNECTIONS]++;
        ThreadOpenAddedConnections2(parg);
        vnThreadsRunning[THREAD_ADDEDCONNECTIONS]--;
    }
    catch (std::exception& e) {
        vnThreadsRunning[THREAD_ADDEDCONNECTIONS]--;
        PrintException(&e, "ThreadOpenAddedConnections()");
    } catch (...) {
        vnThreadsRunning[THREAD_ADDEDCONNECTIONS]--;
        PrintException(nullptr, "ThreadOpenAddedConnections()");
    }
    printf("ThreadOpenAddedConnections exited\n");
}

void ThreadOpenAddedConnections2(void* parg)
{
    printf("ThreadOpenAddedConnections started\n");

    if (mapArgs.count("-addnode") == 0)
        return;

    if (HaveNameProxy()) {
        while(!fShutdown) {
            for (string& strAddNode : mapMultiArgs["-addnode"]) {
                CAddress addr;
                CSemaphoreGrant grant(*semOutbound);
                OpenNetworkConnection(addr, &grant, strAddNode.c_str());
                MilliSleep(500);
            }
            vnThreadsRunning[THREAD_ADDEDCONNECTIONS]--;
            MilliSleep(120000); // Retry every 2 minutes
            vnThreadsRunning[THREAD_ADDEDCONNECTIONS]++;
        }
        return;
    }

    vector<vector<CService> > vservAddressesToAdd(0);
    for (string& strAddNode : mapMultiArgs["-addnode"])
    {
        vector<CService> vservNode(0);
        if(Lookup(strAddNode.c_str(), vservNode, GetDefaultPort(), fNameLookup, 0))
        {
            vservAddressesToAdd.push_back(vservNode);
            {
                LOCK(cs_setservAddNodeAddresses);
                for (CService& serv : vservNode)
                    setservAddNodeAddresses.insert(serv);
            }
        }
    }
    while (true)
    {
        vector<vector<CService> > vservConnectAddresses = vservAddressesToAdd;
        // Attempt to connect to each IP for each addnode entry until at least one is successful per addnode entry
        // (keeping in mind that addnode entries can have many IPs if fNameLookup)
        {
            LOCK(cs_vNodes);
            for (CNode* pnode : vNodes)
                for (vector<vector<CService> >::iterator it = vservConnectAddresses.begin(); it != vservConnectAddresses.end(); it++)
                    for (CService& addrNode : *(it))
                        if (pnode->addr == addrNode)
                        {
                            it = vservConnectAddresses.erase(it);
                            it--;
                            break;
                        }
        }
        for (vector<CService>& vserv : vservConnectAddresses)
        {
            CSemaphoreGrant grant(*semOutbound);
            OpenNetworkConnection(CAddress(*(vserv.begin())), &grant);
            MilliSleep(500);
            if (fShutdown)
                return;
        }
        if (fShutdown)
            return;
        vnThreadsRunning[THREAD_ADDEDCONNECTIONS]--;
        MilliSleep(120000); // Retry every 2 minutes
        vnThreadsRunning[THREAD_ADDEDCONNECTIONS]++;
        if (fShutdown)
            return;
    }
    printf("ThreadOpenAddedConnections exited\n");
}

// if successful, this moves the passed grant to the constructed node
bool OpenNetworkConnection(const CAddress& addrConnect, CSemaphoreGrant *grantOutbound, const char *strDest, bool fOneShot)
{
    //
    // Initiate outbound network connection
    //
    if (fShutdown)
        return false;
    if (!strDest)
        if (IsLocal(addrConnect) ||
            FindNode((CNetAddr)addrConnect) || CNode::IsBanned(addrConnect) ||
            FindNode(addrConnect.ToStringIPPort().c_str()))
            return false;
    if (strDest && FindNode(strDest))
        return false;

    vnThreadsRunning[THREAD_OPENCONNECTIONS]--;
    CNode* pnode = ConnectNode(addrConnect, strDest);
    vnThreadsRunning[THREAD_OPENCONNECTIONS]++;
    if (fShutdown)
        return false;
    if (!pnode)
        return false;
    if (grantOutbound)
        grantOutbound->MoveTo(pnode->grantOutbound);
    pnode->fNetworkNode = true;
    if (fOneShot)
        pnode->fOneShot = true;

    return true;
}








void ThreadMessageHandler(void* parg)
{
    // Make this thread recognisable as the message handling thread
    RenameThread("Triangles-msghand");

    try
    {
        vnThreadsRunning[THREAD_MESSAGEHANDLER]++;
        ThreadMessageHandler2(parg);
        vnThreadsRunning[THREAD_MESSAGEHANDLER]--;
    }
    catch (std::exception& e) {
        vnThreadsRunning[THREAD_MESSAGEHANDLER]--;
        PrintException(&e, "ThreadMessageHandler()");
    } catch (...) {
        vnThreadsRunning[THREAD_MESSAGEHANDLER]--;
        PrintException(nullptr, "ThreadMessageHandler()");
    }
    printf("ThreadMessageHandler exited\n");
}

void ThreadMessageHandler2(void* parg)
{
    printf("ThreadMessageHandler started\n");
    SetThreadPriority(THREAD_PRIORITY_BELOW_NORMAL);
    bool fWasBoosted = false;
    while (!fShutdown)
    {
        vector<CNode*> vNodesCopy;
        {
            LOCK(cs_vNodes);
            vNodesCopy = vNodes;
            for (CNode* pnode : vNodesCopy)
                pnode->AddRef();
        }

        // Poll the connected nodes for messages
        CNode* pnodeTrickle = nullptr;
        if (!vNodesCopy.empty())
            pnodeTrickle = vNodesCopy[GetRand(vNodesCopy.size())];
        for (CNode* pnode : vNodesCopy)
        {
            if (fShutdown)
                break;

            // Receive messages
            {
                TRY_LOCK(pnode->cs_vRecvMsg, lockRecv);
                if (lockRecv)
                    if (!ProcessMessages(pnode))
                        pnode->CloseSocketDisconnect();
            }

            // Send messages
            {
                TRY_LOCK(pnode->cs_vSend, lockSend);
                if (lockSend)
                    SendMessages(pnode, pnode == pnodeTrickle);
            }
        }

        {
            LOCK(cs_vNodes);
            for (CNode* pnode : vNodesCopy)
                pnode->Release();
        }

        // Boost thread priority during IBD, restore when caught up
        if (IsInitialBlockDownload() && !fWasBoosted) {
            SetThreadPriority(THREAD_PRIORITY_NORMAL);
            fWasBoosted = true;
        } else if (!IsInitialBlockDownload() && fWasBoosted) {
            SetThreadPriority(THREAD_PRIORITY_BELOW_NORMAL);
            fWasBoosted = false;
        }

        // Wait and allow messages to bunch up.
        // During IBD, use a shorter sleep to maximize block processing throughput.
        // Reduce vnThreadsRunning so StopNode has permission to exit while
        // we're sleeping, but we must always check fShutdown after doing this.
        vnThreadsRunning[THREAD_MESSAGEHANDLER]--;
        MilliSleep(IsInitialBlockDownload() ? 1 : 100);
        if (fRequestShutdown)
            StartShutdown();
        vnThreadsRunning[THREAD_MESSAGEHANDLER]++;
        if (fShutdown)
            return;
    }
    printf("ThreadMessageHandler exited\n");
}






bool BindListenPort(const CService &addrBind, string& strError)
{
    strError = "";
    int nOne = 1;

#ifdef WIN32
    // Initialize Windows Sockets
    WSADATA wsadata;
    int ret = WSAStartup(MAKEWORD(2,2), &wsadata);
    if (ret != NO_ERROR)
    {
        strError = strprintf("Error: TCP/IP socket library failed to start (WSAStartup returned error %d)", ret);
        printf("%s\n", strError.c_str());
        return false;
    }
#endif

    // Create socket for listening for incoming connections
#ifdef USE_IPV6
    struct sockaddr_storage sockaddr;
#else
    struct sockaddr sockaddr;
#endif
    socklen_t len = sizeof(sockaddr);
    if (!addrBind.GetSockAddr((struct sockaddr*)&sockaddr, &len))
    {
        strError = strprintf("Error: bind address family for %s not supported", addrBind.ToString().c_str());
        printf("%s\n", strError.c_str());
        return false;
    }

    SOCKET hListenSocket = socket(((struct sockaddr*)&sockaddr)->sa_family, SOCK_STREAM, IPPROTO_TCP);
    if (hListenSocket == INVALID_SOCKET)
    {
        strError = strprintf("Error: Couldn't open socket for incoming connections (socket returned error %d)", WSAGetLastError());
        printf("%s\n", strError.c_str());
        return false;
    }

#ifdef SO_NOSIGPIPE
    // Different way of disabling SIGPIPE on BSD
    setsockopt(hListenSocket, SOL_SOCKET, SO_NOSIGPIPE, (void*)&nOne, sizeof(int));
#endif

#ifndef WIN32
    // Allow binding if the port is still in TIME_WAIT state after
    // the program was closed and restarted.  Not an issue on windows.
    setsockopt(hListenSocket, SOL_SOCKET, SO_REUSEADDR, (void*)&nOne, sizeof(int));
#endif


#ifdef WIN32
    // Set to non-blocking, incoming connections will also inherit this
    if (ioctlsocket(hListenSocket, FIONBIO, (u_long*)&nOne) == SOCKET_ERROR)
#else
    if (fcntl(hListenSocket, F_SETFL, O_NONBLOCK) == SOCKET_ERROR)
#endif
    {
        strError = strprintf("Error: Couldn't set properties on socket for incoming connections (error %d)", WSAGetLastError());
        printf("%s\n", strError.c_str());
        return false;
    }

#ifdef USE_IPV6
    // some systems don't have IPV6_V6ONLY but are always v6only; others do have the option
    // and enable it by default or not. Try to enable it, if possible.
    if (addrBind.IsIPv6()) {
#ifdef IPV6_V6ONLY
#ifdef WIN32
        setsockopt(hListenSocket, IPPROTO_IPV6, IPV6_V6ONLY, (const char*)&nOne, sizeof(int));
#else
        setsockopt(hListenSocket, IPPROTO_IPV6, IPV6_V6ONLY, (void*)&nOne, sizeof(int));
#endif
#endif
#ifdef WIN32
        int nProtLevel = 10 /* PROTECTION_LEVEL_UNRESTRICTED */;
        int nParameterId = 23 /* IPV6_PROTECTION_LEVEl */;
        // this call is allowed to fail
        setsockopt(hListenSocket, IPPROTO_IPV6, nParameterId, (const char*)&nProtLevel, sizeof(int));
#endif
    }
#endif

    if (::bind(hListenSocket, (struct sockaddr*)&sockaddr, len) == SOCKET_ERROR)
    {
        int nErr = WSAGetLastError();
        if (nErr == WSAEADDRINUSE)
            strError = strprintf(_("Unable to bind to %s on this computer. Triangles is probably already running."), addrBind.ToString().c_str());
        else
            strError = strprintf(_("Unable to bind to %s on this computer (bind returned error %d, %s)"), addrBind.ToString().c_str(), nErr, strerror(nErr));
        printf("%s\n", strError.c_str());
        return false;
    }
    printf("Bound to %s\n", addrBind.ToString().c_str());

    // Listen for incoming connections
    if (listen(hListenSocket, SOMAXCONN) == SOCKET_ERROR)
    {
        strError = strprintf("Error: Listening for incoming connections failed (listen returned error %d)", WSAGetLastError());
        printf("%s\n", strError.c_str());
        return false;
    }

    vhListenSocket.push_back(hListenSocket);

    //if (addrBind.IsRoutable() && fDiscover)
    //    AddLocal(addrBind, LOCAL_BIND);

    return true;
}

void static Discover()













{
   // no network discovery






}

static void run_tor() {
    // Tor process is now managed by CTorProcess (tor_process.cpp)
    // which starts an external Tor binary with SOCKS5 proxy and v3 hidden service.
    // The old embedded Tor v2 code was removed (incompatible with OpenSSL 3.x).
    printf("Tor v3 mode: using managed Tor process via SOCKS5 proxy.\n");
    triangles_tor_set_initialized();
}


void StartTor(void* parg)









{
    // Make this thread recognisable as the tor thread
    RenameThread("Triangles-onion");
    printf("Onion thread started.");
    try



    {
      run_tor();





    }
    catch (std::exception& e) {
      PrintException(&e, "StartTor()");
    }



    printf("Onion thread exited.");


}

void StartNode(void* parg)
{
    // Make this thread recognisable as the startup thread
    RenameThread("Triangles-start");

    // Configurable outbound connections via -maxoutboundconnections (default 8, range 4-32)
    MAX_OUTBOUND_CONNECTIONS = GetArg("-maxoutboundconnections", 8);
    if (MAX_OUTBOUND_CONNECTIONS < 4)  MAX_OUTBOUND_CONNECTIONS = 4;
    if (MAX_OUTBOUND_CONNECTIONS > 32) MAX_OUTBOUND_CONNECTIONS = 32;
    printf("Configured max outbound connections: %d (from -maxoutboundconnections)\n", MAX_OUTBOUND_CONNECTIONS);

    // If a canonical UTXO snapshot file is already present at startup,
    // advertise NODE_SNAPSHOT to peers BEFORE the first outbound connection.
    // EnsureLocalSnapshot() also sets this flag post-IBD, but at that point
    // already-connected peers have already cached our version message and
    // won't re-read our service bits — so for the "place canonical file in
    // datadir before launch" operator workflow this pre-handshake OR is the
    // load-bearing one.
    if (!fClient) {
        SnapshotNet::EnsureLocalSnapshot();
    }

    if (semOutbound == nullptr) {
        // initialize semaphore — use -maxoutboundconnections (set above), fall back to -maxoutbound
        int nMaxOutbound = (int)GetArg("-maxoutbound", MAX_OUTBOUND_CONNECTIONS);
        nMaxOutbound = min(nMaxOutbound, (int)GetArg("-maxconnections", 125));
        nMaxOutbound = max(nMaxOutbound, 1);  // at least 1 outbound
        printf("Max outbound connections: %d\n", nMaxOutbound);
        semOutbound = new CSemaphore(nMaxOutbound);
    }

    if (pnodeLocalHost == nullptr)
        pnodeLocalHost = new CNode(INVALID_SOCKET, CAddress(CService("127.0.0.1", 0), nLocalServices));

    printf("StartNode(): pnodeLocalHost addr: %s\n",
           pnodeLocalHost->addr.ToString().c_str());

    Discover();

    //
    // Start threads
    //

    // start the onion seeder
    if (!GetBoolArg("-onionseed", true))
        printf(".onion seeding disabled\n");
    else
        if (!NewThread(ThreadOnionSeed, nullptr))
              printf("Error: NewThread(ThreadOnionSeed) failed\n");

    // Map ports with UPnP (default)
    if (fUseUPnP)
        MapPort();

    // HTTP seed list fetch — only as a standalone thread if onion seeding is disabled,
    // since ThreadOnionSeed already calls ThreadHTTPSeedFetch2 internally.
    if (GetBoolArg("-onionseed", true))
        printf("HTTP seed fetch handled by onion seed thread\n");
    else if (GetBoolArg("-noseedurl", false))
        printf("HTTP seed fetch disabled\n");
    else if (!NewThread(ThreadHTTPSeedFetch, nullptr))
        printf("Error: NewThread(ThreadHTTPSeedFetch) failed\n");

    // Send and receive from sockets, accept connections
    if (!NewThread(ThreadSocketHandler, nullptr))
        printf("Error: NewThread(ThreadSocketHandler) failed\n");

    // Initiate outbound connections from -addnode
    if (!NewThread(ThreadOpenAddedConnections, nullptr))
        printf("Error: NewThread(ThreadOpenAddedConnections) failed\n");

    // Initiate outbound connections
    if (!NewThread(ThreadOpenConnections, nullptr))
        printf("Error: NewThread(ThreadOpenConnections) failed\n");

    // Start fork detector (post-IBD background monitor)
    if (!NewThread(ThreadForkDetector, nullptr))
        printf("Error: NewThread(ThreadForkDetector) failed\n");

    // Process messages
    if (!NewThread(ThreadMessageHandler, nullptr))
        printf("Error: NewThread(ThreadMessageHandler) failed\n");

    // Dump network addresses
    if (!NewThread(ThreadDumpAddress, nullptr))
        printf("Error; NewThread(ThreadDumpAddress) failed\n");

    // Mine proof-of-stake blocks in the background
    if (!GetBoolArg("-stake", true))
        printf("Staking disabled at startup (stake=0).\n");
        else
        if (!NewThread(ThreadStakeMiner, pwalletMain.get()))
            printf("Error: NewThread(ThreadStakeMiner) failed\n");
}

bool StopNode()
{
    printf("StopNode()\n");
    fShutdown = true;
    nTransactionsUpdated++;
    int64_t nStart = GetTime();
    if (semOutbound) {
        int nMaxOutbound = (int)GetArg("-maxoutbound", MAX_OUTBOUND_CONNECTIONS);
        nMaxOutbound = min(nMaxOutbound, (int)GetArg("-maxconnections", 125));
        nMaxOutbound = max(nMaxOutbound, 1);
        for (int i=0; i<nMaxOutbound; i++)
            semOutbound->post();
    }
    do
    {
        int nThreadsRunning = 0;
        for (int n = 0; n < THREAD_MAX; n++)
            nThreadsRunning += vnThreadsRunning[n];
        if (nThreadsRunning == 0)
            break;
        if (GetTime() - nStart > 20)
            break;
        MilliSleep(20);
    } while(true);
    if (vnThreadsRunning[THREAD_SOCKETHANDLER] > 0) printf("ThreadSocketHandler still running\n");
    if (vnThreadsRunning[THREAD_OPENCONNECTIONS] > 0) printf("ThreadOpenConnections still running\n");
    if (vnThreadsRunning[THREAD_MESSAGEHANDLER] > 0) printf("ThreadMessageHandler still running\n");
    if (vnThreadsRunning[THREAD_RPCLISTENER] > 0) printf("ThreadRPCListener still running\n");
    if (vnThreadsRunning[THREAD_RPCHANDLER] > 0) printf("ThreadsRPCServer still running\n");
#ifdef USE_UPNP
    if (vnThreadsRunning[THREAD_UPNP] > 0) printf("ThreadMapPort still running\n");
#endif
    if (vnThreadsRunning[THREAD_HTTPSEED] > 0) printf("ThreadHTTPSeedFetch still running\n");
    if (vnThreadsRunning[THREAD_ADDEDCONNECTIONS] > 0) printf("ThreadOpenAddedConnections still running\n");
    if (vnThreadsRunning[THREAD_DUMPADDRESS] > 0) printf("ThreadDumpAddresses still running\n");
    if (vnThreadsRunning[THREAD_STAKE_MINER] > 0) printf("ThreadStakeMiner still running\n");
    {
        int64_t nWaitStart = GetTime();
        while (vnThreadsRunning[THREAD_MESSAGEHANDLER] > 0 || vnThreadsRunning[THREAD_RPCHANDLER] > 0)
        {
            if (GetTime() - nWaitStart > 10)
            {
                printf("Timed out waiting for message/RPC threads to stop\n");
                break;
            }
            MilliSleep(20);
        }
    }
    MilliSleep(50);
    DumpAddresses();

    // Force-disconnect and clean up all remaining nodes now that threads have stopped.
    // Close sockets first so any lingering I/O fails immediately.
    {
        LOCK(cs_vNodes);
        for (CNode* pnode : vNodes)
        {
            pnode->CloseSocketDisconnect();
            pnode->Cleanup();
        }
    }

    return true;
}

class CNetCleanup
{
public:
    CNetCleanup()
    {
    }
    ~CNetCleanup()
    {
        // Close sockets - acquire lock in case other threads are still winding down
        LOCK(cs_vNodes);
        for (CNode* pnode : vNodes)
            if (pnode->hSocket != INVALID_SOCKET)
                closesocket(pnode->hSocket);
        for (SOCKET hListenSocket : vhListenSocket)
            if (hListenSocket != INVALID_SOCKET)
                if (closesocket(hListenSocket) == SOCKET_ERROR)
                    printf("closesocket(hListenSocket) failed with error %d\n", WSAGetLastError());

#ifdef WIN32
        // Shutdown Windows Sockets
        WSACleanup();
#endif
    }
}
instance_of_cnetcleanup;

void RelayTransaction(const CTransaction& tx, const uint256& hash)
{
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss.reserve(10000);
    ss << tx;
    RelayTransaction(tx, hash, ss);
}

void RelayTransaction(const CTransaction& tx, const uint256& hash, const CDataStream& ss)
{
    CInv inv(MSG_TX, hash);
    {
        LOCK(cs_mapRelay);
        // Expire old relay messages
        while (!vRelayExpiration.empty() && vRelayExpiration.front().first < GetTime())
        {
            mapRelay.erase(vRelayExpiration.front().second);
            vRelayExpiration.pop_front();
        }

        // Save original serialized message so newer versions are preserved
        mapRelay.insert({inv, ss});
        vRelayExpiration.push_back({GetTime() + 15 * 60, inv});
    }

    RelayInventory(inv);
}

// ---------------------------------------------------------------------------
// BIP152 Compact Block relay — net-layer integration
// ---------------------------------------------------------------------------

/** Advertise a new block to all connected peers.
 *
 *  For peers that have negotiated compact block relay (fSendCmpct), the
 *  inventory is sent as MSG_CMPCT_BLOCK so they know to request the compact
 *  form.  For legacy peers, standard MSG_BLOCK inventory is sent.
 *
 *  The actual compact block construction and sending happens in main.cpp
 *  (SendCompactBlock / ProcessCompactBlock).  This function only handles
 *  the inventory advertisement at the net layer.
 */
void RelayBlockInventory(const uint256& hash)
{
    LOCK(cs_vNodes);
    for (CNode* pnode : vNodes)
    {
        // Use MSG_CMPCT_BLOCK for peers that support compact relay,
        // MSG_BLOCK for legacy peers.
        int nType = pnode->fSendCmpct ? MSG_CMPCT_BLOCK : MSG_BLOCK;
        pnode->PushInventory(CInv(nType, hash));
    }
}
