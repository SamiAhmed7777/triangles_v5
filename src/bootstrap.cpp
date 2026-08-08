// Copyright (c) 2024 Triangles developers
// Distributed under the MIT/X11 software license

// On Windows we need winsock2.h BEFORE windows.h, otherwise windows.h
// pulls in the legacy winsock.h with conflicting declarations. Define
// WIN32_LEAN_AND_MEAN at the very top so any header that includes
// windows.h (transitively or directly) skips the bloat. This must come
// before any project header that might pull in windows.h.
#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include "bootstrap.h"
#include "bootstrap_roots.h"
#include "utxosnapshot.h"
#include "txdb.h"
#include "checkpoints.h"
#include <filesystem>
#include <fstream>
#include <zlib.h>
#include "version.h"
#include "uint256.h"
#include "netbase.h"
#include "net.h"
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include "key.h"
#include "base58.h"
#include "util.h"
#include "json/nlohmann_json.hpp"
#include <sstream>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cctype>
#include <system_error>
#include <vector>
#include <algorithm>
#include <cerrno>

extern const std::string strMessageMagic;

#ifdef WIN32
// windows.h must come after winsock2.h; WIN32_LEAN_AND_MEAN was defined
// at the top of the file so any transitive windows.h includes there are
// also trimmed.
#include <windows.h>
#else
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#include <unistd.h>
#endif

// Forward declarations to avoid pulling in heavy consensus headers
extern bool fTestNet;
namespace Checkpoints { bool IsKnownCheckpoint(int nHeight, const uint256& hash); }

namespace fs = std::filesystem;

