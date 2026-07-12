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
// Only links json_compat (nlohmann/json via json_spirit shim) and the platform's
// native socket library (Winsock on Windows, libc on POSIX). No Boost dependency
// at all — keeps the binary small and avoids platform-specific link problems
// with boost::asio / libboost_system (MSYS2 names them with -mt- versioned
// suffixes; Homebrew doesn't ship the CMake config for the system component).
//
// Connection parameters (highest precedence first):
//   1. Command line flags: -rpcuser/-rpcpassword/-rpcconnect/-rpcport
//   2. triangles.conf in the data directory (or -conf=<path>)
//   3. Defaults: 127.0.0.1:19112 mainnet, 19111 testnet; no auth (must be set in conf)
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

#include <filesystem>

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

// Cross-platform socket includes
#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib, "ws2_32.lib")
  using socket_t = SOCKET;
  #define TRI_CLI_INVALID_SOCKET INVALID_SOCKET
  #define TRI_CLI_CLOSE_SOCKET(s) closesocket(s)
#else
  #include <sys/types.h>
  #include <sys/stat.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <netdb.h>
  #include <unistd.h>
  #include <fcntl.h>
  #include <errno.h>
  using socket_t = int;
  #define TRI_CLI_INVALID_SOCKET (-1)
  #define TRI_CLI_CLOSE_SOCKET(s) close(s)
#endif

using namespace std;
namespace fs = std::filesystem;
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

