// Copyright (c) 2014-2026 The Cryptographic Triangles developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// triangles-cli — JSON-RPC client for trianglesd.
//
// Talks to a running trianglesd over HTTP/1.1 with HTTP Basic auth and
// JSON-RPC 1.0. Patterned after bitcoin-cli (Bitcoin Core) and dash-cli.
//
// Build with -DBUILD_CLI=ON (default ON).
//
// Self-contained: does NOT link util.cpp / wallet.cpp / net.cpp / triangles_common.
// Only links json_compat (nlohmann/json via json_spirit shim), boost (asio +
// program_options + filesystem + system), and OpenSSL (for base64).
// This keeps the CLI binary small (~600 KB stripped on Linux, ~1.5 MB on Windows).
//
// Connection parameters (highest precedence first):
//   1. Command line flags: -rpcuser/-rpcpassword/-rpcconnect/-rpcport
//   2. triangles.conf in the data directory (or -conf=<path>)
//   3. Defaults: 127.0.0.1:19111 mainnet, 19112 testnet; no auth (must be set in conf)
//
// Usage:
//   triangles-cli help                              List commands (delegates to daemon)
//   triangles-cli help <command>                    Help for one command
//   triangles-cli getinfo                           Example: summary info
//   triangles-cli getblockchaininfo                Example: chain state
//   triangles-cli getbalance                        Example: 0-arg call
//   triangles-cli getbalance "*" 6                  Example: positional args
//   triangles-cli sendtoaddress <addr> 1.5 "memo"    Example: mixed types
//   triangles-cli -getinfo                          Synthesized summary from multiple RPCs
//   triangles-cli -raw <method> <args...>           Print raw JSON response (no pretty-print)
//
// Any command-line arg that parses as a JSON literal (number, bool, null,
// object, array) is forwarded as that literal; otherwise it is sent as a JSON
// string. This matches bitcoin-cli semantics.

#define TRIANGLES_CLI_VERSION "1.0.0"

#include "json/json_compat.h"

#include <boost/asio.hpp>
#include <boost/asio/streambuf.hpp>

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using namespace std;
namespace asio = boost::asio;
using boost::asio::ip::tcp;
namespace fs = boost::filesystem;
using namespace json_spirit;

// ─────────────────────────────────────────────────────────────────────────────
// Minimal arg/config plumbing — self-contained, no util.cpp dep.
// ─────────────────────────────────────────────────────────────────────────────

static map<string, string> mapArgs;
static map<string, vector<string> > mapMultiArgs;

static string GetArg(const string& key, const string& def = "")
{
    auto it = mapArgs.find(key);
    return (it != mapArgs.end()) ? it->second : def;
}

static bool GetBoolArg(const string& key, bool def)
{
    auto it = mapArgs.find(key);
    if (it == mapArgs.end()) return def;
    string v = it->second;
    if (v.empty()) return true;
    return (v != "0" && v != "false" && v != "no");
}

static void ReadConfigFile(const string& path)
{
    ifstream f(path);
    if (!f.good()) return;
    string line;
    while (getline(f, line)) {
        // Strip CR (Windows) and leading whitespace
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t start = line.find_first_not_of(" \t");
        if (start == string::npos) continue;
        if (line[start] == '#') continue;
        // Parse key = value
        size_t eq = line.find('=', start);
        if (eq == string::npos) continue;
        string key   = line.substr(start, eq - start);
        string value = line.substr(eq + 1);
        // Trim whitespace on both ends
        auto trim = [](string& s) {
            size_t a = s.find_first_not_of(" \t");
            size_t b = s.find_last_not_of(" \t");
            if (a == string::npos) { s.clear(); return; }
            s = s.substr(a, b - a + 1);
        };
        trim(key);
        trim(value);
        // Strip surrounding quotes
        if (value.size() >= 2 &&
            ((value.front() == '"'  && value.back() == '"') ||
             (value.front() == '\'' && value.back() == '\''))) {
            value = value.substr(1, value.size() - 2);
        }
        string dashKey = "-" + key;
        if (mapArgs.count(dashKey) == 0) {
            mapArgs[dashKey] = value;
            mapMultiArgs[dashKey].push_back(value);
        }
    }
}