namespace Bootstrap {

// Wide-char-safe file reader: opens a file with _wfopen on Windows
// (which accepts non-ASCII paths regardless of the ANSI code page) and
// fopen elsewhere. Reads the entire file into `out`. Returns true on
// success, false on any error (open failure, read error, flush/close
// failure, or unreadable file). Distinguishes clean EOF from ferror so
// partial reads don't masquerade as success.
static bool ReadFileToMemory(const fs::path& path, std::string& out)
{
    out.clear();
    FILE* f = nullptr;
#ifdef WIN32
    f = _wfopen(path.wstring().c_str(), L"rb");
#else
    f = fopen(path.string().c_str(), "rb");
#endif
    if (!f) return false;
    char buf[8192];
    size_t n;
    bool readOk = true;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
        out.append(buf, n);
    if (ferror(f)) readOk = false;
    // fclose() can still surface buffered-write errors that ferror
    // doesn't; treat close failure as a read failure too.
    if (fclose(f) != 0) readOk = false;
    if (!readOk) {
        out.clear();
        return false;
    }
    return true;
}

bool NeedsBootstrap(const fs::path& dataDir)
{
    // Need bootstrap if there's no chain database (the UTXO set / block index).
    // blk0001.dat alone is NOT sufficient — it's raw block data that requires
    // (fast-import was removed; UTXO snapshot is the only sync path)
    // Check for both LevelDB (txleveldb/), RocksDB (rocksdb/), and legacy
    // chainstate paths. The rocksdb/ check is critical for v6.1.x+ nodes that
    // fully migrated from LevelDB — without it, removing the legacy txleveldb/
    // directory causes the boot path to incorrectly decide "no blockchain data"
    // and trigger a 943 MB bootstrap download over Tor (DNS2 incident
    // 2026-07-03, 5-hour wedge; recovery via v3 snapshot + rm -rf rocksdb).
    bool hasChainDb = fs::exists(dataDir / "txleveldb")
                   || fs::exists(dataDir / "rocksdb")
                   || fs::exists(dataDir / "blocks" / "chainstate")
                   || fs::exists(dataDir / "chainstate");
    return !hasChainDb;
}

// Direct TCP connection bypassing Tor SOCKS proxy.
// Used for bootstrap downloads where the server is on clearnet.
static SOCKET ConnectDirectTCP(const std::string& host, int port, std::string& strError)
{
    struct addrinfo hints, *result, *rp;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    std::string portStr = std::to_string(port);
    int rc = getaddrinfo(host.c_str(), portStr.c_str(), &hints, &result);
    if (rc != 0) {
        strError = "DNS resolution failed for " + host;
        return INVALID_SOCKET;
    }

    SOCKET hSocket = INVALID_SOCKET;
    for (rp = result; rp != nullptr; rp = rp->ai_next) {
        hSocket = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (hSocket == INVALID_SOCKET)
            continue;

        // A bootstrap endpoint must not be able to wedge daemon startup by
        // accepting a connection and then never sending a response.
#ifdef WIN32
        DWORD timeoutMs = 30000;
        setsockopt(hSocket, SOL_SOCKET, SO_RCVTIMEO,
                   reinterpret_cast<const char*>(&timeoutMs), sizeof(timeoutMs));
        setsockopt(hSocket, SOL_SOCKET, SO_SNDTIMEO,
                   reinterpret_cast<const char*>(&timeoutMs), sizeof(timeoutMs));
#else
        struct timeval timeout = {30, 0};
        setsockopt(hSocket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        setsockopt(hSocket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
#endif

        if (connect(hSocket, rp->ai_addr, (int)rp->ai_addrlen) == 0)
            break; // success

        closesocket(hSocket);
        hSocket = INVALID_SOCKET;
    }
    freeaddrinfo(result);

    if (hSocket == INVALID_SOCKET)
        strError = "Cannot connect to " + host + ":" + portStr;

    return hSocket;
}

// RAII wrapper for an HTTP(S) connection (socket + optional TLS)
struct HttpConn {
    SOCKET sock;
    SSL_CTX* ctx;
    SSL* ssl;

    HttpConn() : sock(INVALID_SOCKET), ctx(nullptr), ssl(nullptr) {}
    ~HttpConn() { Close(); }

    void Close() {
        if (ssl)  { SSL_shutdown(ssl); SSL_free(ssl); ssl = nullptr; }
        if (ctx)  { SSL_CTX_free(ctx); ctx = nullptr; }
        if (sock != INVALID_SOCKET) { closesocket(sock); sock = INVALID_SOCKET; }
    }

    bool Send(const char* data, size_t len) {
        while (len > 0) {
            // MSG_NOSIGNAL is Linux/macOS only — Windows Winsock uses 0 to
            // mean "no special flags" (the equivalent of MSG_NOSIGNAL on
            // Windows would be SO_NOSIGPIPE, but that's a setsockopt, not a
            // send flag, and Windows has no SIGPIPE to suppress anyway).
#ifdef WIN32
            constexpr int sendFlags = 0;
#else
            constexpr int sendFlags = MSG_NOSIGNAL;
#endif
            int n = ssl ? SSL_write(ssl, data, (int)std::min(len, (size_t)65536))
                        : send(sock, data, (int)std::min(len, (size_t)65536), sendFlags);
            if (n <= 0) return false;
            data += n;
            len -= n;
        }
        return true;
    }

    int Recv(char* buf, int len) {
        return ssl ? SSL_read(ssl, buf, len) : recv(sock, buf, len, 0);
    }

    // Read until delimiter found. Returns data including delimiter.
    bool RecvUntil(std::string& out, const std::string& delim) {
        out.clear();
        char c;
        while (true) {
            int n = Recv(&c, 1);
            if (n <= 0) return false;
            out += c;
            if (out.size() >= delim.size() &&
                out.compare(out.size() - delim.size(), delim.size(), delim) == 0)
                return true;
            if (out.size() > 64 * 1024) return false; // header too large
        }
    }

    // Establish TLS on an already-connected socket
    bool StartTLS(const std::string& hostname, std::string& strError) {
        ctx = SSL_CTX_new(TLS_client_method());
        if (!ctx) {
            strError = "Failed to create SSL context";
            return false;
        }
        // Trust-store resolution (additive — each successful source EXPANDS the
        // trust store; embedded roots are always added as belt-and-suspenders):
        //   1. <exedir>/cacert.pem  (deploy-time bundle; covers Windows GUI
        //      builds where libssl-3-x64.dll ships without a default cert path)
        //   2. SSL_CERT_FILE env var (operator override; wide-char on Windows)
        //   3. System default verify paths (Linux daemon: /etc/ssl/certs/...)
        //   4. Embedded ISRG Root X1 + X2 (always added as belt-and-suspenders so
        //      a stripped host without usable system anchors still validates LE
        //      chains; no harm to operator-supplied bundles since adding anchors
        //      only ever expands the set of valid chains)
        fs::path exeDir;
#ifdef WIN32
        // Use a dynamic buffer for GetModuleFileNameW — the path can legally
        // exceed MAX_PATH (260) on modern Windows, and GetModuleFileNameW
        // returns the size WITHOUT the null terminator, so a return value
        // equal to the buffer capacity indicates truncation. Loop until the
        // returned size is strictly less than the buffer size.
        std::wstring exePathW;
        for (DWORD cap = MAX_PATH; ; cap *= 2) {
            std::vector<wchar_t> buf(cap);
            DWORD n = GetModuleFileNameW(nullptr, buf.data(), cap);
            if (n == 0) break; // API failure
            if (n < cap) {
                exePathW.assign(buf.data(), n);
                break;
            }
            // n == cap means truncation; try a larger buffer.
            if (cap >= 32768) break; // 32K is a reasonable upper bound
        }
        if (!exePathW.empty()) {
            // fs::path on Windows accepts a wide string directly, which
            // preserves non-ASCII characters in the executable path. The
            // previous std::wstring -> std::string -> fs::path conversion
            // was lossy on UTF-8 paths.
            exeDir = fs::path(exePathW).parent_path();
        }
#else
        std::vector<char> linkPath(4096);
        ssize_t n = readlink("/proc/self/exe", linkPath.data(), linkPath.size() - 1);
        if (n > 0) {
            linkPath[n] = '\0';
            exeDir = fs::path(std::string(linkPath.data(), n)).parent_path();
        }
#endif
        bool trustLoaded = false;
        std::string lastLoadErr;
        // Convert fs::path -> std::string in a way that preserves non-ASCII
        // characters on Windows (where fs::path::string() uses the ANSI
        // code page and silently mangles UTF-8 paths). On non-Windows
        // platforms string() is already UTF-8 and we can call it directly.
        auto pathToUtf8 = [](const fs::path& p) -> std::string {
#ifdef WIN32
            const auto& w = p.wstring();
            if (w.empty()) return std::string();
            int needed = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(),
                                             nullptr, 0, nullptr, nullptr);
            if (needed <= 0) return std::string();
            std::string out(needed, '\0');
            WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(),
                                out.data(), needed, nullptr, nullptr);
            return out;
#else
            return p.string();
#endif
        };
        // Wide-char-safe file reader is defined at file scope (Bootstrap::ReadFileToMemory)
        // so the same routine can be reused by DownloadFile, ReadFileToString,
        // and Sha256OfFile without each one duplicating _wfopen / fopen logic.
        // Parse all PEM certificates from a memory buffer and add each to
        // the X509 store. Returns true if at least one certificate was
        // successfully added. Used for cacert.pem loaded via the wide-char
        // path so we never round-trip through OpenSSL's narrow path API.
        auto loadPemBuffer = [&](const std::string& pemText, const std::string& label) -> bool {
            BIO* bio = BIO_new_mem_buf(pemText.data(), (int)pemText.size());
            if (!bio) return false;
            X509_STORE* store = SSL_CTX_get_cert_store(ctx);
            int added = 0;
            int malformed = 0;
            while (true) {
                X509* cert = PEM_read_bio_X509(bio, nullptr, nullptr, nullptr);
                if (!cert) {
                    // Distinguish clean EOF from parse error. PEM_read_bio_X509
                    // returns NULL on both; on EOF it pushes PEM_R_NO_START_LINE
                    // (the "expected BEGIN, got nothing" error code after the
                    // final cert is parsed), on parse failure it pushes other
                    // PEM_R_* codes. Treat PEM_R_NO_START_LINE as clean EOF
                    // AND verify the BIO is actually empty (no trailing junk).
                    unsigned long e = ERR_peek_last_error();
                    if (e == 0) {
                        // genuine EOF without even a "no start line" error
                        // — must also be at BIO end.
                        if (BIO_ctrl_pending(bio) == 0) break;
                        ++malformed;
                        break;
                    }
                    int reason = ERR_GET_REASON(e);
                    ERR_clear_error();
                    if (reason == PEM_R_NO_START_LINE) {
                        // PEM_R_NO_START_LINE after successful reads is the
                        // normal "end of certs" signal, but only accept it
                        // if the BIO is actually drained.
                        if (BIO_ctrl_pending(bio) == 0) break;
                    }
                    ++malformed;
                    break; // stop at first malformed PEM, don't trust the bundle
                }
                int rc = X509_STORE_add_cert(store, cert);
                X509_free(cert);
                if (rc == 1) {
                    ++added;
                } else {
                    unsigned long e = ERR_get_error();
                    if (ERR_GET_REASON(e) == X509_R_CERT_ALREADY_IN_HASH_TABLE) {
                        ERR_clear_error();
                        ++added; // count duplicates as success
                        continue;
                    }
                    break;
                }
            }
            BIO_free(bio);
            if (added > 0 && malformed == 0) {
                printf("Bootstrap: TLS trust loaded from %s (%d certs)\n",
                       label.c_str(), added);
                trustLoaded = true;
                return true;
            }
            if (malformed > 0) {
                printf("Bootstrap: %s had malformed PEM (loaded %d, rejected %d)\n",
                       label.c_str(), added, malformed);
            }
            return false;
        };
        auto tryLoadFile = [&](const std::string& label, const std::string& file) -> bool {
            if (SSL_CTX_load_verify_locations(ctx, file.c_str(), nullptr) == 1) {
                printf("Bootstrap: TLS trust loaded from %s (%s)\n", label.c_str(), file.c_str());
                trustLoaded = true;
                return true;
            }
            unsigned long e = ERR_get_error();
            char buf[256];
            ERR_error_string_n(e, buf, sizeof(buf));
            lastLoadErr = label + " (" + file + "): " + buf;
            return false;
        };
        // Wide-char-safe path: read cacert.pem ourselves via the file-scope
        // ReadFileToMemory (which uses _wfopen on Windows), then parse the
        // PEMs with OpenSSL using a memory BIO. Avoids the narrow OpenSSL
        // path API entirely.
        auto tryLoadPath = [&](const std::string& label, const fs::path& file) -> bool {
            std::string pem;
            if (!ReadFileToMemory(file, pem)) {
                lastLoadErr = label + ": cannot open " + pathToUtf8(file);
                return false;
            }
            return loadPemBuffer(pem, label);
        };
        auto tryLoadEmbedded = [&](const std::string& label, const char* pem) -> bool {
            BIO* bio = BIO_new_mem_buf(pem, -1);
            if (!bio) return false;
            X509* cert = PEM_read_bio_X509(bio, nullptr, nullptr, nullptr);
            BIO_free(bio);
            if (!cert) return false;
            X509_STORE* store = SSL_CTX_get_cert_store(ctx);
            int rc = X509_STORE_add_cert(store, cert);
            X509_free(cert);
            // X509_STORE_add_cert returns:
            //   1   → cert was added
            //   0   → failure (parse, OOM, etc.)
            // We deliberately treat X509_R_CERT_ALREADY_IN_HASH_TABLE (rc=0
            // with that specific error) as success: the cert is already in
            // the store (e.g. from the system default paths or a previous
            // embedded-root call), and "duplicate" is still a valid trust
            // anchor for our purposes.
            if (rc == 1) {
                printf("Bootstrap: TLS trust loaded from embedded %s\n", label.c_str());
                trustLoaded = true;
                return true;
            }
            unsigned long e = ERR_get_error();
            // X509_R_CERT_ALREADY_IN_HASH_TABLE == 101 (OpenSSL x509err.h).
            // We accept the duplicate-cert code as success.
            if (ERR_GET_REASON(e) == X509_R_CERT_ALREADY_IN_HASH_TABLE) {
                ERR_clear_error();
                printf("Bootstrap: TLS trust from embedded %s already present in store\n", label.c_str());
                trustLoaded = true;
                return true;
            }
            char buf[256];
            ERR_error_string_n(e, buf, sizeof(buf));
            lastLoadErr = "embedded " + label + ": " + buf;
            return false;
        };
        // Trust resolution is genuinely additive — each successful source adds
        // anchors to the store, and the embedded ISRG X1 + X2 are always
        // layered on at the end as belt-and-suspenders. Order is:
        //   (a) exedir/cacert.pem     (deploy-time bundle, covers Windows GUI)
        //   (b) SSL_CERT_FILE          (operator override, wide-char on Windows)
        //   (c) system default paths   (Linux daemon: /etc/ssl/certs/...)
        //   (d) embedded ISRG X1 + X2 (always, on top of whatever loaded above)
        //
        // Important: we MUST NOT reject a successful system default-verify-paths
        // call based on a heuristic that counts X509_STORE objects, because
        // OpenSSL's hashed-directory lookups are LAZY (X509_LOOKUP_hashdir
        // installs the lookup but does NOT eagerly preload every cert). On
        // Ubuntu/Debian/alpine, /etc/ssl/certs/ contains symlinks into a
        // hashed dir; the lookup IS valid even though X509_STORE_get0_objects()
        // returns 0 objects until a real cert chain is verified. Rejecting
        // that valid install would force the daemon to fall through to the
        // embedded fallbacks unnecessarily and (worse) could reject the system
        // path on installations that legitimately have a usable CA bundle.
        //
        // The right model is: trust the API's return code as evidence the
        // lookup source is configured; on API failure, treat the system as
        // unconfigured and rely on the embedded fallbacks (which we always
        // attempt). The embedded X1 + X2 are also tried when the system
        // returns 1, as belt-and-suspenders for chains terminating at the
        // cross-sign root (older R3 intermediates chain to X1; the newer LE
        // YE1 intermediate chains to X2).
        if (!exeDir.empty()) {
            tryLoadPath("exedir cacert.pem", exeDir / "cacert.pem");
        }
        // SSL_CERT_FILE: prefer _wgetenv on Windows so a non-ASCII
        // override path works, then load via ReadFileToMemory + memory
        // BIO (avoiding OpenSSL's narrow filename API).
#ifdef WIN32
        wchar_t* envCertW = _wgetenv(L"SSL_CERT_FILE");
        if (envCertW && *envCertW) {
            tryLoadPath("SSL_CERT_FILE", fs::path(std::wstring(envCertW)));
        }
#else
        const char* envCert = std::getenv("SSL_CERT_FILE");
        if (envCert && *envCert) {
            tryLoadFile("SSL_CERT_FILE", envCert);
        }
#endif
        // System default verify paths: trust the API return. We do not
        // introspect the store object count, because lazy hashed-directory
        // lookups install correctly without eager preload.
        int sysRc = SSL_CTX_set_default_verify_paths(ctx);
        if (sysRc == 1) {
            printf("Bootstrap: TLS trust configured from system default paths\n");
            trustLoaded = true;
        } else {
            unsigned long e = ERR_get_error();
            char buf[256];
            ERR_error_string_n(e, buf, sizeof(buf));
            lastLoadErr = std::string("system default paths: ") + buf;
            ERR_clear_error();
        }
        // Belt-and-suspenders: always attempt to add embedded ISRG X1 + X2
        // regardless of whether (a)/(b)/(c) succeeded, because they are
        // additive (X509_STORE_add_cert + CERT_ALREADY_IN_HASH_TABLE both
        // mean "the anchor is addressable"). They are CRITICAL on a stripped
        // install where (a)/(b)/(c) all fail (rc!=1). The check below
        // verifies that on a stripped install, BOTH X1 and X2 are addressable.
        // On an install where (a)/(b)/(c) succeeded, X1/X2 are layered on
        // for cross-sign resilience and any tryLoadEmbedded failure becomes
        // a warning (the chain will still validate via the layered sources).
        auto countAnchors = [&]() -> int {
            X509_STORE* store = SSL_CTX_get_cert_store(ctx);
            if (!store) return 0;
            // X509_STORE_get0_objects() returns STACK_OF(X509_OBJECT) in
            // OpenSSL 1.1+/3.x. We use this for "how many anchors did this
            // source add" only in restricted diagnostic contexts below — NOT
            // to gate the API return value. (Eagerly loaded anchors via
            // SSL_CTX_load_verify_locations will report here; lazy hashed-
            // directory lookups from set_default_verify_paths will not.
            // See the comment block above.)
            STACK_OF(X509_OBJECT)* objs = X509_STORE_get0_objects(store);
            return objs ? sk_X509_OBJECT_num(objs) : 0;
        };
        // Capture whether ANY external source (cacert/SSL_CERT_FILE/system)
        // succeeded BEFORE embedded calls run. tryLoadEmbedded sets trustLoaded
        // and we need this separate signal to distinguish "external sources
        // worked but X2 hard-failed" (chain still validates via external
        // anchors, warning-only) from "all external sources failed AND X2
        // hard-failed" (truly zero usable anchors, fail closed).
        bool externalSourceSucceeded = trustLoaded;
        bool x1Addr = tryLoadEmbedded("ISRG Root X1", EMBEDDED_ISRG_ROOT_X1_PEM);
        ERR_clear_error();
        bool x2Addr = tryLoadEmbedded("ISRG Root X2", EMBEDDED_ISRG_ROOT_X2_PEM);
        ERR_clear_error();
        if (!x1Addr || !x2Addr) {
            // Either X1 or X2 (or both) hard-failed. We only treat this as
            // fatal when no external source succeeded — i.e. the system has
            // truly zero usable trust anchors. If (a)/(b)/(c) succeeded, the
            // chain can still validate via those (e.g. R3 intermediate chains
            // to X1 via a non-ISRG-root cert in /etc/ssl/certs), but we warn.
            if (!externalSourceSucceeded) {
                int anchors = countAnchors();
                if (anchors == 0) {
                    strError = "TLS trust store is empty: cacert/SSL_CERT_FILE/system "
                               "not configured (rc!=1) AND embedded ISRG fallbacks "
                               "failed (X1=" + std::string(x1Addr ? "ok" : "FAIL") +
                               ", X2=" + std::string(x2Addr ? "ok" : "FAIL") +
                               ", last attempt: " + lastLoadErr + ")";
                    return false;
                }
            }
            printf("Bootstrap: WARNING — embedded ISRG load partial: X1=%s X2=%s "
                   "(ca.pem/SSL_CERT_FILE/system are configured, chain will validate via those)\n",
                   x1Addr ? "ok" : "FAIL", x2Addr ? "ok" : "FAIL");
        }
        // trustLoaded true if any external source succeeded OR if X1+X2 were
        // both added/duplicated on a stripped host.
        if (x1Addr && x2Addr) {
            trustLoaded = true;
        }
        if (!trustLoaded) {
            strError = "Failed to load any TLS trust store (last attempt: " + lastLoadErr + ")";
            return false;
        }
        SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, nullptr);

        ssl = SSL_new(ctx);
        if (!ssl) {
            strError = "Failed to create SSL object";
            return false;
        }
        SSL_set_fd(ssl, (int)sock);
        if (SSL_set_tlsext_host_name(ssl, hostname.c_str()) != 1 ||
            SSL_set1_host(ssl, hostname.c_str()) != 1) {
            strError = "Failed to configure TLS hostname verification for " + hostname;
            return false;
        }

        if (SSL_connect(ssl) != 1) {
            unsigned long err = ERR_get_error();
            char errBuf[256];
            ERR_error_string_n(err, errBuf, sizeof(errBuf));
            strError = "TLS handshake failed with " + hostname + ": " + errBuf;
            return false;
        }
        if (SSL_get_verify_result(ssl) != X509_V_OK) {
            strError = "TLS certificate verification failed for " + hostname;
            return false;
        }
        return true;
    }
};

