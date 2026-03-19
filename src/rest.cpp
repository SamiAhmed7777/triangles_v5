// Copyright (c) 2024 The Triangles developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "rest.h"
#include "trianglesrpc.h"
#include "main.h"
#include "sync.h"
#include "util.h"
#include "base58.h"
#include "addressindex.h"
#include "wallet.h"
#include "init.h"

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

using namespace std;
using namespace json_spirit;

// Forward declarations from rpcblockchain.cpp
extern Object blockToJSON(const CBlock& block, const CBlockIndex* blockindex, bool fPrintTransactionDetail);
extern void TxToJSON(const CTransaction& tx, const uint256 hashBlock, json_spirit::Object& entry);

// Forward declaration from trianglesrpc.cpp
extern bool HTTPAuthorized(map<string, string>& mapHeaders);
extern string rfc1123Time();

// ============================================================================
// Rate limiter
// ============================================================================

struct IPRateLimiter
{
    int64_t nTokens;
    int64_t nLastRefill;

    IPRateLimiter() : nTokens(0), nLastRefill(0) {}

    bool Allow(int64_t nMaxTokens, int64_t nRefillRate)
    {
        int64_t nNow = GetTime();
        if (nLastRefill == 0) {
            nLastRefill = nNow;
            nTokens = nMaxTokens;
        }
        // Refill tokens
        int64_t nElapsed = nNow - nLastRefill;
        if (nElapsed > 0) {
            nTokens += nElapsed * nRefillRate;
            if (nTokens > nMaxTokens)
                nTokens = nMaxTokens;
            nLastRefill = nNow;
        }
        if (nTokens > 0) {
            nTokens--;
            return true;
        }
        return false;
    }
};

static CCriticalSection cs_rateLimiter;
static map<string, IPRateLimiter> mapRateLimiters;
static int64_t nLastCleanup = 0;

bool CheckRESTRateLimit(const string& strIP)
{
    int64_t nLimit = GetArg("-restratelimit", 30);
    int64_t nBurst = nLimit * 2; // burst = 2x sustained rate

    if (nLimit <= 0)
        return true; // rate limiting disabled

    LOCK(cs_rateLimiter);

    // Periodic cleanup of stale entries (every 60s)
    int64_t nNow = GetTime();
    if (nNow - nLastCleanup > 60) {
        map<string, IPRateLimiter>::iterator it = mapRateLimiters.begin();
        while (it != mapRateLimiters.end()) {
            if (nNow - it->second.nLastRefill > 300) // 5 min stale
                mapRateLimiters.erase(it++);
            else
                ++it;
        }
        nLastCleanup = nNow;
    }

    return mapRateLimiters[strIP].Allow(nBurst, nLimit);
}

// ============================================================================
// HTTP response helpers
// ============================================================================

string HTTPReplyREST(int nStatus, const string& strMsg, const string& contentType)
{
    string strCorsOrigin = GetArg("-restcorsorigin", "*");

    const char *cStatus;
         if (nStatus == 200) cStatus = "OK";
    else if (nStatus == 204) cStatus = "No Content";
    else if (nStatus == 400) cStatus = "Bad Request";
    else if (nStatus == 401) cStatus = "Unauthorized";
    else if (nStatus == 403) cStatus = "Forbidden";
    else if (nStatus == 404) cStatus = "Not Found";
    else if (nStatus == 429) cStatus = "Too Many Requests";
    else if (nStatus == 500) cStatus = "Internal Server Error";
    else if (nStatus == 503) cStatus = "Service Unavailable";
    else cStatus = "";

    return strprintf(
            "HTTP/1.1 %d %s\r\n"
            "Date: %s\r\n"
            "Connection: close\r\n"
            "Content-Length: %" PRIszu "\r\n"
            "Content-Type: %s\r\n"
            "Access-Control-Allow-Origin: %s\r\n"
            "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
            "Access-Control-Allow-Headers: Content-Type, Authorization\r\n"
            "Access-Control-Max-Age: 86400\r\n"
            "Server: Triangles/%s\r\n"
            "\r\n"
            "%s",
        nStatus,
        cStatus,
        rfc1123Time().c_str(),
        strMsg.size(),
        contentType.c_str(),
        strCorsOrigin.c_str(),
        FormatFullVersion().c_str(),
        strMsg.c_str());
}

