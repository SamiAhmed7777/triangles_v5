// Copyright (c) 2009-2012 Bitcoin Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <algorithm>
#include "net.h"
#include "addrman.h"
#include "trianglesrpc.h"
#include "wallet.h"
#include "db.h"
#include "walletdb.h"
#include "net_bootstrap.h"
#include "i2p.h"
#include "tor/onion_v3.h"
#include "tor/tor_embedded.h"

using namespace json_spirit;
using namespace std;

Value getnetworkinfo(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getnetworkinfo\n"
            "Returns an object containing various state info regarding P2P networking,\n"
            "including peer mix, bootstrap mode, and basic sync health.");

    proxyType proxy;
    GetProxy(NET_IPV4, proxy);
    const NetBootstrap::NetworkHealth health = NetBootstrap::GetNetworkHealth();

    Object healthObj;
    healthObj.push_back(Pair("connectedpeers", health.connectedPeers));
    healthObj.push_back(Pair("torpeers", health.torPeers));
    healthObj.push_back(Pair("bootstrapped", health.isBootstrapped));
    healthObj.push_back(Pair("syncing", health.isSyncing));
    healthObj.push_back(Pair("lastblocktime", static_cast<int64_t>(health.lastBlockTime)));
    healthObj.push_back(Pair("networkmode", "tor_native"));

    // Tor .onion address (wallet hidden service).
    std::string onionAddress = CTorV3Manager::GetInstance()->GetWalletOnionAddress();
    if (onionAddress.empty())
        onionAddress = CTorEmbedded::GetInstance()->GetOnionAddress();

    // I2P session state and .b32.i2p address.
    CI2PSession* i2p = CI2PSession::GetInstance();
    int nI2PPeers = 0;
    {
        LOCK(cs_vNodes);
        for (CNode* pnode : vNodes)
            if (pnode->addr.IsI2P())
                nI2PPeers++;
    }
    Object i2pObj;
    i2pObj.push_back(Pair("enabled", i2p->IsEnabled()));
    i2pObj.push_back(Pair("active",  i2p->IsActive()));
    i2pObj.push_back(Pair("address", i2p->GetB32Address()));
    i2pObj.push_back(Pair("peers",   nI2PPeers));

    Object obj;
    obj.push_back(Pair("version",         FormatFullVersion()));
    obj.push_back(Pair("protocolversion", (int)PROTOCOL_VERSION));
    obj.push_back(Pair("connections",     (int)vNodes.size()));
    obj.push_back(Pair("proxy",           (proxy.first.IsValid() ? proxy.first.ToStringIPPort() : string())));
    obj.push_back(Pair("ip",             addrSeenByPeer.ToStringIP()));
    obj.push_back(Pair("toraddress",     onionAddress));
    obj.push_back(Pair("i2p",            i2pObj));
    obj.push_back(Pair("localservices",  strprintf("%016"PRIx64, nLocalServices)));
    obj.push_back(Pair("testnet",        fTestNet));
    obj.push_back(Pair("networkhealth",  healthObj));
    obj.push_back(Pair("errors",         GetWarnings("statusbar")));
    return obj;
}

Value getconnectioncount(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getconnectioncount\n"
            "Returns the number of connections to other nodes.");

    LOCK(cs_vNodes);
    return (int)vNodes.size();
}

static void CopyNodeStats(std::vector<CNodeStats>& vstats)
{
    vstats.clear();

    LOCK(cs_vNodes);
    vstats.reserve(vNodes.size());
    for (CNode* pnode : vNodes) {
        CNodeStats stats;
        pnode->copyStats(stats);
        vstats.push_back(stats);
    }
}

Value getpeerinfo(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getpeerinfo\n"
            "Returns data about each connected network node.");

    vector<CNodeStats> vstats;
    CopyNodeStats(vstats);

    Array ret;

    for (const CNodeStats& stats : vstats) {
        Object obj;

        obj.push_back(Pair("addr", stats.addrName));
        obj.push_back(Pair("services", strprintf("%08"PRIx64, stats.nServices)));
        obj.push_back(Pair("lastsend", (int64_t)stats.nLastSend));
        obj.push_back(Pair("lastrecv", (int64_t)stats.nLastRecv));
        obj.push_back(Pair("conntime", (int64_t)stats.nTimeConnected));
        obj.push_back(Pair("version", stats.nVersion));
        obj.push_back(Pair("subver", stats.strSubVer));
        obj.push_back(Pair("inbound", stats.fInbound));
        obj.push_back(Pair("startingheight", stats.nStartingHeight));
        obj.push_back(Pair("banscore", stats.nMisbehavior));
        obj.push_back(Pair("pingtime", stats.nPingUsecTime > 0 ? (double)stats.nPingUsecTime / 1000000.0 : -1.0));
        obj.push_back(Pair("blocksdelivered", stats.nBlocksDelivered));
        obj.push_back(Pair("avglatency", stats.nAvgBlockLatencyUs > 0 ? (double)stats.nAvgBlockLatencyUs / 1000.0 : -1.0));

        ret.push_back(obj);
    }

    return ret;
}


