// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "init.h"
#include "util.h"
#include "sync.h"
#include "ui_interface.h"
#include "base58.h"
#include "trianglesrpc.h"
#include "db.h"
#include "main.h"
#include "net.h"
#include "notificationqueue.h"
#include "util_signal.h"

#undef printf
#include "rpc_httpsocket.h"   // raw-socket HTTP transport (replaces Boost.Asio)
#include <filesystem>
#include <fstream>
#include <memory>
#include <list>

#ifndef WIN32
#include <sys/select.h>
#endif

#define printf OutputDebugStringF

using namespace std;
using namespace json_spirit;
namespace fs = std::filesystem;

void ThreadRPCServer2(void* parg);

static std::string strRPCUserColonPass;

const Object emptyobj;

CNotificationQueue* pNotificationQueue = nullptr;

void ThreadRPCServer3(void* parg);

static inline unsigned short GetDefaultRPCPort()
{
    return GetBoolArg("-testnet", false) ? 19111 : 19112;
}

Object JSONRPCError(int code, const string& message)
{
    Object error;
    error.push_back(Pair("code", code));
    error.push_back(Pair("message", message));
    return error;
}

void RPCTypeCheck(const Array& params,
                  const list<Value_type>& typesExpected,
                  bool fAllowNull)
{
    unsigned int i = 0;
    for (Value_type t : typesExpected)
    {
        if (params.size() <= i)
            break;

        const Value& v = params[i];
        if (!((v.type() == t) || (fAllowNull && (v.type() == null_type))))
        {
            string err = strprintf("Expected type %s, got %s",
                                   ValueTypeName(t), ValueTypeName(v.type()));
            throw JSONRPCError(RPC_TYPE_ERROR, err);
        }
        i++;
    }
}

void RPCTypeCheck(const Object& o,
                  const map<string, Value_type>& typesExpected,
                  bool fAllowNull)
{
    for (const auto& t : typesExpected)
    {
        const Value& v = find_value(o, t.first);
        if (!fAllowNull && v.type() == null_type)
            throw JSONRPCError(RPC_TYPE_ERROR, strprintf("Missing %s", t.first.c_str()));

        if (!((v.type() == t.second) || (fAllowNull && (v.type() == null_type))))
        {
            string err = strprintf("Expected type %s for %s, got %s",
                                   ValueTypeName(t.second), t.first.c_str(), ValueTypeName(v.type()));
            throw JSONRPCError(RPC_TYPE_ERROR, err);
        }
    }
}

int64_t AmountFromValue(const Value& value)
{
    double dAmount = value.get_real();
    if (dAmount <= 0.0 || dAmount > MAX_MONEY)
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid amount");
    int64_t nAmount = roundint64(dAmount * COIN);
    if (!MoneyRange(nAmount))
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid amount");
    return nAmount;
}

Value ValueFromAmount(int64_t amount)
{
    return (double)amount / (double)COIN;
}

std::string HexBits(unsigned int nBits)
{
    union {
        int32_t nBits;
        char cBits[4];
    } uBits;
    uBits.nBits = htonl((int32_t)nBits);
    return HexStr(BEGIN(uBits.cBits), END(uBits.cBits));
}


//
// Utilities: convert hex-encoded Values
// (throws error if not hex).
//
uint256 ParseHashV(const Value& v, string strName)
{
    string strHex;
    if (v.type() == str_type)
        strHex = v.get_str();
    if (!IsHex(strHex)) // Note: IsHex("") is false
        throw JSONRPCError(RPC_INVALID_PARAMETER, strName+" must be hexadecimal string (not '"+strHex+"')");
    uint256 result;
    result.SetHex(strHex);
    return result;
}

uint256 ParseHashO(const Object& o, string strKey)
{
    return ParseHashV(find_value(o, strKey), strKey);
}

vector<unsigned char> ParseHexV(const Value& v, string strName)
{
    string strHex;
    if (v.type() == str_type)
        strHex = v.get_str();
    if (!IsHex(strHex))
        throw JSONRPCError(RPC_INVALID_PARAMETER, strName+" must be hexadecimal string (not '"+strHex+"')");
    return ParseHex(strHex);
}

vector<unsigned char> ParseHexO(const Object& o, string strKey)
{
    return ParseHexV(find_value(o, strKey), strKey);
}


///
/// Note: This interface may still be subject to change.
///

string CRPCTable::help(string strCommand) const
{
    string strRet;
    set<rpcfn_type> setDone;
    for (map<string, const CRPCCommand*>::const_iterator mi = mapCommands.begin(); mi != mapCommands.end(); ++mi)
    {
        const CRPCCommand *pcmd = mi->second;
        string strMethod = mi->first;
        // We already filter duplicates, but these deprecated screw up the sort order
        if (strMethod.find("label") != string::npos)
            continue;
        if (strCommand != "" && strMethod != strCommand)
            continue;
        try
        {
            Array params;
            rpcfn_type pfn = pcmd->actor;
            if (setDone.insert(pfn).second)
                (*pfn)(params, true);
        }
        catch (std::exception& e)
        {
            // Help text is returned in an exception
            string strHelp = string(e.what());
            if (strCommand == "")
                if (strHelp.find('\n') != string::npos)
                    strHelp = strHelp.substr(0, strHelp.find('\n'));
            strRet += strHelp + "\n";
        }
    }
    if (strRet == "")
        strRet = strprintf("help: unknown command: %s\n", strCommand.c_str());
    strRet = strRet.substr(0,strRet.size()-1);
    return strRet;
}

Value help(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
            "help [command]\n"
            "List commands, or get help for a command.");

    string strCommand;
    if (params.size() > 0)
        strCommand = params[0].get_str();

    return tableRPC.help(strCommand);
}


Value stop(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
            "stop <detach>\n"
            "<detach> is true or false to detach the database or not for this stop only\n"
            "Stop Triangles server (and possibly override the detachdb config value).");
    // Shutdown will take long enough that the response should get back
    if (params.size() > 0)
        bitdb.SetDetach(params[0].get_bool());
    StartShutdown();
    return "Triangles server stopping";
}



//
// Call Table
//