// ============================================================================
// URL parsing
// ============================================================================

static void ParseRESTPath(const string& strURI, vector<string>& parts, map<string, string>& queryParams)
{
    string path = strURI;

    // Split query string
    size_t qpos = path.find('?');
    string queryString;
    if (qpos != string::npos) {
        queryString = path.substr(qpos + 1);
        path = path.substr(0, qpos);
    }

    // Split path into parts
    boost::split(parts, path, boost::is_any_of("/"));

    // Parse query parameters
    if (!queryString.empty()) {
        vector<string> pairs;
        boost::split(pairs, queryString, boost::is_any_of("&"));
        for (size_t i = 0; i < pairs.size(); i++) {
            size_t eq = pairs[i].find('=');
            if (eq != string::npos)
                queryParams[pairs[i].substr(0, eq)] = pairs[i].substr(eq + 1);
        }
    }
}

bool IsRESTPath(const string& strURI)
{
    return strURI.size() >= 6 && strURI.substr(0, 6) == "/rest/";
}

// ============================================================================
// Auth helpers
// ============================================================================

static bool RESTAuthorized(map<string, string>& mapHeaders)
{
    // Check Bearer token first (if -restapikey is set)
    string strApiKey = GetArg("-restapikey", "");
    if (!strApiKey.empty()) {
        string strAuth = mapHeaders.count("authorization") ? mapHeaders["authorization"] : "";
        if (strAuth.substr(0, 7) == "Bearer ") {
            string strToken = strAuth.substr(7);
            boost::trim(strToken);
            if (TimingResistantEqual(strToken, strApiKey))
                return true;
        }
    }

    // Fall back to Basic Auth (same as RPC)
    if (mapHeaders.count("authorization"))
        return HTTPAuthorized(mapHeaders);

    return false;
}

// ============================================================================
// JSON error helper
// ============================================================================

static string RESTError(const string& message, int code = -1)
{
    Object obj;
    obj.push_back(Pair("error", message));
    if (code != -1)
        obj.push_back(Pair("code", code));
    return write_string(Value(obj), false) + "\n";
}

// ============================================================================
// Helper: call an RPC method and return JSON string
// ============================================================================

static bool CallRPCMethod(const string& method, const Array& params,
                          string& strReply, int& nStatus)
{
    try {
        Value result = tableRPC.execute(method, params);
        strReply = write_string(result, false) + "\n";
        nStatus = HTTP_OK;
        return true;
    }
    catch (Object& objError) {
        int code = find_value(objError, "code").get_int();
        string msg = find_value(objError, "message").get_str();
        if (code == RPC_METHOD_NOT_FOUND) nStatus = HTTP_NOT_FOUND;
        else if (code == RPC_WALLET_UNLOCK_NEEDED) nStatus = HTTP_FORBIDDEN;
        else if (code == RPC_INVALID_PARAMETER || code == RPC_INVALID_ADDRESS_OR_KEY) nStatus = HTTP_BAD_REQUEST;
        else nStatus = HTTP_INTERNAL_SERVER_ERROR;
        strReply = RESTError(msg, code);
        return true;
    }
    catch (std::exception& e) {
        nStatus = HTTP_INTERNAL_SERVER_ERROR;
        strReply = RESTError(e.what());
        return true;
    }
}

// ============================================================================
// Public endpoint handlers
// ============================================================================

// GET /rest/chaininfo
static bool HandleChainInfo(string& strReply, int& nStatus)
{
    LOCK(cs_main);
    Object obj, diff;
    obj.push_back(Pair("chain", fTestNet ? string("test") : string("main")));
    obj.push_back(Pair("blocks", (int)nBestHeight));
    obj.push_back(Pair("bestblockhash", hashBestChain.GetHex()));
    diff.push_back(Pair("proof-of-work", GetDifficulty()));
    diff.push_back(Pair("proof-of-stake", GetDifficulty(GetLastBlockIndex(pindexBest, true))));
    obj.push_back(Pair("difficulty", diff));
    obj.push_back(Pair("moneysupply", ValueFromAmount(pindexBest->nMoneySupply)));
    strReply = write_string(Value(obj), false) + "\n";
    nStatus = HTTP_OK;
    return true;
}