// Parse host, port, and path from an absolute URL.
// Sets useSSL, host, port, path. Returns false for unsupported schemes.
static bool ParseAbsoluteUrl(const std::string& url,
                              bool& useSSL, std::string& host,
                              int& port, std::string& path)
{
    if (url.compare(0, 8, "https://") == 0) {
        useSSL = true;
        std::string rest = url.substr(8);
        size_t pathStart = rest.find('/');
        if (pathStart != std::string::npos) {
            host = rest.substr(0, pathStart);
            path = rest.substr(pathStart);
        } else {
            host = rest;
            path = "/";
        }
        size_t colonPos = host.find(':');
        if (colonPos != std::string::npos) {
            port = std::atoi(host.c_str() + colonPos + 1);
            host = host.substr(0, colonPos);
        } else {
            port = 443;
        }
        return true;
    } else if (url.compare(0, 7, "http://") == 0) {
        useSSL = false;
        std::string rest = url.substr(7);
        size_t pathStart = rest.find('/');
        if (pathStart != std::string::npos) {
            host = rest.substr(0, pathStart);
            path = rest.substr(pathStart);
        } else {
            host = rest;
            path = "/";
        }
        size_t colonPos = host.find(':');
        if (colonPos != std::string::npos) {
            port = std::atoi(host.c_str() + colonPos + 1);
            host = host.substr(0, colonPos);
        } else {
            port = 80;
        }
        return true;
    }
    return false;
}