static const CRPCCommand vRPCCommands[] =
{ //  name                      function                 safemd  unlocked
  //  ------------------------  -----------------------  ------  --------
    { "help",                   &help,                   true,   true },
    { "stop",                   &stop,                   true,   true },
    { "getbestblockhash",       &getbestblockhash,       true,   false },
    { "getblockcount",          &getblockcount,          true,   false },
    { "getconnectioncount",     &getconnectioncount,     true,   false },
    { "getpeerinfo",            &getpeerinfo,            true,   false },
    { "addnode",                &addnode,                true,   false },
    { "disconnectnode",         &disconnectnode,         true,   false },
    { "getdifficulty",          &getdifficulty,          true,   false },
    { "getblockheader",         &getblockheader,         true,   false },
    { "getblockchaininfo",      &getblockchaininfo,      true,   false },
    { "getwalletinfo",          &getwalletinfo,          true,   false },
    { "getnetworkinfo",         &getnetworkinfo,         true,   false },
    { "getseedlist",            &getseedlist,            true,   false },
    { "getnetworkstability",    &getnetworkstability,    true,   false },
    { "gettxoutsetinfo",        &gettxoutsetinfo,        true,   false },
    { "estimatefee",            &estimatefee,            true,   false },
    { "getaddressbalance",      &getaddressbalance,      true,   false },
    { "getaddressutxos",        &getaddressutxos,        true,   false },
    { "getaddresstxids",        &getaddresstxids,        true,   false },
    { "getinfo",                &getinfo,                true,   false },
    { "getsubsidy",             &getsubsidy,             true,   false },
    { "getmininginfo",          &getmininginfo,          true,   false },
    { "getstakinginfo",         &getstakinginfo,         true,   false },
    { "getnewaddress",          &getnewaddress,          true,   false },
    { "getnewpubkey",           &getnewpubkey,           true,   false },
    { "getaccountaddress",      &getaccountaddress,      true,   false },
    { "setaccount",             &setaccount,             true,   false },
    { "getaccount",             &getaccount,             false,  false },
    { "getaddressesbyaccount",  &getaddressesbyaccount,  true,   false },
    { "sendtoaddress",          &sendtoaddress,          false,  false },
    { "getreceivedbyaddress",   &getreceivedbyaddress,   false,  false },
    { "getreceivedbyaccount",   &getreceivedbyaccount,   false,  false },
    { "listreceivedbyaddress",  &listreceivedbyaddress,  false,  false },
    { "listreceivedbyaccount",  &listreceivedbyaccount,  false,  false },
    { "backupwallet",           &backupwallet,           true,   false },
    { "keypoolrefill",          &keypoolrefill,          true,   false },
    { "walletpassphrase",       &walletpassphrase,       true,   false },
    { "walletpassphrasechange", &walletpassphrasechange, false,  false },
    { "walletlock",             &walletlock,             true,   false },
    { "encryptwallet",          &encryptwallet,          false,  false },
    { "validateaddress",        &validateaddress,        true,   false },
    { "validatepubkey",         &validatepubkey,         true,   false },
    { "getbalance",             &getbalance,             false,  false },
    { "move",                   &movecmd,                false,  false },
    { "sendfrom",               &sendfrom,               false,  false },
    { "sendmany",               &sendmany,               false,  false },
    { "addmultisigaddress",     &addmultisigaddress,     false,  false },
    { "addredeemscript",        &addredeemscript,        false,  false },
    { "getrawmempool",          &getrawmempool,          true,   false },
    { "getblock",               &getblock,               false,  false },
    { "getblockbynumber",       &getblockbynumber,       false,  false },
    { "getblockhash",           &getblockhash,           false,  false },
    { "gettransaction",         &gettransaction,         false,  false },
    { "listtransactions",       &listtransactions,       false,  false },
    { "listaddressgroupings",   &listaddressgroupings,   false,  false },
    { "signmessage",            &signmessage,            false,  false },
    { "verifymessage",          &verifymessage,          false,  false },
    { "listaccounts",           &listaccounts,           false,  false },
    { "settxfee",               &settxfee,               false,  false },
    { "listsinceblock",         &listsinceblock,         false,  false },
    { "dumpprivkey",            &dumpprivkey,            false,  false },
    { "hdnew",                  &hdnew,                  false,  false },
    { "hdrestore",              &hdrestore,              false,  false },
    { "hdshow",                 &hdshow,                 false,  false },
    { "hdinfo",                 &hdinfo,                 true,   false },
    { "dumpwallet",             &dumpwallet,             true,   false },
    { "importwallet",           &importwallet,           false,  false },
    { "importprivkey",          &importprivkey,          false,  false },
    { "listunspent",            &listunspent,            false,  false },
    { "getrawtransaction",      &getrawtransaction,      false,  false },
    { "createrawtransaction",   &createrawtransaction,   false,  false },
    { "decoderawtransaction",   &decoderawtransaction,   false,  false },
    { "decodescript",           &decodescript,           false,  false },
    { "signrawtransaction",     &signrawtransaction,     false,  false },
    { "sendrawtransaction",     &sendrawtransaction,     false,  false },
    { "getcheckpoint",          &getcheckpoint,          true,   false },
    { "gencheckpoints",         &gencheckpoints,          true,   false },
    { "publishcheckpoint",      &publishcheckpoint,       true,   false },
    { "getchaintips",           &getchaintips,           true,   false },
    { "invalidateblock",        &invalidateblock,        false,  false },
    { "reconsiderblock",        &reconsiderblock,        false,  false },
    { "recalculatesupply",      &recalculatesupply,      false,  false },
    { "auditsignatures",        &auditsignatures,        true,   false },
    { "dumputxoset",            &dumputxoset,            false,  false },
    { "reservebalance",         &reservebalance,         false,  true},
    { "checkwallet",            &checkwallet,            false,  true},
    { "repairwallet",           &repairwallet,           false,  true},
    { "resendtx",               &resendtx,               false,  true},
    { "abandontransaction",     &abandontransaction,     true,   true},
    { "makekeypair",            &makekeypair,            false,  true},

    { "smsgenable",             &smsgenable,             false,  false},
    { "smsgdisable",            &smsgdisable,            false,  false},
    { "smsglocalkeys",          &smsglocalkeys,          false,  false},
    { "smsgoptions",            &smsgoptions,            false,  false},
    { "smsgscanchain",          &smsgscanchain,          false,  false},
    { "smsgscanbuckets",        &smsgscanbuckets,        false,  false},
    { "smsgaddkey",             &smsgaddkey,             false,  false},
    { "smsggetpubkey",          &smsggetpubkey,          false,  false},
    { "smsgsend",               &smsgsend,               false,  false},
    { "smsgsendanon",           &smsgsendanon,           false,  false},
    { "smsginbox",              &smsginbox,              false,  false},
    { "smsgoutbox",             &smsgoutbox,             false,  false},
    { "smsgbuckets",            &smsgbuckets,            false,  false},
    { "smsgbroadcast",          &smsgbroadcast,          false,  false},
    
    
    
    
    
};