// GET /rest/block/{hash_or_param}[.hex]
static bool HandleBlock(const string& param, const string& format, string& strReply, int& nStatus)
{
    LOCK(cs_main);
    uint256 hash(param);
    if (mapBlockIndex.count(hash) == 0) {
        nStatus = HTTP_NOT_FOUND;
        strReply = RESTError("Block not found");
        return true;
    }
    CBlock block;
    CBlockIndex* pblockindex = mapBlockIndex[hash];
    block.ReadFromDisk(pblockindex, true);

    if (format == "hex") {
        CDataStream ssBlock(SER_NETWORK, PROTOCOL_VERSION);
        ssBlock << block;
        strReply = HexStr(ssBlock.begin(), ssBlock.end()) + "\n";
    } else {
        Object obj = blockToJSON(block, pblockindex, false);
        strReply = write_string(Value(obj), false) + "\n";
    }
    nStatus = HTTP_OK;
    return true;
}

// GET /rest/blockheader/{hash}
static bool HandleBlockHeader(const string& param, string& strReply, int& nStatus)
{
    LOCK(cs_main);
    uint256 hash(param);
    if (mapBlockIndex.count(hash) == 0) {
        nStatus = HTTP_NOT_FOUND;
        strReply = RESTError("Block not found");
        return true;
    }
    CBlockIndex* pblockindex = mapBlockIndex[hash];
    Object result;
    result.push_back(Pair("hash", pblockindex->GetBlockHash().GetHex()));
    result.push_back(Pair("confirmations", pindexBest->nHeight - pblockindex->nHeight + 1));
    result.push_back(Pair("height", pblockindex->nHeight));
    result.push_back(Pair("version", pblockindex->nVersion));
    result.push_back(Pair("merkleroot", pblockindex->hashMerkleRoot.GetHex()));
    result.push_back(Pair("time", (boost::int64_t)pblockindex->GetBlockTime()));
    result.push_back(Pair("nonce", (boost::uint64_t)pblockindex->nNonce));
    result.push_back(Pair("bits", HexBits(pblockindex->nBits)));
    result.push_back(Pair("difficulty", GetDifficulty(pblockindex)));
    result.push_back(Pair("flags", strprintf("%s%s",
        pblockindex->IsProofOfStake() ? "proof-of-stake" : "proof-of-work",
        pblockindex->GeneratedStakeModifier() ? " stake-modifier" : "")));
    if (pblockindex->pprev)
        result.push_back(Pair("previousblockhash", pblockindex->pprev->GetBlockHash().GetHex()));
    if (pblockindex->pnext)
        result.push_back(Pair("nextblockhash", pblockindex->pnext->GetBlockHash().GetHex()));
    strReply = write_string(Value(result), false) + "\n";
    nStatus = HTTP_OK;
    return true;
}

// GET /rest/tx/{txid}[.hex]
static bool HandleTx(const string& param, const string& format, string& strReply, int& nStatus)
{
    LOCK(cs_main);
    uint256 hash(param);
    CTransaction tx;
    uint256 hashBlock = 0;
    if (!GetTransaction(hash, tx, hashBlock)) {
        nStatus = HTTP_NOT_FOUND;
        strReply = RESTError("Transaction not found");
        return true;
    }
    if (format == "hex") {
        CDataStream ssTx(SER_NETWORK, PROTOCOL_VERSION);
        ssTx << tx;
        strReply = HexStr(ssTx.begin(), ssTx.end()) + "\n";
    } else {
        Object obj;
        obj.push_back(Pair("txid", tx.GetHash().GetHex()));
        TxToJSON(tx, hashBlock, obj);
        strReply = write_string(Value(obj), false) + "\n";
    }
    nStatus = HTTP_OK;
    return true;
}