// Cross-platform default data directory (matches the daemon's path)
static fs::path GetDefaultDataDir()
{
#ifdef WIN32
    // %APPDATA%/CryptographicTriangles
    const char* appdata = getenv("APPDATA");
    if (appdata && *appdata) {
        return fs::path(appdata) / "CryptographicTriangles";
    }
    return fs::path("C:/CryptographicTriangles");
#elif defined(__APPLE__)
    // ~/Library/Application Support/CryptographicTriangles
    const char* home = getenv("HOME");
    if (home && *home) {
        return fs::path(home) / "Library/Application Support/CryptographicTriangles";
    }
    return fs::path("/tmp/CryptographicTriangles");
#else
    // ~/.cryptographic-triangles (matches daemon's GetDefaultDataDir)
    const char* home = getenv("HOME");
    if (home && *home) {
        return fs::path(home) / ".cryptographic-triangles";
    }
    return fs::path("/tmp/CryptographicTriangles");
#endif
}

static fs::path GetConfigFilePath()
{
    fs::path confPath = GetArg("-conf", "triangles.conf");
    if (confPath.is_absolute()) return confPath;
    fs::path datadir = GetArg("-datadir", "");
    if (datadir.empty()) datadir = GetDefaultDataDir().string();
    return fs::path(datadir) / confPath;
}

// ─────────────────────────────────────────────────────────────────────────────
// Command-line parsing
// ─────────────────────────────────────────────────────────────────────────────