CRPCTable::CRPCTable()
{
    unsigned int vcidx;
    for (vcidx = 0; vcidx < (sizeof(vRPCCommands) / sizeof(vRPCCommands[0])); vcidx++)
    {
        const CRPCCommand *pcmd;

        pcmd = &vRPCCommands[vcidx];
        mapCommands[pcmd->name] = pcmd;
    }
}

const CRPCCommand *CRPCTable::operator[](string name) const
{
    map<string, const CRPCCommand*>::const_iterator it = mapCommands.find(name);
    if (it == mapCommands.end())
        return nullptr;
    return (*it).second;
}

//
// HTTP protocol
//
// This ain't Apache.  We're just using HTTP header for the length field
// and to be compatible with other JSON-RPC implementations.
//

string HTTPPost(const string& strMsg, const map<string,string>& mapRequestHeaders)
{
    ostringstream s;
    s << "POST / HTTP/1.1\r\n"
      << "User-Agent: Triangles-json-rpc/" << FormatFullVersion() << "\r\n"
      << "Host: 127.0.0.1\r\n"
      << "Content-Type: application/json\r\n"
      << "Content-Length: " << strMsg.size() << "\r\n"
      << "Connection: close\r\n"
      << "Accept: application/json\r\n";
    for (const auto& item : mapRequestHeaders)
        s << item.first << ": " << item.second << "\r\n";
    s << "\r\n" << strMsg;

    return s.str();
}

string rfc1123Time()
{
    char buffer[64];
    time_t now;
    time(&now);
    struct tm* now_gmt = gmtime(&now);
    string locale(setlocale(LC_TIME, nullptr));
    setlocale(LC_TIME, "C"); // we want POSIX (aka "C") weekday/month strings
    strftime(buffer, sizeof(buffer), "%a, %d %b %Y %H:%M:%S +0000", now_gmt);
    setlocale(LC_TIME, locale.c_str());
    return string(buffer);
}

static string HTTPReply(int nStatus, const string& strMsg, bool keepalive)
{
    if (nStatus == HTTP_UNAUTHORIZED)
        return strprintf("HTTP/1.0 401 Authorization Required\r\n"
            "Date: %s\r\n"
            "Server: Triangles-json-rpc/%s\r\n"
            "WWW-Authenticate: Basic realm=\"jsonrpc\"\r\n"
            "Content-Type: text/html\r\n"
            "Content-Length: 296\r\n"
            "\r\n"
            "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\"\r\n"
            "\"http://www.w3.org/TR/1999/REC-html401-19991224/loose.dtd\">\r\n"
            "<HTML>\r\n"
            "<HEAD>\r\n"
            "<TITLE>Error</TITLE>\r\n"
            "<META HTTP-EQUIV='Content-Type' CONTENT='text/html; charset=ISO-8859-1'>\r\n"
            "</HEAD>\r\n"
            "<BODY><H1>401 Unauthorized.</H1></BODY>\r\n"
            "</HTML>\r\n", rfc1123Time().c_str(), FormatFullVersion().c_str());
    const char *cStatus;
         if (nStatus == HTTP_OK) cStatus = "OK";
    else if (nStatus == HTTP_BAD_REQUEST) cStatus = "Bad Request";
    else if (nStatus == HTTP_FORBIDDEN) cStatus = "Forbidden";
    else if (nStatus == HTTP_NOT_FOUND) cStatus = "Not Found";
    else if (nStatus == HTTP_INTERNAL_SERVER_ERROR) cStatus = "Internal Server Error";
    else cStatus = "";
    return strprintf(
            "HTTP/1.1 %d %s\r\n"
            "Date: %s\r\n"
            "Connection: %s\r\n"
            "Content-Length: %" PRIszu "\r\n"
            "Content-Type: application/json\r\n"
            "Server: Triangles-json-rpc/%s\r\n"
            "\r\n"
            "%s",
        nStatus,
        cStatus,
        rfc1123Time().c_str(),
        keepalive ? "keep-alive" : "close",
        strMsg.size(),
        FormatFullVersion().c_str(),
        strMsg.c_str());
}

int ReadHTTPStatus(std::basic_istream<char>& stream, int &proto,
                   string& strMethodHTTP, string& strURI)
{
    string str;
    getline(stream, str);
    // Trim trailing \r
    if (!str.empty() && str[str.size()-1] == '\r')
        str.resize(str.size()-1);
    auto vWords = SplitString(str, ' ');
    if (vWords.size() < 2)
        return HTTP_INTERNAL_SERVER_ERROR;
    proto = 0;
    const char *ver = strstr(str.c_str(), "HTTP/1.");
    if (ver != nullptr)
        proto = atoi(ver+7);

    // Detect request line (GET/POST/...) vs response line (HTTP/1.x ...)
    if (vWords[0] == "GET" || vWords[0] == "POST" || vWords[0] == "HEAD" ||
        vWords[0] == "PUT" || vWords[0] == "DELETE" || vWords[0] == "OPTIONS") {
        strMethodHTTP = vWords[0];
        strURI = vWords[1];
        return 0; // request line, no status code
    }

    return atoi(vWords[1].c_str());
}

int ReadHTTPHeader(std::basic_istream<char>& stream, map<string, string>& mapHeadersRet)
{
    int nLen = 0;
    while (true)
    {
        string str;
        std::getline(stream, str);
        if (str.empty() || str == "\r")
            break;
        string::size_type nColon = str.find(":");
        if (nColon != string::npos)
        {
            string strHeader = str.substr(0, nColon);
            strHeader = TrimString(strHeader);
            strHeader = ToLower(strHeader);
            string strValue = str.substr(nColon+1);
            strValue = TrimString(strValue);
            mapHeadersRet[strHeader] = strValue;
            if (strHeader == "content-length")
                nLen = atoi(strValue.c_str());
        }
    }
    return nLen;
}