// GET /rest/blockhashbyheight/{n}
static bool HandleBlockHashByHeight(const string& param, string& strReply, int& nStatus)
{
    LOCK(cs_main);
    int nHeight = atoi(param.c_str());
    if (nHeight < 0 || nHeight > nBestHeight) {
        nStatus = HTTP_NOT_FOUND;
        strReply = RESTError("Block height out of range");
        return true;
    }
    CBlockIndex* pblockindex = FindBlockByHeight(nHeight);
    Object obj;
    obj.push_back(Pair("blockhash", pblockindex->phashBlock->GetHex()));
    strReply = write_string(Value(obj), false) + "\n";
    nStatus = HTTP_OK;
    return true;
}

// GET /rest/blockbyheight/{n}
static bool HandleBlockByHeight(const string& param, const string& format, string& strReply, int& nStatus)
{
    LOCK(cs_main);
    int nHeight = atoi(param.c_str());
    if (nHeight < 0 || nHeight > nBestHeight) {
        nStatus = HTTP_NOT_FOUND;
        strReply = RESTError("Block height out of range");
        return true;
    }
    CBlockIndex* pblockindex = FindBlockByHeight(nHeight);
    CBlock block;
    block.ReadFromDisk(pblockindex, true);

    if (format == "hex") {
        CDataStream ssBlock(SER_NETWORK, PROTOCOL_VERSION);
        ssBlock << block;
        strReply = HexStr(ssBlock.begin(), ssBlock.end()) + "\n";
    } else {
        Object obj = blockToJSON(block, pblockindex, false);
        strReply = write_string(Value(obj), false) + "\n";
    }
    nStatus = HTTP_OK;
    return true;
}

// GET /rest/mempool
static bool HandleMempool(string& strReply, int& nStatus)
{
    vector<uint256> vtxid;
    mempool.queryHashes(vtxid);
    Array a;
    for (const uint256& hash : vtxid)
        a.push_back(hash.ToString());
    strReply = write_string(Value(a), false) + "\n";
    nStatus = HTTP_OK;
    return true;
}

// GET /rest/difficulty
static bool HandleDifficulty(string& strReply, int& nStatus)
{
    Array params;
    return CallRPCMethod("getdifficulty", params, strReply, nStatus);
}

// GET /rest/supply
static bool HandleSupply(string& strReply, int& nStatus)
{
    Array params;
    return CallRPCMethod("gettxoutsetinfo", params, strReply, nStatus);
}

// GET /rest/staking
static bool HandleStaking(string& strReply, int& nStatus)
{
    Array params;
    return CallRPCMethod("getstakinginfo", params, strReply, nStatus);
}

// GET /rest/mining
static bool HandleMining(string& strReply, int& nStatus)
{
    Array params;
    return CallRPCMethod("getmininginfo", params, strReply, nStatus);
}

// GET /rest/subsidy
static bool HandleSubsidy(string& strReply, int& nStatus)
{
    Array params;
    return CallRPCMethod("getsubsidy", params, strReply, nStatus);
}

// GET /rest/estimatefee
static bool HandleEstimateFee(string& strReply, int& nStatus)
{
    Array params;
    params.push_back(6); // default 6 blocks
    return CallRPCMethod("estimatefee", params, strReply, nStatus);
}

// GET /rest/checkpoint
static bool HandleCheckpoint(string& strReply, int& nStatus)
{
    Array params;
    return CallRPCMethod("getcheckpoint", params, strReply, nStatus);
}

// GET /rest/network
static bool HandleNetwork(string& strReply, int& nStatus)
{
    Array params;
    return CallRPCMethod("getnetworkinfo", params, strReply, nStatus);
}

// GET /rest/peers
static bool HandlePeers(string& strReply, int& nStatus)
{
    Array params;
    return CallRPCMethod("getpeerinfo", params, strReply, nStatus);
}

// GET /rest/validate/{address}
static bool HandleValidate(const string& addr, string& strReply, int& nStatus)
{
    Array params;
    params.push_back(addr);
    return CallRPCMethod("validateaddress", params, strReply, nStatus);
}