Value addnode(const Array& params, bool fHelp)
{
    string strCommand;
    if (params.size() == 2)
        strCommand = params[1].get_str();
    if (fHelp || params.size() != 2 ||
        (strCommand != "onetry" && strCommand != "add" && strCommand != "remove"))
        throw runtime_error(
            "addnode <node> <add|remove|onetry>\n"
            "Attempts to add or remove a node from the addnode list,\n"
            "or try a connection to a node once.\n"
            "<node> must be a .onion address (Tor-native network).");

    string strNode = params[0].get_str();

    // Tor-native: require .onion addresses
    if (strNode.find(".onion") == string::npos)
        throw runtime_error("Only .onion addresses are supported on this network.");

    if (strCommand == "onetry")
    {
        CAddress addr;
        CNode* pnode = ConnectNode(addr, strNode.c_str());
        if (!pnode)
            throw runtime_error("Failed to connect to node (may already be connected or unreachable).");
        pnode->Release();
        return Value::null;
    }

    // For add/remove, manipulate the -addnode list that ThreadOpenAddedConnections uses
    LOCK(cs_vNodes);
    vector<string>& vAddedNodes = mapMultiArgs["-addnode"];

    if (strCommand == "add")
    {
        for (const string& existing : vAddedNodes)
            if (existing == strNode)
                throw runtime_error("Node already added.");
        vAddedNodes.push_back(strNode);
    }
    else if (strCommand == "remove")
    {
        auto it = std::find(vAddedNodes.begin(), vAddedNodes.end(), strNode);
        if (it == vAddedNodes.end())
            throw runtime_error("Node not found in addnode list.");
        vAddedNodes.erase(it);
    }

    return Value::null;
}

Value disconnectnode(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "disconnectnode <node>\n"
            "Immediately disconnects from the specified node.");

    string strNode = params[0].get_str();

    LOCK(cs_vNodes);
    for (CNode* pnode : vNodes) {
        if (pnode->addrName == strNode || pnode->addr.ToString() == strNode) {
            pnode->CloseSocketDisconnect();
            return Value::null;
        }
    }
    throw runtime_error("Node not found.");
}

Value getseedlist(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getseedlist\n"
            "Returns known .onion peer addresses from the address manager.\n"
            "Used by the seed collector to build the dynamic seed list.");

    vector<CAddress> vAddr = addrman.GetAddr();
    Array ret;

    for (const CAddress& addr : vAddr) {
        if (!addr.IsTor())
            continue;

        Object obj;
        obj.push_back(Pair("address", addr.ToStringIP()));
        obj.push_back(Pair("port", (int)addr.GetPort()));
        obj.push_back(Pair("lastseen", (int64_t)addr.nTime));
        ret.push_back(obj);
    }

    return ret;
}

Value getnetworkstability(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getnetworkstability\n"
            "Returns detailed network stability metrics including peer quality,\n"
            "connection health, and isolation risk assessment.");

    int nOutbound = 0, nInbound = 0, nTotal = 0;
    int64_t nBestPing = INT64_MAX, nWorstPing = 0, nTotalPing = 0;
    int nPingCount = 0;
    int nTotalBlocksDelivered = 0;
    int64_t nOldestConnection = 0;
    int64_t nNewestConnection = INT64_MAX;

    {
        LOCK(cs_vNodes);
        nTotal = vNodes.size();
        for (CNode* pnode : vNodes) {
            if (pnode->fInbound)
                nInbound++;
            else
                nOutbound++;

            if (pnode->nPingUsecTime > 0) {
                nTotalPing += pnode->nPingUsecTime;
                nPingCount++;
                if (pnode->nPingUsecTime < nBestPing)
                    nBestPing = pnode->nPingUsecTime;
                if (pnode->nPingUsecTime > nWorstPing)
                    nWorstPing = pnode->nPingUsecTime;
            }

            nTotalBlocksDelivered += pnode->nBlocksDelivered;

            int64_t uptime = GetTime() - pnode->nTimeConnected;
            if (uptime > nOldestConnection)
                nOldestConnection = uptime;
            if (uptime < nNewestConnection)
                nNewestConnection = uptime;
        }
    }

    // Determine isolation risk
    string strRisk;
    if (nOutbound == 0 && nInbound == 0)
        strRisk = "critical";
    else if (nOutbound == 0)
        strRisk = "high";
    else if (nOutbound == 1)
        strRisk = "elevated";
    else if (nOutbound < 3)
        strRisk = "moderate";
    else
        strRisk = "low";

    Object obj;
    obj.push_back(Pair("connections_total", nTotal));
    obj.push_back(Pair("connections_outbound", nOutbound));
    obj.push_back(Pair("connections_inbound", nInbound));
    obj.push_back(Pair("isolation_risk", strRisk));
    obj.push_back(Pair("blocks_delivered_total", nTotalBlocksDelivered));
    obj.push_back(Pair("known_addresses", (int)addrman.size()));

    Object pingObj;
    pingObj.push_back(Pair("best_ms", nPingCount > 0 ? (double)nBestPing / 1000.0 : -1.0));
    pingObj.push_back(Pair("worst_ms", nPingCount > 0 ? (double)nWorstPing / 1000.0 : -1.0));
    pingObj.push_back(Pair("avg_ms", nPingCount > 0 ? (double)nTotalPing / nPingCount / 1000.0 : -1.0));
    pingObj.push_back(Pair("peers_measured", nPingCount));
    obj.push_back(Pair("ping", pingObj));

    Object uptimeObj;
    uptimeObj.push_back(Pair("newest_sec", nTotal > 0 ? (int64_t)nNewestConnection : 0));
    uptimeObj.push_back(Pair("oldest_sec", nTotal > 0 ? (int64_t)nOldestConnection : 0));
    obj.push_back(Pair("connection_uptime", uptimeObj));

    obj.push_back(Pair("seconds_since_last_block", (int64_t)(GetTime() - nTimeBestReceived)));
    obj.push_back(Pair("current_height", nBestHeight));

    return obj;
}