int ReadHTTP(std::basic_istream<char>& stream, map<string, string>& mapHeadersRet, string& strMessageRet)
{
    mapHeadersRet.clear();
    strMessageRet = "";

    // Read status/request line
    int nProto = 0;
    string strMethodHTTP, strURI;
    int nStatus = ReadHTTPStatus(stream, nProto, strMethodHTTP, strURI);
    if (!strMethodHTTP.empty())
        mapHeadersRet["_method"] = strMethodHTTP;
    if (!strURI.empty())
        mapHeadersRet["_uri"] = strURI;

    // Read header
    int nLen = ReadHTTPHeader(stream, mapHeadersRet);
    if (nLen < 0 || nLen > (int)MAX_SIZE)
        return HTTP_INTERNAL_SERVER_ERROR;

    // Read message
    if (nLen > 0)
    {
        vector<char> vch(nLen);
        stream.read(&vch[0], nLen);
        strMessageRet = string(vch.begin(), vch.end());
    }

    string sConHdr = mapHeadersRet["connection"];

    if ((sConHdr != "close") && (sConHdr != "keep-alive"))
    {
        if (nProto >= 1)
            mapHeadersRet["connection"] = "keep-alive";
        else
            mapHeadersRet["connection"] = "close";
    }

    return nStatus;
}

bool HTTPAuthorized(map<string, string>& mapHeaders)
{
    string strAuth = mapHeaders["authorization"];
    if (strAuth.size() < 6 || strAuth.substr(0,6) != "Basic ")
        return false;
    string strUserPass64 = strAuth.substr(6); strUserPass64 = TrimString(strUserPass64);
    if (strUserPass64.empty())
        return false;
    string strUserPass;
    try {
        strUserPass = DecodeBase64(strUserPass64);
    } catch (const std::exception&) {
        return false;
    }
    return TimingResistantEqual(strUserPass, strRPCUserColonPass);
}

//
// JSON-RPC protocol.  Triangles speaks version 1.0 for maximum compatibility,
// but uses JSON-RPC 1.1/2.0 standards for parts of the 1.0 standard that were
// unspecified (HTTP errors and contents of 'error').
//
// 1.0 spec: http://json-rpc.org/wiki/specification
// 1.2 spec: http://groups.google.com/group/json-rpc/web/json-rpc-over-http
// http://www.codeproject.com/KB/recipes/JSON_Spirit.aspx
//

string JSONRPCRequest(const string& strMethod, const Array& params, const Value& id)
{
    Object request;
    request.push_back(Pair("method", strMethod));
    request.push_back(Pair("params", params));
    request.push_back(Pair("id", id));
    return write_string(Value(request), false) + "\n";
}

Object JSONRPCReplyObj(const Value& result, const Value& error, const Value& id)
{
    Object reply;
    if (error.type() != null_type)
        reply.push_back(Pair("result", Value::null));
    else
        reply.push_back(Pair("result", result));
    reply.push_back(Pair("error", error));
    reply.push_back(Pair("id", id));
    return reply;
}

string JSONRPCReply(const Value& result, const Value& error, const Value& id)
{
    Object reply = JSONRPCReplyObj(result, error, id);
    return write_string(Value(reply), false) + "\n";
}

void ErrorReply(std::ostream& stream, const Object& objError, const Value& id)
{
    // Send error reply from json-rpc error object
    int nStatus = HTTP_INTERNAL_SERVER_ERROR;
    int code = find_value(objError, "code").get_int();
    if (code == RPC_INVALID_REQUEST) nStatus = HTTP_BAD_REQUEST;
    else if (code == RPC_METHOD_NOT_FOUND) nStatus = HTTP_NOT_FOUND;
    string strReply = JSONRPCReply(Value::null, objError, id);
    stream << HTTPReply(nStatus, strReply, false) << std::flush;
}

bool ClientAllowed(const std::string& strAddressIn)
{
    // Treat IPv4-mapped IPv6 addresses (::ffff:a.b.c.d) as plain IPv4.
    std::string strAddress = strAddressIn;
    const std::string v4mapped = "::ffff:";
    if (strAddress.compare(0, v4mapped.size(), v4mapped) == 0)
        strAddress = strAddress.substr(v4mapped.size());

    // Always allow loopback: ::1 and the 127.0.0.0/8 subnet.
    if (strAddress == "::1" || strAddress.compare(0, 4, "127.") == 0)
        return true;

    const vector<string>& vAllow = mapMultiArgs["-rpcallowip"];
    for (const string& strAllow : vAllow)
        if (WildcardMatch(strAddress, strAllow))
            return true;
    return false;
}

//
// A single accepted RPC connection, backed by a raw socket exposed as a
// std::iostream so the HTTP/JSON/SSE/REST code below is transport-agnostic.
//
class AcceptedConnection
{
public:
    virtual ~AcceptedConnection() {}

    virtual std::iostream& stream() = 0;
    virtual std::string peer_address_to_string() const = 0;
    virtual void close() = 0;
};

class AcceptedConnectionImpl : public AcceptedConnection
{
public:
    AcceptedConnectionImpl(SOCKET hSocketIn, const std::string& strPeer)
        : hSocket(hSocketIn), peer(strPeer), _stream(hSocketIn)
    {
    }

    ~AcceptedConnectionImpl() override
    {
        close();
    }

    std::iostream& stream() override { return _stream; }
    std::string peer_address_to_string() const override { return peer; }

    void close() override
    {
        if (hSocket != INVALID_SOCKET)
            closesocket(hSocket);  // sets hSocket = INVALID_SOCKET (see compat.h)
    }

private:
    SOCKET hSocket;
    std::string peer;
    CSocketIOStream _stream;
};