// GET /rest/address/{addr}/balance
static bool HandleAddressBalance(const string& addr, string& strReply, int& nStatus)
{
    if (!fAddressIndex) {
        nStatus = 503;
        strReply = RESTError("Address index not enabled. Start daemon with -addressindex=1");
        return true;
    }
    Object addrObj;
    Array addrArray;
    addrArray.push_back(addr);
    addrObj.push_back(Pair("addresses", addrArray));
    Array params;
    params.push_back(addrObj);
    return CallRPCMethod("getaddressbalance", params, strReply, nStatus);
}

// GET /rest/address/{addr}/utxos
static bool HandleAddressUtxos(const string& addr, string& strReply, int& nStatus)
{
    if (!fAddressIndex) {
        nStatus = 503;
        strReply = RESTError("Address index not enabled. Start daemon with -addressindex=1");
        return true;
    }
    Object addrObj;
    Array addrArray;
    addrArray.push_back(addr);
    addrObj.push_back(Pair("addresses", addrArray));
    Array params;
    params.push_back(addrObj);
    return CallRPCMethod("getaddressutxos", params, strReply, nStatus);
}

// GET /rest/address/{addr}/txids[?start=N&end=N]
static bool HandleAddressTxids(const string& addr, const map<string, string>& queryParams,
                               string& strReply, int& nStatus)
{
    if (!fAddressIndex) {
        nStatus = 503;
        strReply = RESTError("Address index not enabled. Start daemon with -addressindex=1");
        return true;
    }
    Object addrObj;
    Array addrArray;
    addrArray.push_back(addr);
    addrObj.push_back(Pair("addresses", addrArray));

    map<string, string>::const_iterator itStart = queryParams.find("start");
    map<string, string>::const_iterator itEnd = queryParams.find("end");
    if (itStart != queryParams.end())
        addrObj.push_back(Pair("start", atoi(itStart->second.c_str())));
    if (itEnd != queryParams.end())
        addrObj.push_back(Pair("end", atoi(itEnd->second.c_str())));

    Array params;
    params.push_back(addrObj);
    return CallRPCMethod("getaddresstxids", params, strReply, nStatus);
}

// POST /rest/tx/decode  body: {"hex":"..."}
static bool HandleTxDecode(const string& strBody, string& strReply, int& nStatus)
{
    Value valBody;
    if (!read_string(strBody, valBody) || valBody.type() != obj_type) {
        nStatus = HTTP_BAD_REQUEST;
        strReply = RESTError("Invalid JSON body. Expected: {\"hex\":\"...\"}");
        return true;
    }
    Object bodyObj = valBody.get_obj();
    Value hexVal = find_value(bodyObj, "hex");
    if (hexVal.type() != str_type) {
        nStatus = HTTP_BAD_REQUEST;
        strReply = RESTError("Missing 'hex' field in request body");
        return true;
    }
    Array params;
    params.push_back(hexVal.get_str());
    return CallRPCMethod("decoderawtransaction", params, strReply, nStatus);
}

// POST /rest/tx/send  body: {"hex":"..."}
static bool HandleTxSend(const string& strBody, string& strReply, int& nStatus)
{
    Value valBody;
    if (!read_string(strBody, valBody) || valBody.type() != obj_type) {
        nStatus = HTTP_BAD_REQUEST;
        strReply = RESTError("Invalid JSON body. Expected: {\"hex\":\"...\"}");
        return true;
    }
    Object bodyObj = valBody.get_obj();
    Value hexVal = find_value(bodyObj, "hex");
    if (hexVal.type() != str_type) {
        nStatus = HTTP_BAD_REQUEST;
        strReply = RESTError("Missing 'hex' field in request body");
        return true;
    }
    Array params;
    params.push_back(hexVal.get_str());
    return CallRPCMethod("sendrawtransaction", params, strReply, nStatus);
}

// ============================================================================
// Wallet endpoint handlers (authenticated)
// ============================================================================

// GET /rest/wallet/info
static bool HandleWalletInfo(string& strReply, int& nStatus)
{
    Array params;
    return CallRPCMethod("getwalletinfo", params, strReply, nStatus);
}

// GET /rest/wallet/balance
static bool HandleWalletBalance(string& strReply, int& nStatus)
{
    Array params;
    return CallRPCMethod("getbalance", params, strReply, nStatus);
}