bool DownloadFile(const std::string& host, const std::string& urlPath,
                  const fs::path& destPath,
                  ProgressCallback progressFn,
                  std::string& strError,
                  bool noProxy,
                  int portOverride,
                  int64_t maxDownloadBytes)
{
    try {
        if (maxDownloadBytes <= 0) {
            strError = "Download size limit must be positive";
            return false;
        }
        std::string currentHost = host;
        std::string currentPath = urlPath;
        int currentPort = (portOverride > 0) ? portOverride : PORT;
        bool useSSL = (currentPort == 443);
        std::string headerData;
        int redirectCount = 0;
        const int MAX_REDIRECTS = 5;

        HttpConn conn;

        // Connection + redirect loop
        while (true) {
            conn.Close(); // clean slate for each attempt

            if (noProxy) {
                conn.sock = ConnectDirectTCP(currentHost, currentPort, strError);
                if (conn.sock == INVALID_SOCKET)
                    return false;
            } else {
                CService addr;
                if (!ConnectSocketByName(addr, conn.sock, currentHost.c_str(), currentPort, 30)) {
                    strError = "Cannot connect to " + currentHost + " (check Tor proxy)";
                    return false;
                }
            }

            // Establish TLS when needed
            if (useSSL) {
                if (!conn.StartTLS(currentHost, strError))
                    return false;
                printf("Bootstrap: TLS established with %s:%d\n",
                       currentHost.c_str(), currentPort);
            }

            // Send HTTP GET request
            std::string request =
                "GET " + currentPath + " HTTP/1.1\r\n"
                "Host: " + currentHost + "\r\n"
                "Connection: close\r\n"
                "User-Agent: Triangles\r\n"
                "\r\n";

            if (!conn.Send(request.data(), request.size())) {
                strError = "Failed to send request to " + currentHost;
                return false;
            }

            // Read response headers
            if (!conn.RecvUntil(headerData, "\r\n\r\n")) {
                strError = "Failed to read HTTP headers from " + currentHost;
                return false;
            }

            // Parse status code from "HTTP/1.x NNN ..."
            unsigned int status_code = 0;
            size_t sp = headerData.find(' ');
            if (sp != std::string::npos)
                status_code = atoi(headerData.c_str() + sp + 1);

            // Handle HTTP redirects
            if (status_code == 301 || status_code == 302 ||
                status_code == 307 || status_code == 308) {

                if (++redirectCount > MAX_REDIRECTS) {
                    strError = "Too many redirects for " + urlPath;
                    return false;
                }

                // Find Location header (case-insensitive)
                std::string lowerHdr = headerData;
                std::transform(lowerHdr.begin(), lowerHdr.end(),
                               lowerHdr.begin(), ::tolower);
                size_t locPos = lowerHdr.find("\nlocation:");
                if (locPos == std::string::npos) {
                    strError = "Redirect " + std::to_string(status_code) + " without Location header";
                    return false;
                }

                size_t valStart = locPos + 10; // skip "\nlocation:"
                while (valStart < headerData.size() && headerData[valStart] == ' ')
                    valStart++;
                size_t lineEnd = headerData.find("\r\n", valStart);
                std::string location;
                if (lineEnd != std::string::npos)
                    location = headerData.substr(valStart, lineEnd - valStart);
                else
                    location = headerData.substr(valStart);
                location = TrimString(location);

                // Parse redirect URL — supports http://, https://, and relative paths
                if (location.compare(0, 7, "http://") == 0 ||
                    location.compare(0, 8, "https://") == 0) {
                    bool redirectUsesSSL = false;
                    std::string redirectHost;
                    std::string redirectPath;
                    int redirectPort = 0;
                    if (!ParseAbsoluteUrl(location, redirectUsesSSL, redirectHost,
                                          redirectPort, redirectPath)) {
                        strError = "Unsupported redirect location: " + location;
                        return false;
                    }
                    if (useSSL && !redirectUsesSSL) {
                        strError = "Refusing HTTPS downgrade redirect to " + location;
                        return false;
                    }
                    useSSL = redirectUsesSSL;
                    currentHost = redirectHost;
                    currentPath = redirectPath;
                    currentPort = redirectPort;
                } else if (!location.empty() && location[0] == '/') {
                    currentPath = location;
                } else {
                    strError = "Unsupported redirect location: " + location;
                    return false;
                }

                printf("Bootstrap: redirect %d -> %s%s%s (port %d)\n",
                       status_code, useSSL ? "https://" : "http://",
                       currentHost.c_str(), currentPath.c_str(), currentPort);
                continue;
            }

            if (status_code != 200) {
                strError = "HTTP error " + std::to_string(status_code) + " for " + currentPath;
                return false;
            }

            break; // Got 200, proceed to download
        }

        // Parse Content-Length
        int64_t content_length = 0;
        std::string lowerHeaders = headerData;
        std::transform(lowerHeaders.begin(), lowerHeaders.end(),
                       lowerHeaders.begin(), ::tolower);
        size_t clPos = lowerHeaders.find("content-length:");
        if (clPos != std::string::npos) {
            size_t valStart = clPos + 15;
            size_t lineEnd = lowerHeaders.find("\r\n", valStart);
            if (lineEnd != std::string::npos)
                content_length = std::stoll(headerData.substr(valStart, lineEnd - valStart));
        }
        if (content_length < 0 || content_length > maxDownloadBytes) {
            strError = "Download response exceeds the configured size limit";
            return false;
        }

        // Open output file (use _wfopen on Windows for non-ASCII path support)
        FILE* file = nullptr;
#ifdef WIN32
        file = _wfopen(destPath.wstring().c_str(), L"wb");
#else
        file = fopen(destPath.string().c_str(), "wb");
#endif
        if (!file) {
            strError = "Cannot create file: " + destPath.string();
            return false;
        }

        // Read body in chunks
        int64_t bytes_written = 0;
        int64_t last_progress = 0;
        char chunk[65536];

        while (true) {
            int n = conn.Recv(chunk, sizeof(chunk));
            if (n < 0) {
                fclose(file);
                fs::remove(destPath);
                strError = "Network error during download";
                return false;
            }
            if (n == 0) break; // EOF

            if (bytes_written > maxDownloadBytes - n) {
                fclose(file);
                fs::remove(destPath);
                strError = "Download response exceeded the configured size limit";
                return false;
            }
            if (fwrite(chunk, 1, n, file) != static_cast<size_t>(n)) {
                fclose(file);
                fs::remove(destPath);
                strError = "Failed writing bootstrap data to disk";
                return false;
            }
            bytes_written += n;

            if (progressFn && (bytes_written - last_progress >= 262144)) {
                last_progress = bytes_written;
                progressFn(bytes_written, content_length);
            }
        }

        // Flush + close the output file. fclose() can still report buffered-write
        // errors that didn't surface from individual fwrite() calls (disk full,
        // network filesystem dropped, etc.); treat that as a failed download.
        if (fflush(file) != 0) {
            int err = ferror(file);
            fclose(file);
            fs::remove(destPath);
            strError = "Failed to flush download to disk (errno=" + std::to_string(err) + ")";
            return false;
        }
        if (fclose(file) != 0) {
            int err = errno;
            fs::remove(destPath);
            strError = "Failed to close downloaded file (errno=" + std::to_string(err) + ")";
            return false;
        }
        // conn destructor handles socket + SSL cleanup

        // Verify download size if Content-Length was provided
        if (content_length > 0 && bytes_written != content_length) {
            fs::remove(destPath);
            strError = "Incomplete download: got " + std::to_string(bytes_written)
                     + " of " + std::to_string(content_length) + " bytes";
            return false;
        }

        return true;

    } catch (std::exception& e) {
        strError = std::string("Download failed: ") + e.what();
        return false;
    }
}