void ThreadRPCServer(void* parg)
{
    // Make this thread recognisable as the RPC listener
    RenameThread("Triangles-rpclist");

    try
    {
        vnThreadsRunning[THREAD_RPCLISTENER]++;
        ThreadRPCServer2(parg);
        vnThreadsRunning[THREAD_RPCLISTENER]--;
    }
    catch (std::exception& e) {
        vnThreadsRunning[THREAD_RPCLISTENER]--;
        PrintException(&e, "ThreadRPCServer()");
    } catch (...) {
        vnThreadsRunning[THREAD_RPCLISTENER]--;
        PrintException(nullptr, "ThreadRPCServer()");
    }
    printf("ThreadRPCServer exited\n");
}

void ThreadRPCServer2(void* parg)
{
    printf("ThreadRPCServer started\n");

    strRPCUserColonPass = mapArgs["-rpcuser"] + ":" + mapArgs["-rpcpassword"];
    if ((mapArgs["-rpcpassword"] == "") ||
        (mapArgs["-rpcuser"] == mapArgs["-rpcpassword"]))
    {
        unsigned char rand_pwd[32];
        RAND_bytes(rand_pwd, 32);
        string strWhatAmI = "To use trianglesd";
        if (mapArgs.count("-server"))
            strWhatAmI = strprintf(_("To use the %s option"), "\"-server\"");
        else if (mapArgs.count("-daemon"))
            strWhatAmI = strprintf(_("To use the %s option"), "\"-daemon\"");
        uiInterface.ThreadSafeMessageBox(strprintf(
            _("%s, you must set a rpcpassword in the configuration file:\n %s\n"
              "It is recommended you use the following random password:\n"
              "rpcuser=trianglesrpc\n"
              "rpcpassword=%s\n"
              "(you do not need to remember this password)\n"
              "The username and password MUST NOT be the same.\n"
              "If the file does not exist, create it with owner-readable-only file permissions.\n"),
                strWhatAmI.c_str(),
                GetConfigFile().string().c_str(),
                EncodeBase58(&rand_pwd[0],&rand_pwd[0]+32).c_str()),
            _("Error"), CClientUIInterface::OK | CClientUIInterface::MODAL);
        StartShutdown();
        return;
    }

    if (GetBoolArg("-rpcssl")) {
        printf("ThreadRPCServer WARNING: -rpcssl is no longer supported. RPC TLS "
               "was removed together with the Boost.Asio dependency. To reach the "
               "RPC port securely from another host, use an SSH tunnel, stunnel, "
               "or Tor.\n");
    }

    // Bind the loopback interface(s) unless the operator explicitly opened the
    // RPC port to other hosts with -rpcallowip.
    const bool loopbackOnly = !mapArgs.count("-rpcallowip");
    const int nPort = (int)GetArg("-rpcport", GetDefaultRPCPort());

    std::string strBindError;
    std::vector<SOCKET> vListen = BindRPCSockets(nPort, loopbackOnly, strBindError);
    if (vListen.empty()) {
        uiInterface.ThreadSafeMessageBox(
            strprintf(_("An error occurred while setting up the RPC port %d for listening: %s"),
                      nPort, strBindError.c_str()),
            _("Error"), CClientUIInterface::OK | CClientUIInterface::MODAL);
        StartShutdown();
        return;
    }
    printf("RPC server listening on port %d (%s)\n", nPort,
           loopbackOnly ? "loopback only" : "all interfaces");

    // Accept loop. select() with a short timeout keeps the listener responsive
    // to fShutdown. Each accepted connection is handed to its own handler thread
    // (ThreadRPCServer3), preserving the previous thread-per-connection model
    // (and keeping the blocking SSE handler working).
    vnThreadsRunning[THREAD_RPCLISTENER]--;
    while (!fShutdown)
    {
        fd_set readset;
        FD_ZERO(&readset);
        SOCKET hSocketMax = 0;
        for (SOCKET s : vListen) {
            FD_SET(s, &readset);
            if (s > hSocketMax) hSocketMax = s;
        }

        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000;  // 100 ms
        int nSelect = select(hSocketMax + 1, &readset, nullptr, nullptr, &timeout);
        if (nSelect <= 0)
            continue;  // timeout or interrupted — re-check fShutdown

        for (SOCKET s : vListen)
        {
            if (!FD_ISSET(s, &readset))
                continue;

            struct sockaddr_storage ss;
            socklen_t len = sizeof(ss);
            SOCKET hConn = accept(s, (struct sockaddr*)&ss, &len);
            if (hConn == INVALID_SOCKET) {
                printf("RPC accept() failed\n");
                continue;
            }

            const std::string strPeer = SockaddrToString((struct sockaddr*)&ss, len);

            // Filter by IP before spawning a handler thread (DoS mitigation).
            if (!ClientAllowed(strPeer)) {
                {
                    CSocketIOStream s403(hConn);
                    s403 << HTTPReply(HTTP_FORBIDDEN, "", false) << std::flush;
                }
                closesocket(hConn);
                continue;
            }

            AcceptedConnection* conn = new AcceptedConnectionImpl(hConn, strPeer);
            if (!NewThread(ThreadRPCServer3, conn)) {
                printf("Failed to create RPC server client thread\n");
                delete conn;  // destructor closes hConn
            }
        }
    }
    vnThreadsRunning[THREAD_RPCLISTENER]++;

    for (SOCKET s : vListen)
        closesocket(s);
}

class JSONRequest
{
public:
    Value id;
    string strMethod;
    Array params;

    JSONRequest() { id = Value::null; }
    void parse(const Value& valRequest);
};

void JSONRequest::parse(const Value& valRequest)
{
    // Parse request
    if (valRequest.type() != obj_type)
        throw JSONRPCError(RPC_INVALID_REQUEST, "Invalid Request object");
    const Object& request = valRequest.get_obj();

    // Parse id now so errors from here on will have the id
    id = find_value(request, "id");

    // Parse method
    Value valMethod = find_value(request, "method");
    if (valMethod.type() == null_type)
        throw JSONRPCError(RPC_INVALID_REQUEST, "Missing method");
    if (valMethod.type() != str_type)
        throw JSONRPCError(RPC_INVALID_REQUEST, "Method must be a string");
    strMethod = valMethod.get_str();
    printf("ThreadRPCServer method=%s\n", strMethod.c_str());

    // Parse params
    Value valParams = find_value(request, "params");
    if (valParams.type() == array_type)
        params = valParams.get_array();
    else if (valParams.type() == null_type)
        params = Array();
    else
        throw JSONRPCError(RPC_INVALID_REQUEST, "Params must be an array");
}