// GET /rest/wallet/transactions[?count=N&skip=N]
static bool HandleWalletTransactions(const map<string, string>& queryParams,
                                     string& strReply, int& nStatus)
{
    Array params;
    params.push_back("*"); // all accounts

    map<string, string>::const_iterator itCount = queryParams.find("count");
    map<string, string>::const_iterator itSkip = queryParams.find("skip");

    int nCount = 10;
    int nSkip = 0;
    if (itCount != queryParams.end())
        nCount = atoi(itCount->second.c_str());
    if (itSkip != queryParams.end())
        nSkip = atoi(itSkip->second.c_str());

    params.push_back(nCount);
    params.push_back(nSkip);
    return CallRPCMethod("listtransactions", params, strReply, nStatus);
}

// GET /rest/wallet/transaction/{txid}
static bool HandleWalletTransaction(const string& txid, string& strReply, int& nStatus)
{
    Array params;
    params.push_back(txid);
    return CallRPCMethod("gettransaction", params, strReply, nStatus);
}

// GET /rest/wallet/unspent[?minconf=N&maxconf=N]
static bool HandleWalletUnspent(const map<string, string>& queryParams,
                                string& strReply, int& nStatus)
{
    Array params;
    map<string, string>::const_iterator itMin = queryParams.find("minconf");
    map<string, string>::const_iterator itMax = queryParams.find("maxconf");

    params.push_back(itMin != queryParams.end() ? atoi(itMin->second.c_str()) : 1);
    params.push_back(itMax != queryParams.end() ? atoi(itMax->second.c_str()) : 9999999);
    return CallRPCMethod("listunspent", params, strReply, nStatus);
}

// GET /rest/wallet/addresses
static bool HandleWalletAddresses(string& strReply, int& nStatus)
{
    Array params;
    return CallRPCMethod("listaddressgroupings", params, strReply, nStatus);
}

// GET /rest/wallet/staking
static bool HandleWalletStaking(string& strReply, int& nStatus)
{
    Array params;
    return CallRPCMethod("getstakinginfo", params, strReply, nStatus);
}

// POST /rest/wallet/address/new  body: {} or {"account":"..."}
static bool HandleWalletNewAddress(const string& strBody, string& strReply, int& nStatus)
{
    Array params;
    if (!strBody.empty()) {
        Value valBody;
        if (read_string(strBody, valBody) && valBody.type() == obj_type) {
            Value acctVal = find_value(valBody.get_obj(), "account");
            if (acctVal.type() == str_type)
                params.push_back(acctVal.get_str());
        }
    }
    return CallRPCMethod("getnewaddress", params, strReply, nStatus);
}

// POST /rest/wallet/send  body: {"address":"...", "amount":N}
static bool HandleWalletSend(const string& strBody, string& strReply, int& nStatus)
{
    Value valBody;
    if (!read_string(strBody, valBody) || valBody.type() != obj_type) {
        nStatus = HTTP_BAD_REQUEST;
        strReply = RESTError("Invalid JSON body. Expected: {\"address\":\"...\", \"amount\":N}");
        return true;
    }
    Object bodyObj = valBody.get_obj();
    Value addrVal = find_value(bodyObj, "address");
    Value amtVal = find_value(bodyObj, "amount");
    if (addrVal.type() != str_type || (amtVal.type() != real_type && amtVal.type() != int_type)) {
        nStatus = HTTP_BAD_REQUEST;
        strReply = RESTError("Missing 'address' (string) or 'amount' (number) in request body");
        return true;
    }
    Array params;
    params.push_back(addrVal.get_str());
    params.push_back(amtVal);

    // Optional comment fields
    Value commentVal = find_value(bodyObj, "comment");
    Value commentToVal = find_value(bodyObj, "comment_to");
    if (commentVal.type() == str_type)
        params.push_back(commentVal.get_str());
    else
        params.push_back("");
    if (commentToVal.type() == str_type)
        params.push_back(commentToVal.get_str());

    return CallRPCMethod("sendtoaddress", params, strReply, nStatus);
}