namespace {


// Read an entire file into a string. Empty string on error.
// Uses Bootstrap::ReadFileToMemory (file-scope, _wfopen on Windows) so
// non-ASCII paths are handled correctly on Windows.
std::string ReadFileToString(const fs::path& path)
{
    std::string s;
    if (!ReadFileToMemory(path, s)) return "";
    return s;
}

// Compute the SHA256 of a file, return as lowercase hex string.
// Streams the file in 64 KiB chunks so we never need to hold the whole
// snapshot in memory (snapshots can be up to 4 GiB). Uses _wfopen on
// Windows for non-ASCII path support. Returns empty string on any error
// (open, read, or close failure).
std::string Sha256OfFile(const fs::path& path)
{
    FILE* f = nullptr;
#ifdef WIN32
    f = _wfopen(path.wstring().c_str(), L"rb");
#else
    f = fopen(path.string().c_str(), "rb");
#endif
    if (!f) return "";
    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    unsigned char buf[64 * 1024];
    size_t n;
    bool readOk = true;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
        SHA256_Update(&ctx, buf, n);
    if (ferror(f)) readOk = false;
    if (fclose(f) != 0) readOk = false;
    if (!readOk) return "";
    unsigned char out[SHA256_DIGEST_LENGTH];
    SHA256_Final(out, &ctx);
    static const char hex[] = "0123456789abcdef";
    std::string hexStr(SHA256_DIGEST_LENGTH * 2, '0');
    for (size_t i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        hexStr[2*i]     = hex[(out[i] >> 4) & 0x0F];
        hexStr[2*i + 1] = hex[out[i] & 0x0F];
    }
    return hexStr;
}

} // anonymous namespace