static Object JSONRPCExecOne(const Value& req)
{
    Object rpc_result;

    JSONRequest jreq;
    try {
        jreq.parse(req);

        Value result = tableRPC.execute(jreq.strMethod, jreq.params);
        rpc_result = JSONRPCReplyObj(result, Value::null, jreq.id);
    }
    catch (Object& objError)
    {
        rpc_result = JSONRPCReplyObj(Value::null, objError, jreq.id);
    }
    catch (std::exception& e)
    {
        rpc_result = JSONRPCReplyObj(Value::null,
                                     JSONRPCError(RPC_PARSE_ERROR, e.what()), jreq.id);
    }

    return rpc_result;
}

static string JSONRPCExecBatch(const Array& vReq)
{
    Array ret;
    for (unsigned int reqIdx = 0; reqIdx < vReq.size(); reqIdx++)
        ret.push_back(JSONRPCExecOne(vReq[reqIdx]));

    return write_string(Value(ret), false) + "\n";
}

// REST API support (implementation in rest.cpp)
#include "rest.h"

// Old HandleRESTRequest removed - now in rest.cpp

/**
 * Handle SSE (Server-Sent Events) stream connection.
 * Keeps the HTTP connection open and streams block/tx events as they arrive.
 * Requires authentication. Enabled with -ssenotify=1.
 */
static void HandleSSEConnection(AcceptedConnection* conn)
{
    // Send SSE headers
    std::string strHeaders = strprintf(
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/event-stream\r\n"
        "Cache-Control: no-cache\r\n"
        "Connection: keep-alive\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Server: Triangles-json-rpc/%s\r\n"
        "\r\n",
        FormatFullVersion().c_str());

    conn->stream() << strHeaders << std::flush;

    // Send initial comment to confirm connection
    conn->stream() << ": connected to Triangles SSE stream\n\n" << std::flush;

    if (!pNotificationQueue)
        return;

    // Start from current position (don't replay old events)
    uint64_t nLastId = pNotificationQueue->GetLatestId();

    while (!fShutdown)
    {
        std::vector<std::string> vEvents;
        pNotificationQueue->WaitForEvents(nLastId, vEvents, 15000, fShutdown);

        if (fShutdown)
            break;

        // Send events
        for (size_t i = 0; i < vEvents.size(); i++)
        {
            std::string strSSE = strprintf("id: %" PRIu64 "\ndata: %s\n\n", nLastId - vEvents.size() + i + 1, vEvents[i].c_str());
            try {
                conn->stream() << strSSE << std::flush;
            } catch (...) {
                // Client disconnected
                return;
            }
        }

        // Send keepalive comment if no events (prevents proxy timeouts)
        if (vEvents.empty())
        {
            try {
                conn->stream() << ": keepalive\n\n" << std::flush;
            } catch (...) {
                return;
            }
        }
    }
}

static CCriticalSection cs_THREAD_RPCHANDLER;

void ThreadRPCServer3(void* parg)
{
    // Make this thread recognisable as the RPC handler
    RenameThread("Triangles-rpchand");

    {
        LOCK(cs_THREAD_RPCHANDLER);
        vnThreadsRunning[THREAD_RPCHANDLER]++;
    }
    AcceptedConnection *conn = (AcceptedConnection *) parg;

    bool fRun = true;
    try {
    while (true)
    {
        if (fShutdown || !fRun)
        {
            conn->close();
            delete conn;
            {
                LOCK(cs_THREAD_RPCHANDLER);
                --vnThreadsRunning[THREAD_RPCHANDLER];
            }
            return;
        }
        map<string, string> mapHeaders;
        string strRequest;

        ReadHTTP(conn->stream(), mapHeaders, strRequest);

        // Handle REST API requests
        string strHTTPMethod = mapHeaders.count("_method") ? mapHeaders["_method"] : "POST";
        string strURI = mapHeaders.count("_uri") ? mapHeaders["_uri"] : "/";

        if (IsRESTPath(strURI) || (strHTTPMethod == "OPTIONS" && IsRESTPath(strURI)))
        {
            if (!GetBoolArg("-rest", false))
            {
                conn->stream() << HTTPReplyREST(HTTP_FORBIDDEN, "{\"error\":\"REST API not enabled. Start with -rest=1\"}") << std::flush;
                break;
            }

            // Rate limit public (non-wallet) endpoints
            if (strURI.find("/rest/wallet/") == string::npos)
            {
                string strPeerIP = conn->peer_address_to_string();
                if (!CheckRESTRateLimit(strPeerIP))
                {
                    conn->stream() << HTTPReplyREST(429, "{\"error\":\"Rate limit exceeded. Try again later.\"}") << std::flush;
                    break;
                }
            }

            string strReply, strContentType;
            int nRESTStatus;
            HandleRESTRequest(strHTTPMethod, strURI, strRequest, mapHeaders, strReply, strContentType, nRESTStatus);
            conn->stream() << HTTPReplyREST(nRESTStatus, strReply, strContentType) << std::flush;
            break;
        }

        // Check authorization
        if (mapHeaders.count("authorization") == 0)
        {
            conn->stream() << HTTPReply(HTTP_UNAUTHORIZED, "", false) << std::flush;
            break;
        }
        if (!HTTPAuthorized(mapHeaders))
        {
            printf("ThreadRPCServer incorrect password attempt from %s\n", conn->peer_address_to_string().c_str());
            /* Deter brute-forcing short passwords.
               If this results in a DOS the user really
               shouldn't have their RPC port exposed.*/
            if (mapArgs["-rpcpassword"].size() < 20)
                MilliSleep(250);

            conn->stream() << HTTPReply(HTTP_UNAUTHORIZED, "", false) << std::flush;
            break;
        }
        if (mapHeaders["connection"] == "close")
            fRun = false;

        // Handle SSE stream (authenticated, long-lived connection)
        if (strHTTPMethod == "GET" && (strURI == "/events" || strURI == "/events/"))
        {
            if (!GetBoolArg("-ssenotify", false))
            {
                conn->stream() << HTTPReply(HTTP_FORBIDDEN, "{\"error\":\"SSE not enabled. Start with -ssenotify=1\"}", false) << std::flush;
                break;
            }
            HandleSSEConnection(conn);
            break;
        }

        JSONRequest jreq;
        try
        {
            // Parse request
            Value valRequest;
            if (!read_string(strRequest, valRequest))
                throw JSONRPCError(RPC_PARSE_ERROR, "Parse error");

            string strReply;

            // singleton request
            if (valRequest.type() == obj_type) {
                jreq.parse(valRequest);

                Value result = tableRPC.execute(jreq.strMethod, jreq.params);

                // Send reply
                strReply = JSONRPCReply(result, Value::null, jreq.id);

            // array of requests
            } else if (valRequest.type() == array_type)
                strReply = JSONRPCExecBatch(valRequest.get_array());
            else
                throw JSONRPCError(RPC_PARSE_ERROR, "Top-level object parse error");

            conn->stream() << HTTPReply(HTTP_OK, strReply, fRun) << std::flush;
        }
        catch (Object& objError)
        {
            ErrorReply(conn->stream(), objError, jreq.id);
            break;
        }
        catch (std::exception& e)
        {
            ErrorReply(conn->stream(), JSONRPCError(RPC_PARSE_ERROR, e.what()), jreq.id);
            break;
        }
    }

    } // end try
    catch (std::exception& e) {
        PrintException(&e, "ThreadRPCServer3()");
    } catch (...) {
        PrintException(NULL, "ThreadRPCServer3()");
    }

    delete conn;
    {
        LOCK(cs_THREAD_RPCHANDLER);
        vnThreadsRunning[THREAD_RPCHANDLER]--;
    }
}

