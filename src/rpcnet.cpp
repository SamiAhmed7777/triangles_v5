// Copyright (c) 2009-2012 Bitcoin Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <algorithm>
#include "net.h"
#include "addrman.h"
#include "trianglesrpc.h"
#include "alert.h"
#include "wallet.h"
#include "db.h"
#include "walletdb.h"
#include "net_bootstrap.h"

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
    healthObj.push_back(Pair("lastblocktime", static_cast<boost::int64_t>(health.lastBlockTime)));
    healthObj.push_back(Pair("networkmode", "tor_native"));

    Object obj;
    obj.push_back(Pair("version",         FormatFullVersion()));
    obj.push_back(Pair("protocolversion", (int)PROTOCOL_VERSION));
    obj.push_back(Pair("connections",     (int)vNodes.size()));
    obj.push_back(Pair("proxy",           (proxy.first.IsValid() ? proxy.first.ToStringIPPort() : string())));
    obj.push_back(Pair("ip",             addrSeenByPeer.ToStringIP()));
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
        obj.push_back(Pair("lastsend", (boost::int64_t)stats.nLastSend));
        obj.push_back(Pair("lastrecv", (boost::int64_t)stats.nLastRecv));
        obj.push_back(Pair("conntime", (boost::int64_t)stats.nTimeConnected));
        obj.push_back(Pair("version", stats.nVersion));
        obj.push_back(Pair("subver", stats.strSubVer));
        obj.push_back(Pair("inbound", stats.fInbound));
        obj.push_back(Pair("startingheight", stats.nStartingHeight));
        obj.push_back(Pair("banscore", stats.nMisbehavior));

        ret.push_back(obj);
    }

    return ret;
}

extern CCriticalSection cs_mapAlerts;
extern map<uint256, CAlert> mapAlerts;
 
// triangles: send alert.  
// There is a known deadlock situation with ThreadMessageHandler
// ThreadMessageHandler: holds cs_vSend and acquiring cs_main in SendMessages()
// ThreadRPCServer: holds cs_main and acquiring cs_vSend in alert.RelayTo()/PushMessage()/BeginMessage()
Value sendalert(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 6)
        throw runtime_error(
            "sendalert <message> <privatekey> <minver> <maxver> <priority> <id> [cancelupto]\n"
            "<message> is the alert text message\n"
            "<privatekey> is hex string of alert master private key\n"
            "<minver> is the minimum applicable internal client version\n"
            "<maxver> is the maximum applicable internal client version\n"
            "<priority> is integer priority number\n"
            "<id> is the alert id\n"
            "[cancelupto] cancels all alert id's up to this number\n"
            "Returns true or false.");

    CAlert alert;
    CKey key;

    alert.strStatusBar = params[0].get_str();
    alert.nMinVer = params[2].get_int();
    alert.nMaxVer = params[3].get_int();
    alert.nPriority = params[4].get_int();
    alert.nID = params[5].get_int();
    if (params.size() > 6)
        alert.nCancel = params[6].get_int();
    alert.nVersion = PROTOCOL_VERSION;
    alert.nRelayUntil = GetAdjustedTime() + 365*24*60*60;
    alert.nExpiration = GetAdjustedTime() + 365*24*60*60;

    CDataStream sMsg(SER_NETWORK, PROTOCOL_VERSION);
    sMsg << (CUnsignedAlert)alert;
    alert.vchMsg = vector<unsigned char>(sMsg.begin(), sMsg.end());

    vector<unsigned char> vchPrivKey = ParseHex(params[1].get_str());
    key.SetPrivKey(CPrivKey(vchPrivKey.begin(), vchPrivKey.end())); // if key is not correct openssl may crash
    if (!key.Sign(Hash(alert.vchMsg.begin(), alert.vchMsg.end()), alert.vchSig))
        throw runtime_error(
            "Unable to sign alert, check private key?\n");  
    if(!alert.ProcessAlert()) 
        throw runtime_error(
            "Failed to process alert.\n");
    // Relay alert
    {
        LOCK(cs_vNodes);
        for (CNode* pnode : vNodes)
            alert.RelayTo(pnode);
    }

    Object result;
    result.push_back(Pair("strStatusBar", alert.strStatusBar));
    result.push_back(Pair("nVersion", alert.nVersion));
    result.push_back(Pair("nMinVer", alert.nMinVer));
    result.push_back(Pair("nMaxVer", alert.nMaxVer));
    result.push_back(Pair("nPriority", alert.nPriority));
    result.push_back(Pair("nID", alert.nID));
    if (alert.nCancel > 0)
        result.push_back(Pair("nCancel", alert.nCancel));
    return result;
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
        obj.push_back(Pair("lastseen", (boost::int64_t)addr.nTime));
        ret.push_back(obj);
    }

    return ret;
}