// POST /rest/wallet/sendmany  body: {"recipients":{"addr":amount,...}}
static bool HandleWalletSendMany(const string& strBody, string& strReply, int& nStatus)
{
    Value valBody;
    if (!read_string(strBody, valBody) || valBody.type() != obj_type) {
        nStatus = HTTP_BAD_REQUEST;
        strReply = RESTError("Invalid JSON body. Expected: {\"recipients\":{\"addr\":amount,...}}");
        return true;
    }
    Object bodyObj = valBody.get_obj();
    Value recipVal = find_value(bodyObj, "recipients");
    if (recipVal.type() != obj_type) {
        nStatus = HTTP_BAD_REQUEST;
        strReply = RESTError("Missing 'recipients' object in request body");
        return true;
    }
    Array params;
    params.push_back("");           // fromaccount (default)
    params.push_back(recipVal);     // {addr: amount, ...}
    return CallRPCMethod("sendmany", params, strReply, nStatus);
}

// POST /rest/wallet/unlock  body: {"passphrase":"...", "timeout":N, "staking_only":bool}
static bool HandleWalletUnlock(const string& strBody, string& strReply, int& nStatus)
{
    Value valBody;
    if (!read_string(strBody, valBody) || valBody.type() != obj_type) {
        nStatus = HTTP_BAD_REQUEST;
        strReply = RESTError("Invalid JSON body. Expected: {\"passphrase\":\"...\", \"timeout\":N}");
        return true;
    }
    Object bodyObj = valBody.get_obj();
    Value passVal = find_value(bodyObj, "passphrase");
    Value timeVal = find_value(bodyObj, "timeout");
    if (passVal.type() != str_type || timeVal.type() != int_type) {
        nStatus = HTTP_BAD_REQUEST;
        strReply = RESTError("Missing 'passphrase' (string) or 'timeout' (integer) in request body");
        return true;
    }
    Array params;
    params.push_back(passVal.get_str());
    params.push_back(timeVal.get_int());

    Value stakingVal = find_value(bodyObj, "staking_only");
    if (stakingVal.type() == bool_type)
        params.push_back(stakingVal.get_bool());

    return CallRPCMethod("walletpassphrase", params, strReply, nStatus);
}

// POST /rest/wallet/lock
static bool HandleWalletLock(string& strReply, int& nStatus)
{
    Array params;
    return CallRPCMethod("walletlock", params, strReply, nStatus);
}

// ============================================================================
// Main router
// ============================================================================