namespace {

bool IsHexString(const std::string& value, size_t expectedLength)
{
    if (value.size() != expectedLength)
        return false;
    for (unsigned char c : value) {
        if (!std::isxdigit(c))
            return false;
    }
    return true;
}

} // anonymous namespace

bool ParseRemoteSnapshotManifest(const std::string& manifestText,
                                 RemoteSnapshot& snapshot,
                                 std::string& strError)
{
    snapshot = RemoteSnapshot{};

    try {
        const nlohmann::json root = nlohmann::json::parse(manifestText);
        if (!root.is_object() || !root.contains("canonical") ||
            !root.contains("files") || !root.contains("chain_tip")) {
            strError = "manifest.json is missing canonical, files, or chain_tip";
            return false;
        }

        snapshot.filename = root.at("canonical").at("snapshot").get<std::string>();
        if (snapshot.filename.empty() || snapshot.filename == "." ||
            snapshot.filename == ".." ||
            snapshot.filename.find('/') != std::string::npos ||
            snapshot.filename.find('\\') != std::string::npos) {
            strError = "manifest snapshot filename must be a plain filename";
            return false;
        }

        const nlohmann::json& files = root.at("files");
        if (!files.is_object() || !files.contains(snapshot.filename)) {
            strError = "canonical snapshot is absent from the files object";
            return false;
        }

        const nlohmann::json& file = files.at(snapshot.filename);
        const std::string type = file.at("type").get<std::string>();
        if (type.rfind("utxo_snapshot", 0) != 0) {
            strError = "canonical file is not a UTXO snapshot";
            return false;
        }

        snapshot.sha256 = file.at("sha256").get<std::string>();
        std::transform(snapshot.sha256.begin(), snapshot.sha256.end(),
                       snapshot.sha256.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        snapshot.height = root.at("chain_tip").at("height").get<int>();
        snapshot.blockHash = root.at("chain_tip").at("blockhash").get<std::string>();
        std::transform(snapshot.blockHash.begin(), snapshot.blockHash.end(),
                       snapshot.blockHash.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (snapshot.height <= 0 || !IsHexString(snapshot.sha256, 64) ||
            !IsHexString(snapshot.blockHash, 64)) {
            strError = "manifest snapshot height or hash fields are invalid";
            return false;
        }
    } catch (const std::exception& e) {
        strError = std::string("invalid manifest.json: ") + e.what();
        return false;
    }

    return true;
}

bool DownloadUtxoSnapshot(const std::string& host,
                          const fs::path& dataDir,
                          ProgressCallback progressFn,
                          std::string& strError)
{
    const bool noProxy = true;

    // manifest.json is discovery metadata, not a trust root. The only accepted
    // snapshot hash is the one compiled into this release for the same height.
    fs::path tmpManifest = dataDir / "manifest.json.tmp";
    if (!DownloadFile(host, std::string(BASE_PATH) + "manifest.json",
                      tmpManifest, nullptr, strError, noProxy,
                      -1, 4 * 1024 * 1024)) {
        fs::remove(tmpManifest);
        return false;
    }

    const std::string manifestText = ReadFileToString(tmpManifest);
    fs::remove(tmpManifest);
    if (manifestText.empty()) {
        strError = "Cannot read downloaded manifest.json";
        return false;
    }

    RemoteSnapshot snapshot;
    if (!ParseRemoteSnapshotManifest(manifestText, snapshot, strError))
        return false;

    const uint256 manifestBlockHash(snapshot.blockHash);
    if (!Checkpoints::IsKnownCheckpoint(snapshot.height, manifestBlockHash)) {
        strError = "Server snapshot tip is not a hardened checkpoint in this release";
        return false;
    }

    uint256 compiledFileHash;
    if (!Checkpoints::GetSnapshotHash(snapshot.height, compiledFileHash)) {
        strError = "Snapshot height " + std::to_string(snapshot.height) +
                   " has no file hash compiled into this release";
        return false;
    }
    const std::string compiledSha256 = compiledFileHash.ToString();
    if (snapshot.sha256 != compiledSha256) {
        strError = "Server snapshot hash does not match the hash compiled into this release";
        return false;
    }

    printf("Bootstrap: manifest selects compiled snapshot %s at height %d (sha256=%s)\n",
           snapshot.filename.c_str(), snapshot.height,
           compiledSha256.substr(0, 16).c_str());

    fs::path tmpPath = dataDir / "utxo-snapshot.bin.tmp";
    std::string urlPath = std::string(BASE_PATH) + snapshot.filename;

    printf("Bootstrap: downloading UTXO snapshot from %s%s...\n", host.c_str(), urlPath.c_str());

    if (!DownloadFile(host, urlPath, tmpPath, progressFn, strError, noProxy)) {
        fs::remove(tmpPath);
        return false;
    }

    const std::string actualSha256 = Sha256OfFile(tmpPath);
    if (actualSha256.empty()) {
        strError = "Cannot read downloaded snapshot for SHA256 verification";
        fs::remove(tmpPath);
        return false;
    }
    if (actualSha256 != compiledSha256) {
        strError = "Snapshot SHA256 does not match the hash compiled into this release";
        fs::remove(tmpPath);
        return false;
    }
    printf("Bootstrap: compiled snapshot SHA256 verified (%s)\n",
           actualSha256.substr(0, 16).c_str());

    printf("Bootstrap: UTXO snapshot downloaded, loading into database...\n");

    // File hash and tip checkpoint are independent gates. The hash commits to
    // the complete serialized UTXO set; the checkpoint commits to chain identity.
    if (!UtxoSnapshot::LoadSnapshot(tmpPath, dataDir, strError, /*requireCheckpoint=*/true)) {
        fs::remove(tmpPath);
        return false;
    }

    fs::remove(tmpPath);
    printf("Bootstrap: UTXO snapshot loaded successfully.\n");
    return true;
}

} // namespace Bootstrap