json_spirit::Value CRPCTable::execute(const std::string &strMethod, const json_spirit::Array &params) const
{
    // Find method
    const CRPCCommand *pcmd = tableRPC[strMethod];
    if (!pcmd)
        throw JSONRPCError(RPC_METHOD_NOT_FOUND, "Method not found");

    // Observe safe mode
    string strWarning = GetWarnings("rpc");
    if (strWarning != "" && !GetBoolArg("-disablesafemode") &&
        !pcmd->okSafeMode)
        throw JSONRPCError(RPC_FORBIDDEN_BY_SAFE_MODE, string("Safe mode: ") + strWarning);

    try
    {
        // Execute
        Value result;
        {
            if (pcmd->unlocked)
                result = pcmd->actor(params, false);
            else {
                LOCK2(cs_main, pwalletMain->cs_wallet);
                result = pcmd->actor(params, false);
            }
        }
        return result;
    }
    catch (std::exception& e)
    {
        throw JSONRPCError(RPC_MISC_ERROR, e.what());
    }
}


Object CallRPC(const string& strMethod, const Array& params)
{
    if (mapArgs["-rpcuser"] == "" && mapArgs["-rpcpassword"] == "")
        throw runtime_error(strprintf(
            _("You must set rpcpassword=<password> in the configuration file:\n%s\n"
              "If the file does not exist, create it with owner-readable-only file permissions."),
                GetConfigFile().string().c_str()));

    // Connect to the RPC server over a plain TCP socket. (RPC TLS was removed
    // with the Boost.Asio dependency; tunnel the connection for remote use.)
    SOCKET hSocket = ConnectRPCSocket(
        GetArg(std::string_view{"-rpcconnect"}, std::string_view{"127.0.0.1"}),
        (int)GetArg(std::string_view{"-rpcport"}, (int64_t)GetDefaultRPCPort()));
    if (hSocket == INVALID_SOCKET)
        throw runtime_error("couldn't connect to server");
    CSocketIOStream stream(hSocket);

    // HTTP basic authentication
    string strUserPass64 = EncodeBase64(mapArgs["-rpcuser"] + ":" + mapArgs["-rpcpassword"]);
    map<string, string> mapRequestHeaders;
    mapRequestHeaders["Authorization"] = string("Basic ") + strUserPass64;

    // Send request
    string strRequest = JSONRPCRequest(strMethod, params, 1);
    string strPost = HTTPPost(strRequest, mapRequestHeaders);
    stream << strPost << std::flush;

    // Receive reply
    map<string, string> mapHeaders;
    string strReply;
    int nStatus = ReadHTTP(stream, mapHeaders, strReply);
    closesocket(hSocket);
    if (nStatus == HTTP_UNAUTHORIZED)
        throw runtime_error("incorrect rpcuser or rpcpassword (authorization failed)");
    else if (nStatus >= 400 && nStatus != HTTP_BAD_REQUEST && nStatus != HTTP_NOT_FOUND && nStatus != HTTP_INTERNAL_SERVER_ERROR)
        throw runtime_error(strprintf("server returned HTTP error %d", nStatus));
    else if (strReply.empty())
        throw runtime_error("no response from server");

    // Parse reply
    Value valReply;
    if (!read_string(strReply, valReply))
        throw runtime_error("couldn't parse reply from server");
    const Object& reply = valReply.get_obj();
    if (reply.empty())
        throw runtime_error("expected reply to have result, error and id properties");

    return reply;
}




template<typename T>
void ConvertTo(Value& value, bool fAllowNull=false)
{
    if (fAllowNull && value.type() == null_type)
        return;
    if (value.type() == str_type)
    {
        // reinterpret string as unquoted json value
        Value value2;
        string strJSON = value.get_str();
        if (!read_string(strJSON, value2))
            throw runtime_error(string("Error parsing JSON:")+strJSON);
        ConvertTo<T>(value2, fAllowNull);
        value = value2;
    }
    else
    {
        value = value.get_value<T>();
    }
}