static bool ReadConfigFile(const string& path, string& error)
{
#ifndef _WIN32
    struct stat configStat;
    if (::lstat(path.c_str(), &configStat) == 0 &&
        (!S_ISREG(configStat.st_mode) || configStat.st_uid != geteuid() ||
         (configStat.st_mode & (S_IRWXG | S_IRWXO)) != 0)) {
        error = "refusing insecure configuration file " + path +
                "; require a regular file owned by the current user with no group or other access";
        return false;
    }
#endif
    ifstream f(path);
    if (!f.good()) return true;
    string line;
    while (getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t start = line.find_first_not_of(" \t");
        if (start == string::npos) continue;
        if (line[start] == '#') continue;
        size_t eq = line.find('=', start);
        if (eq == string::npos) continue;
        string key   = line.substr(start, eq - start);
        string value = line.substr(eq + 1);
        auto trim = [](string& s) {
            size_t a = s.find_first_not_of(" \t");
            size_t b = s.find_last_not_of(" \t");
            if (a == string::npos) { s.clear(); return; }
            s = s.substr(a, b - a + 1);
        };
        trim(key);
        trim(value);
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
    return true;
}

static fs::path GetDefaultDataDir()
{
#ifdef _WIN32
    const char* appdata = getenv("APPDATA");
    if (appdata && *appdata) {
        return fs::path(appdata) / "CryptographicTriangles";
    }
    return fs::path("C:/CryptographicTriangles");
#elif defined(__APPLE__)
    const char* home = getenv("HOME");
    if (home && *home) {
        return fs::path(home) / "Library/Application Support/CryptographicTriangles";
    }
    return fs::path("/tmp/CryptographicTriangles");
#else
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
    // Bug fix (round-8 NIT-1): if the user passed `-conf=` with no value
    // (shell-quoting accident, env-var unset, or copy-paste template),
    // mapArgs["-conf"] is "". Without this guard, we'd append an empty
    // string to the datadir and produce a directory path like
    // "/root/.cryptographic-triangles/" — std::ifstream opens that as a
    // directory successfully on Linux, then readConfigFile's getline
    // finds no lines and the error path mis-reports "missing BOTH
    // rpcuser/rpcpassword" for a file we never actually read.
    // Treat empty -conf as "use default name" so the conf lookup at the
    // default datadir works the same as if no -conf was passed at all.
    if (confPath.empty()) confPath = "triangles.conf";
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
        if (str == "-") {
            mapMultiArgs["-"].push_back("-");
            continue;
        }
        // Historical behavior: every arg is parsed as a potential flag.
        // A non-flag positional arg like "getinfo" gets stored as
        // mapArgs["-getinfo"] = "1" (the leading dash comes from the arg
        // itself, NOT from us prepending one). It doesn't collide with
        // any real flag because no flag has a name that looks like a
        // typical RPC method.
        //
        // Bug fix: previous code was doing strKey = "-" + str, which
        // produced "--conf" (two dashes) when the arg was "-conf=...",
        // so GetArg("-conf") lookups never matched. The arg already
        // starts with a dash, so we use it verbatim.
        string strKey, strVal;
        size_t idx = str.find('=');
        if (idx == string::npos) {
            strKey = str;       // already starts with "-", do NOT prepend another
            strVal = "1";
        } else {
            strKey = str.substr(0, idx);  // already starts with "-"
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
    string port = "19112";
    string user;
    string pass;
};

static int AppInitRPCConn(RPCConn& conn)
{
    fs::path confPath = GetConfigFilePath();
    bool confExisted = false;
    if (!confPath.empty())
    {
        std::ifstream f(confPath);
        confExisted = f.good();
        if (confExisted) {
            string configError;
            if (!ReadConfigFile(confPath.string(), configError)) {
                cerr << "triangles-cli: " << configError << "\n";
                return 1;
            }
        }
    }

    bool fTestNet = GetBoolArg("-testnet", false);
    conn.port = GetArg("-rpcport", fTestNet ? "19111" : "19112");
    conn.host = GetArg("-rpcconnect", "127.0.0.1");
    conn.user = GetArg("-rpcuser", "");
    conn.pass = GetArg("-rpcpassword", "");

    if (conn.user.empty() || conn.pass.empty()) {
        // Distinguish between "no conf file at all", "conf file exists but
        // is missing one or both keys", and "no -rpcuser/-rpcpassword
        // passed". The previous single-line error didn't tell the operator
        // which case they were in, leading to confusion when the conf path
        // was correct but a key was missing (or vice versa).
        cerr << "triangles-cli: missing RPC credentials.\n";
        if (confExisted) {
            cerr << "  Read conf: " << confPath.string() << "\n";
            if (conn.user.empty() && conn.pass.empty()) {
                cerr << "  Conf is missing BOTH rpcuser= and rpcpassword= lines.\n";
            } else if (conn.user.empty()) {
                cerr << "  Conf is missing rpcuser= (rpcpassword was found).\n";
            } else {
                cerr << "  Conf is missing rpcpassword= (rpcuser was found).\n";
            }
            cerr << "  Edit the conf to add the missing line(s), or override on the command line:\n"
                 << "    triangles-cli -rpcuser=<user> -rpcpassword=<pw> [other flags] <command>\n";
        } else {
            cerr << "  Could not read conf: " << confPath.string() << " (file not found).\n"
                 << "  Either:\n"
                 << "    - Create " << confPath.string() << " with rpcuser/rpcpassword set, or\n"
                 << "    - Pass -conf=<path> to point at a conf that exists, or\n"
                 << "    - Pass -rpcuser=<user> -rpcpassword=<pw> on the command line.\n";
            if (GetArg("-datadir", "").empty()) {
                cerr << "  Note: no -datadir was passed, so the default datadir\n"
                     << "        (" << GetDefaultDataDir().string() << ") was used to find the conf.\n"
                     << "        If your conf is elsewhere, pass -datadir=<dir> or -conf=<path>.\n";
            }
        }
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
// HTTP/1.1 JSON-RPC POST (plaintext) — using raw sockets (no Boost)
// ─────────────────────────────────────────────────────────────────────────────

namespace {

class SocketInit {
public:
    SocketInit() {
#ifdef _WIN32
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
    }
    ~SocketInit() {
#ifdef _WIN32
        WSACleanup();
#endif
    }
};

inline void close_socket(socket_t s) {
    TRI_CLI_CLOSE_SOCKET(s);
}

} // namespace

static int CallRPC(const RPCConn& conn, const string& strMethod,
                   const Array& params, Value& result)
{
    SocketInit sockInit;

    Object req;
    req.push_back(Pair("jsonrpc", Value(string("1.0"))));
    req.push_back(Pair("id",      Value(string("triangles-cli"))));
    req.push_back(Pair("method",  Value(strMethod)));
    req.push_back(Pair("params",  Value(params)));
    string strRequest = write_string(Value(req), false) + "\n";

    string strAuth = Base64Encode(conn.user + ":" + conn.pass);

    // Resolve host:port via getaddrinfo
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    struct addrinfo* addrRes = nullptr;
    int rc = getaddrinfo(conn.host.c_str(), conn.port.c_str(), &hints, &addrRes);
    if (rc != 0 || addrRes == nullptr) {
        cerr << "triangles-cli: resolve " << conn.host << ":" << conn.port
             << " failed: " << gai_strerror(rc) << "\n";
        if (addrRes) freeaddrinfo(addrRes);
        return 1;
    }

    // Try each resolved address until one connects
    socket_t sock = TRI_CLI_INVALID_SOCKET;
    for (struct addrinfo* ai = addrRes; ai != nullptr; ai = ai->ai_next) {
        sock = ::socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (sock == TRI_CLI_INVALID_SOCKET) {
            continue;
        }
        if (::connect(sock, ai->ai_addr, ai->ai_addrlen) == 0) {
            break;  // connected
        }
        close_socket(sock);
        sock = TRI_CLI_INVALID_SOCKET;
    }
    freeaddrinfo(addrRes);
    if (sock == TRI_CLI_INVALID_SOCKET) {
        cerr << "triangles-cli: connect to " << conn.host << ":" << conn.port
             << " failed\n"
             << "(is trianglesd running and accepting JSON-RPC?)\n";
        return 1;
    }

    // Build HTTP/1.1 request
    string reqData =
        "POST / HTTP/1.1\r\n"
        "Host: " + conn.host + ":" + conn.port + "\r\n"
        "Authorization: Basic " + strAuth + "\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: " + to_string(strRequest.size()) + "\r\n"
        "Connection: close\r\n"
        "\r\n" + strRequest;

    // Send
    size_t totalSent = 0;
    while (totalSent < reqData.size()) {
        ssize_t n = ::send(sock, reqData.data() + totalSent,
                           reqData.size() - totalSent, 0);
        if (n <= 0) {
            cerr << "triangles-cli: write failed\n";
            close_socket(sock);
            return 1;
        }
        totalSent += static_cast<size_t>(n);
    }

    // Read full response (until EOF)
    string respData;
    char buf[4096];
    while (true) {
        ssize_t n = ::recv(sock, buf, sizeof(buf), 0);
        if (n > 0) {
            respData.append(buf, static_cast<size_t>(n));
        } else if (n == 0) {
            break;  // EOF
        } else {
            // Error
#ifdef _WIN32
            int err = WSAGetLastError();
            if (err == WSAECONNRESET || err == WSAECONNABORTED) {
                // Treat as EOF
                break;
            }
#else
            if (errno == EINTR) continue;  // interrupted, retry
            if (errno == ECONNRESET) break;  // peer closed
#endif
            cerr << "triangles-cli: read failed\n";
            close_socket(sock);
            return 1;
        }
    }
    close_socket(sock);

    // Parse status line
    size_t hdrEnd = respData.find("\r\n\r\n");
    if (hdrEnd == string::npos) {
        cerr << "triangles-cli: malformed response (no header terminator)\n";
        return 1;
    }
    string statusLine = respData.substr(0, respData.find("\r\n"));
    int status = 0;
    {
        istringstream iss(statusLine);
        string httpVer;
        iss >> httpVer >> status;
    }
    if (status != 200) {
        cerr << "triangles-cli: server returned HTTP " << status << "\n";
        string body = respData.substr(hdrEnd + 4);
        if (!body.empty()) cerr << body << "\n";
        return 1;
    }
    string body = respData.substr(hdrEnd + 4);

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
        << "  -testnet           Use testnet (RPC port 19111)\n"
        << "  -rpcconnect=<ip>   Send commands to node running on <ip> (default: 127.0.0.1)\n"
        << "  -rpcport=<port>    Connect to JSON-RPC on <port> (default: 19112 or testnet: 19111)\n"
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