static void ParseCommandLine(int argc, char* const argv[])
{
    mapArgs.clear();
    mapMultiArgs.clear();
    for (int i = 1; i < argc; ++i) {
        string str(argv[i]);
        // Bare "-" means: read remaining args from stdin
        if (str == "-") {
            mapMultiArgs["-"].push_back("-");
            continue;
        }
        string strKey, strVal;
        size_t idx = str.find('=');
        if (idx == string::npos) {
            strKey = "-" + str;
            strVal = "1";
        } else {
            strKey = "-" + str.substr(0, idx);
            strVal = str.substr(idx + 1);
        }
        mapArgs[strKey] = strVal;
        mapMultiArgs[strKey].push_back(strVal);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// RPC connection parameters
// ─────────────────────────────────────────────────────────────────────────────

struct RPCConn {
    string host = "127.0.0.1";
    string port = "19111";
    string user;
    string pass;
};

static int AppInitRPCConn(RPCConn& conn)
{
    // Load conf file FIRST (before pulling creds) so defaults from triangles.conf
    // are visible. Command-line flags (already in mapArgs) take precedence because
    // ReadConfigFile only inserts when key is absent.
    fs::path confPath = GetConfigFilePath();
    if (!confPath.empty()) ReadConfigFile(confPath.string());

    bool fTestNet = GetBoolArg("-testnet", false);
    conn.port = GetArg("-rpcport", fTestNet ? "19112" : "19111");
    conn.host = GetArg("-rpcconnect", "127.0.0.1");
    conn.user = GetArg("-rpcuser", "");
    conn.pass = GetArg("-rpcpassword", "");

    if (conn.user.empty() || conn.pass.empty()) {
        cerr << "triangles-cli: missing RPC credentials. Set rpcuser/rpcpassword in triangles.conf\n"
             << "              or pass -rpcuser=<user> -rpcpassword=<pw> on the command line.\n"
             << "              (RPC config file: " << confPath.string() << ")\n";
        return 1;
    }
    return 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// JSON-RPC param conversion
// ─────────────────────────────────────────────────────────────────────────────

static Value ParseCLIParam(const string& arg)
{
    if (arg.empty()) {
        return Value(string(""));
    }
    Value v;
    // Try parsing the arg as JSON. If it parses to a non-string literal, keep.
    if (read_string(arg, v) && v.type() != str_type) {
        return v;
    }
    return Value(arg);
}

static void ParseCommandLineRPCParams(int argc, char* const argv[],
                                      Value& method, Array& params)
{
    method = Value(string(""));
    params.clear();
    int i = 1;
    // Skip leading flags
    static const set<string> valFlags = {
        "-conf", "-datadir", "-rpcconnect", "-rpcport",
        "-rpcuser", "-rpcpassword"
    };
    while (i < argc) {
        string arg(argv[i]);
        if (arg == "-" || arg.size() < 2 || arg[0] != '-') break;
        if (valFlags.count(arg) && i + 1 < argc &&
            string(argv[i+1]).substr(0,1) != "-") {
            i += 2;
        } else {
            ++i;
        }
    }
    if (i >= argc) {
        method = Value(string("help"));
        return;
    }
    method = Value(string(argv[i]));
    ++i;
    while (i < argc) {
        string arg(argv[i]);
        if (arg == "-") {
            string line;
            while (getline(cin, line)) {
                if (!line.empty() && line.back() == '\r') line.pop_back();
                params.push_back(ParseCLIParam(line));
            }
        } else {
            params.push_back(ParseCLIParam(arg));
        }
        ++i;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Base64 (RFC 4648) — for HTTP Basic auth
// ─────────────────────────────────────────────────────────────────────────────

static const char b64_table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static string Base64Encode(const string& in)
{
    string out;
    out.reserve(((in.size() + 2) / 3) * 4);
    int val = 0, valb = -6;
    for (unsigned char c : in) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            out.push_back(b64_table[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) out.push_back(b64_table[((val << 8) >> (valb + 8)) & 0x3F]);
    while (out.size() % 4) out.push_back('=');
    return out;
}

// ─────────────────────────────────────────────────────────────────────────────
// HTTP/1.1 JSON-RPC POST (plaintext)
// ─────────────────────────────────────────────────────────────────────────────

static int CallRPC(const RPCConn& conn, const string& strMethod,
                   const Array& params, Value& result)
{
    Object req;
    req.push_back(Pair("jsonrpc", Value(string("1.0"))));
    req.push_back(Pair("id",      Value(string("triangles-cli"))));
    req.push_back(Pair("method",  Value(strMethod)));
    req.push_back(Pair("params",  Value(params)));
    string strRequest = write_string(Value(req), false) + "\n";

    string strAuth = Base64Encode(conn.user + ":" + conn.pass);

    asio::io_context io;
    tcp::resolver resolver(io);
    boost::system::error_code ec;
    auto endpoints = resolver.resolve(conn.host, conn.port, ec);
    if (ec) {
        cerr << "triangles-cli: resolve " << conn.host << ":" << conn.port
             << " failed: " << ec.message() << "\n";
        return 1;
    }

    tcp::socket sock(io);
    sock.connect(*endpoints.begin(), ec);
    if (ec) {
        cerr << "triangles-cli: connect to " << conn.host << ":" << conn.port
             << " failed: " << ec.message() << "\n"
             << "(is trianglesd running and accepting JSON-RPC?)\n";
        return 1;
    }

    ostringstream reqStream;
    reqStream << "POST / HTTP/1.1\r\n"
              << "Host: " << conn.host << ":" << conn.port << "\r\n"
              << "Authorization: Basic " << strAuth << "\r\n"
              << "Content-Type: application/json\r\n"
              << "Content-Length: " << strRequest.size() << "\r\n"
              << "Connection: close\r\n"
              << "\r\n"
              << strRequest;
    asio::streambuf requestBuf;
    std::ostream os(&requestBuf);
    os << reqStream.str();
    asio::write(sock, requestBuf, ec);
    if (ec) {
        cerr << "triangles-cli: write failed: " << ec.message() << "\n";
        return 1;
    }

    asio::streambuf responseBuf;
    boost::system::error_code readEc;
    while (asio::read(sock, responseBuf,
                      asio::transfer_at_least(1), readEc)) {
        // keep reading until EOF or error
    }
    if (readEc && readEc != asio::error::eof) {
        cerr << "triangles-cli: read failed: " << readEc.message() << "\n";
        return 1;
    }

    std::istream rs(&responseBuf);
    string line;
    if (!std::getline(rs, line)) {
        cerr << "triangles-cli: empty response\n";
        return 1;
    }
    if (!line.empty() && line.back() == '\r') line.pop_back();
    int status = 0;
    {
        istringstream iss(line);
        string httpVer;
        iss >> httpVer >> status;
    }
    if (status != 200) {
        cerr << "triangles-cli: server returned HTTP " << status << "\n";
        ostringstream body;
        body << rs.rdbuf();
        if (!body.str().empty()) cerr << body.str() << "\n";
        return 1;
    }
    while (std::getline(rs, line) && line != "\r" && !line.empty()) {}
    string body;
    {
        ostringstream oss;
        oss << rs.rdbuf();
        body = oss.str();
    }

    Value reply;
    if (!read_string(body, reply)) {
        cerr << "triangles-cli: could not parse JSON response:\n" << body << "\n";
        return 1;
    }

    if (reply.type() != obj_type) {
        cerr << "triangles-cli: unexpected response (not an object):\n"
             << write_string(reply, true) << "\n";
        return 1;
    }

    Object replyObj = reply.get_obj();
    const Value& err = find_value(replyObj, "error");
    if (err.type() != null_type) {
        cerr << "RPC error: " << write_string(err, false) << "\n";
        return 1;
    }
    const Value& res = find_value(replyObj, "result");
    result = res;
    return 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// triangles-cli -getinfo — synthesize a friendly summary from a few RPC calls
// ─────────────────────────────────────────────────────────────────────────────

static int Getinfo(const RPCConn& conn, bool fPretty)
{
    Object info;
    Value r;
    Array emptyParams;

    if (CallRPC(conn, "getnetworkinfo", emptyParams, r) == 0) {
        info.push_back(Pair("network", r));
    }
    if (CallRPC(conn, "getblockchaininfo", emptyParams, r) == 0) {
        Object chain = r.get_obj();
        info.push_back(Pair("blockchain", r));
        info.push_back(Pair("blocks",          find_value(chain, "blocks")));
        info.push_back(Pair("headers",         find_value(chain, "headers")));
        info.push_back(Pair("bestblockhash",   find_value(chain, "bestblockhash")));
        info.push_back(Pair("difficulty",      find_value(chain, "difficulty")));
        info.push_back(Pair("verificationprogress",
                                                find_value(chain, "verificationprogress")));
        info.push_back(Pair("chain",           find_value(chain, "chain")));
    }
    if (CallRPC(conn, "getwalletinfo", emptyParams, r) == 0) {
        Object wal = r.get_obj();
        info.push_back(Pair("wallet",  r));
        info.push_back(Pair("balance", find_value(wal, "balance")));
    }
    Object connObj;
    connObj.push_back(Pair("rpcconnect", Value(conn.host)));
    connObj.push_back(Pair("rpcport",    Value(conn.port)));
    info.push_back(Pair("connection", Value(connObj)));
    cout << write_string(Value(info), fPretty) << "\n";
    return 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Help / version
// ─────────────────────────────────────────────────────────────────────────────

static int CommandLineHelp(ostream& out)
{
    out << "Usage: triangles-cli [options] <command> [params]\n"
        << "\n"
        << "  triangles-cli [options] help                List commands (delegates to daemon)\n"
        << "  triangles-cli [options] help <command>      Help for one command (delegates to daemon)\n"
        << "  triangles-cli -getinfo                      Show summary info from the daemon\n"
        << "\n"
        << "Options:\n"
        << "  -conf=<file>       Specify configuration file (default: triangles.conf)\n"
        << "  -datadir=<dir>     Specify data directory\n"
        << "  -testnet           Use testnet (RPC port 19112)\n"
        << "  -rpcconnect=<ip>   Send commands to node running on <ip> (default: 127.0.0.1)\n"
        << "  -rpcport=<port>    Connect to JSON-RPC on <port> (default: 19111 or testnet: 19112)\n"
        << "  -rpcuser=<user>    Username for JSON-RPC connections\n"
        << "  -rpcpassword=<pw>  Password for JSON-RPC connections\n"
        << "  -stdin             Read extra params from standard input, one per line\n"
        << "  -raw               Print raw JSON response (no pretty-printing)\n"
        << "  -version           Print version and exit\n"
        << "\n"
        << "Examples:\n"
        << "  triangles-cli getinfo\n"
        << "  triangles-cli getblockchaininfo\n"
        << "  triangles-cli getbalance\n"
        << "  triangles-cli getbalance \"*\" 6\n"
        << "  triangles-cli sendtoaddress <address> <amount> [comment]\n"
        << "  triangles-cli -getinfo\n"
        << "\n";
    return 0;
}

static int CommandLineVersion()
{
    cout << "triangles-cli version " << TRIANGLES_CLI_VERSION
         << " (Cryptographic Triangles RPC client)\n";
    return 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// main
// ─────────────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[])
{
    ParseCommandLine(argc, argv);

    if (argc < 2 || GetArg("-?", "") == "1" || GetArg("-h", "") == "1" ||
        GetArg("--help", "") == "1") {
        CommandLineHelp(cerr);
        return argc < 2 ? 1 : 0;
    }
    if (!GetArg("-version", "").empty() || !GetArg("--version", "").empty()) {
        CommandLineVersion();
        return 0;
    }

    RPCConn conn;
    if (AppInitRPCConn(conn) != 0) return 1;

    Value method;
    Array params;
    ParseCommandLineRPCParams(argc, argv, method, params);
    string strMethod = method.get_str();

    if (strMethod == "help" || strMethod == "-help") {
        if (params.empty()) {
            CommandLineHelp(cout);
            return 0;
        }
        // else fall through: delegate to daemon's help <command>
    }

    if (!GetArg("-getinfo", "").empty()) {
        return Getinfo(conn, /*fPretty=*/true);
    }

    bool fPretty = GetArg("-raw", "").empty();

    Value result;
    int nRet = CallRPC(conn, strMethod, params, result);
    if (nRet == 0) {
        cout << write_string(result, fPretty) << "\n";
    }
    return nRet;
}