// Convert strings to command-specific RPC representation
Array RPCConvertValues(const std::string &strMethod, const std::vector<std::string> &strParams)
{
    Array params;
    for (const std::string &param : strParams)
        params.push_back(param);

    int n = params.size();

    //
    // Special case non-string parameter types
    //
    if (strMethod == "stop"                   && n > 0) ConvertTo<bool>(params[0]);
    if (strMethod == "sendtoaddress"          && n > 1) ConvertTo<double>(params[1]);
    if (strMethod == "settxfee"               && n > 0) ConvertTo<double>(params[0]);
    if (strMethod == "getreceivedbyaddress"   && n > 1) ConvertTo<int64_t>(params[1]);
    if (strMethod == "getreceivedbyaccount"   && n > 1) ConvertTo<int64_t>(params[1]);
    if (strMethod == "listreceivedbyaddress"  && n > 0) ConvertTo<int64_t>(params[0]);
    if (strMethod == "listreceivedbyaddress"  && n > 1) ConvertTo<bool>(params[1]);
    if (strMethod == "listreceivedbyaccount"  && n > 0) ConvertTo<int64_t>(params[0]);
    if (strMethod == "listreceivedbyaccount"  && n > 1) ConvertTo<bool>(params[1]);
    if (strMethod == "getbalance"             && n > 1) ConvertTo<int64_t>(params[1]);
    if (strMethod == "getblock"               && n > 1) ConvertTo<bool>(params[1]);
    if (strMethod == "getblockbynumber"       && n > 0) ConvertTo<int64_t>(params[0]);
    if (strMethod == "getblockbynumber"       && n > 1) ConvertTo<bool>(params[1]);
    if (strMethod == "getblockhash"           && n > 0) ConvertTo<int64_t>(params[0]);
    if (strMethod == "move"                   && n > 2) ConvertTo<double>(params[2]);
    if (strMethod == "move"                   && n > 3) ConvertTo<int64_t>(params[3]);
    if (strMethod == "sendfrom"               && n > 2) ConvertTo<double>(params[2]);
    if (strMethod == "sendfrom"               && n > 3) ConvertTo<int64_t>(params[3]);
    if (strMethod == "listtransactions"       && n > 1) ConvertTo<int64_t>(params[1]);
    if (strMethod == "listtransactions"       && n > 2) ConvertTo<int64_t>(params[2]);
    if (strMethod == "listaccounts"           && n > 0) ConvertTo<int64_t>(params[0]);
    if (strMethod == "walletpassphrase"       && n > 1) ConvertTo<int64_t>(params[1]);
    if (strMethod == "walletpassphrase"       && n > 2) ConvertTo<bool>(params[2]);
    if (strMethod == "listsinceblock"         && n > 1) ConvertTo<int64_t>(params[1]);

    if (strMethod == "sendmany"               && n > 1) ConvertTo<Object>(params[1]);
    if (strMethod == "sendmany"               && n > 2) ConvertTo<int64_t>(params[2]);
    if (strMethod == "reservebalance"         && n > 0) ConvertTo<bool>(params[0]);
    if (strMethod == "reservebalance"         && n > 1) ConvertTo<double>(params[1]);
    if (strMethod == "addmultisigaddress"     && n > 0) ConvertTo<int64_t>(params[0]);
    if (strMethod == "addmultisigaddress"     && n > 1) ConvertTo<Array>(params[1]);
    if (strMethod == "listunspent"            && n > 0) ConvertTo<int64_t>(params[0]);
    if (strMethod == "listunspent"            && n > 1) ConvertTo<int64_t>(params[1]);
    if (strMethod == "listunspent"            && n > 2) ConvertTo<Array>(params[2]);
    if (strMethod == "getrawtransaction"      && n > 1) ConvertTo<int64_t>(params[1]);
    if (strMethod == "createrawtransaction"   && n > 0) ConvertTo<Array>(params[0]);
    if (strMethod == "createrawtransaction"   && n > 1) ConvertTo<Object>(params[1]);
    if (strMethod == "signrawtransaction"     && n > 1) ConvertTo<Array>(params[1], true);
    if (strMethod == "signrawtransaction"     && n > 2) ConvertTo<Array>(params[2], true);
    if (strMethod == "keypoolrefill"          && n > 0) ConvertTo<int64_t>(params[0]);
    if (strMethod == "getblockheader"         && n > 1) ConvertTo<bool>(params[1]);
    if (strMethod == "estimatefee"            && n > 0) ConvertTo<int64_t>(params[0]);
    if (strMethod == "getaddressbalance"     && n > 0) ConvertTo<Object>(params[0]);
    if (strMethod == "getaddressutxos"       && n > 0) ConvertTo<Object>(params[0]);
    if (strMethod == "getaddresstxids"       && n > 0) ConvertTo<Object>(params[0]);

    return params;
}

int CommandLineRPC(int argc, char *argv[])
{
    string strPrint;
    int nRet = 0;
    try
    {
        // Skip switches
        while (argc > 1 && IsSwitchChar(argv[1][0]))
        {
            argc--;
            argv++;
        }

        // Method
        if (argc < 2)
            throw runtime_error("too few parameters");
        string strMethod = argv[1];

        // Parameters default to strings
        std::vector<std::string> strParams(&argv[2], &argv[argc]);
        Array params = RPCConvertValues(strMethod, strParams);

        // Execute
        Object reply = CallRPC(strMethod, params);

        // Parse reply
        const Value& result = find_value(reply, "result");
        const Value& error  = find_value(reply, "error");

        if (error.type() != null_type)
        {
            // Error
            strPrint = "error: " + write_string(error, false);
            int code = find_value(error.get_obj(), "code").get_int();
            nRet = abs(code);
        }
        else
        {
            // Result
            if (result.type() == null_type)
                strPrint = "";
            else if (result.type() == str_type)
                strPrint = result.get_str();
            else
                strPrint = write_string(result, true);
        }
    }
    catch (std::exception& e)
    {
        strPrint = string("error: ") + e.what();
        nRet = 87;
    }
    catch (...)
    {
        PrintException(nullptr, "CommandLineRPC()");
    }

    if (strPrint != "")
    {
        fprintf((nRet == 0 ? stdout : stderr), "%s\n", strPrint.c_str());
    }
    return nRet;
}




#ifdef TEST
int main(int argc, char *argv[])
{
#ifdef _MSC_VER
    // Turn off Microsoft heap dump noise
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_WARN, CreateFile("NUL", GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, 0));
#endif
    setbuf(stdin, nullptr);
    setbuf(stdout, nullptr);
    setbuf(stderr, nullptr);

    try
    {
        if (argc >= 2 && string(argv[1]) == "-server")
        {
            printf("server ready\n");
            ThreadRPCServer(nullptr);
        }
        else
        {
            return CommandLineRPC(argc, argv);
        }
    }
    catch (std::exception& e) {
        PrintException(&e, "main()");
    } catch (...) {
        PrintException(nullptr, "main()");
    }
    return 0;
}
#endif

const CRPCTable tableRPC;