bool HandleRESTRequest(const string& strMethod,
                       const string& strURI,
                       const string& strBody,
                       map<string, string>& mapHeaders,
                       string& strReply,
                       string& strContentType,
                       int& nStatus)
{
    strContentType = "application/json";

    // OPTIONS: CORS preflight
    if (strMethod == "OPTIONS") {
        nStatus = 204;
        strReply = "";
        return true;
    }

    // Parse path
    vector<string> parts;
    map<string, string> queryParams;
    ParseRESTPath(strURI, parts, queryParams);
    // parts: ["", "rest", "resource", "param", ...]

    if (parts.size() < 3) {
        nStatus = HTTP_NOT_FOUND;
        strReply = RESTError("Not found");
        return true;
    }

    string resource = parts[2];

    // Detect format suffix (.hex, .json)
    string format = "json";
    string lastPart = parts.size() > 3 ? parts[parts.size() - 1] : "";
    size_t dotPos = lastPart.rfind('.');
    string param;
    if (dotPos != string::npos) {
        param = lastPart.substr(0, dotPos);
        format = lastPart.substr(dotPos + 1);
    } else {
        param = lastPart;
    }

    if (format == "hex")
        strContentType = "text/plain";

    // ---- Wallet endpoints (authenticated) ----
    if (resource == "wallet") {
        if (!RESTAuthorized(mapHeaders)) {
            nStatus = HTTP_UNAUTHORIZED;
            strReply = RESTError("Authentication required for wallet endpoints");
            return true;
        }

        if (parts.size() < 4) {
            nStatus = HTTP_NOT_FOUND;
            strReply = RESTError("Unknown wallet endpoint");
            return true;
        }

        string walletResource = parts[3];

        if (strMethod == "GET") {
            if (walletResource == "info")
                return HandleWalletInfo(strReply, nStatus);
            if (walletResource == "balance")
                return HandleWalletBalance(strReply, nStatus);
            if (walletResource == "transactions")
                return HandleWalletTransactions(queryParams, strReply, nStatus);
            if (walletResource == "transaction" && parts.size() > 4)
                return HandleWalletTransaction(parts[4], strReply, nStatus);
            if (walletResource == "unspent")
                return HandleWalletUnspent(queryParams, strReply, nStatus);
            if (walletResource == "addresses")
                return HandleWalletAddresses(strReply, nStatus);
            if (walletResource == "staking")
                return HandleWalletStaking(strReply, nStatus);
        }
        else if (strMethod == "POST") {
            if (walletResource == "address" && parts.size() > 4 && parts[4] == "new")
                return HandleWalletNewAddress(strBody, strReply, nStatus);
            if (walletResource == "send")
                return HandleWalletSend(strBody, strReply, nStatus);
            if (walletResource == "sendmany")
                return HandleWalletSendMany(strBody, strReply, nStatus);
            if (walletResource == "unlock")
                return HandleWalletUnlock(strBody, strReply, nStatus);
            if (walletResource == "lock")
                return HandleWalletLock(strReply, nStatus);
        }

        nStatus = HTTP_NOT_FOUND;
        strReply = RESTError("Unknown wallet endpoint");
        return true;
    }

    // ---- Public GET endpoints ----
    if (strMethod == "GET") {
        if (resource == "chaininfo")
            return HandleChainInfo(strReply, nStatus);
        if (resource == "block" && !param.empty())
            return HandleBlock(param, format, strReply, nStatus);
        if (resource == "blockheader" && !param.empty())
            return HandleBlockHeader(param, strReply, nStatus);
        if (resource == "tx") {
            // /rest/tx/decode and /rest/tx/send are POST-only
            if (!param.empty())
                return HandleTx(param, format, strReply, nStatus);
        }
        if (resource == "blockhashbyheight" && !param.empty())
            return HandleBlockHashByHeight(param, strReply, nStatus);
        if (resource == "blockbyheight" && !param.empty())
            return HandleBlockByHeight(param, format, strReply, nStatus);
        if (resource == "mempool")
            return HandleMempool(strReply, nStatus);
        if (resource == "difficulty")
            return HandleDifficulty(strReply, nStatus);
        if (resource == "supply")
            return HandleSupply(strReply, nStatus);
        if (resource == "staking")
            return HandleStaking(strReply, nStatus);
        if (resource == "mining")
            return HandleMining(strReply, nStatus);
        if (resource == "subsidy")
            return HandleSubsidy(strReply, nStatus);
        if (resource == "estimatefee")
            return HandleEstimateFee(strReply, nStatus);
        if (resource == "checkpoint")
            return HandleCheckpoint(strReply, nStatus);
        if (resource == "network")
            return HandleNetwork(strReply, nStatus);
        if (resource == "peers")
            return HandlePeers(strReply, nStatus);
        if (resource == "validate" && !param.empty())
            return HandleValidate(param, strReply, nStatus);

        // Address endpoints: /rest/address/{addr}/balance etc.
        if (resource == "address" && parts.size() >= 5) {
            string addr = parts[3];
            string addrAction = parts[4];
            if (addrAction == "balance")
                return HandleAddressBalance(addr, strReply, nStatus);
            if (addrAction == "utxos")
                return HandleAddressUtxos(addr, strReply, nStatus);
            if (addrAction == "txids")
                return HandleAddressTxids(addr, queryParams, strReply, nStatus);
        }
    }

    // ---- Public POST endpoints ----
    if (strMethod == "POST") {
        if (resource == "tx" && parts.size() >= 4) {
            string txAction = parts[3];
            if (txAction == "decode")
                return HandleTxDecode(strBody, strReply, nStatus);
            if (txAction == "send")
                return HandleTxSend(strBody, strReply, nStatus);
        }
    }

    nStatus = HTTP_NOT_FOUND;
    strReply = RESTError("Unknown REST endpoint");
    return true;
}
