// Copyright (c) 2024 Triangles developers
// Distributed under the MIT/X11 software license

#include "bootstrap.h"
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

#include "key.h"
#include "base58.h"
#include "util.h"
#include "json/nlohmann_json.hpp"

extern const std::string strMessageMagic;

#include <fstream>
#include <sstream>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cctype>

#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
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
            int n = ssl ? SSL_write(ssl, data, (int)std::min(len, (size_t)65536))
                        : send(sock, data, (int)std::min(len, (size_t)65536), MSG_NOSIGNAL);
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
        if (SSL_CTX_set_default_verify_paths(ctx) != 1) {
            strError = "Failed to load the system TLS trust store";
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

        // Open output file
        FILE* file = fopen(destPath.string().c_str(), "wb");
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

        fclose(file);
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

bool FetchFileList(const std::string& host,
                   std::vector<std::string>& files,
                   std::string& strError,
                   bool noProxy)
{
    // Download filelist.txt to a temp file
    fs::path tmpPath = fs::temp_directory_path() / "triangles_bootstrap_filelist.txt";

    std::string urlPath = std::string(BASE_PATH) + "filelist.txt";
    if (!DownloadFile(host, urlPath, tmpPath, nullptr, strError, noProxy,
                      -1, 1024 * 1024))
        return false;

    // Read lines
    std::ifstream in(tmpPath.string().c_str());
    if (!in.is_open()) {
        strError = "Cannot read downloaded file list";
        return false;
    }

    files.clear();
    std::string line;
    while (std::getline(in, line)) {
        line = TrimString(line);
        if (!line.empty() && line[0] != '#')
            files.push_back(line);
    }
    in.close();
    fs::remove(tmpPath);

    if (files.empty()) {
        strError = "File list is empty";
        return false;
    }

    return true;
}

// --- tar.gz bootstrap support ---

bool ParseManifest(const fs::path& manifestPath,
                   SnapshotManifest& manifest,
                   std::string& strError)
{
    std::ifstream in(manifestPath.string().c_str());
    if (!in.is_open()) {
        strError = "Cannot open " + manifestPath.string();
        return false;
    }

    manifest.format = 0;
    manifest.network.clear();
    manifest.height = -1;
    manifest.hash.clear();
    manifest.dbversion = 0;

    std::string line;
    while (std::getline(in, line)) {
        line = TrimString(line);
        if (line.empty() || line[0] == '#')
            continue;

        size_t eq = line.find('=');
        if (eq == std::string::npos)
            continue;

        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);
        key = TrimString(key);
        val = TrimString(val);

        if (key == "format")
            manifest.format = std::atoi(val.c_str());
        else if (key == "network")
            manifest.network = val;
        else if (key == "height")
            manifest.height = std::atoi(val.c_str());
        else if (key == "hash")
            manifest.hash = val;
        else if (key == "dbversion")
            manifest.dbversion = std::atoi(val.c_str());
        else if (key == "signature")
            manifest.signature = val;
    }
    in.close();

    if (manifest.format == 0) {
        strError = "Manifest missing 'format' field";
        return false;
    }
    if (manifest.network.empty()) {
        strError = "Manifest missing 'network' field";
        return false;
    }
    if (manifest.height < 0) {
        strError = "Manifest missing or invalid 'height' field";
        return false;
    }
    if (manifest.hash.empty()) {
        strError = "Manifest missing 'hash' field";
        return false;
    }
    if (manifest.dbversion == 0) {
        strError = "Manifest missing 'dbversion' field";
        return false;
    }

    return true;
}

bool VerifyManifest(const SnapshotManifest& manifest,
                    std::string& strError)
{
    if (manifest.format != 1) {
        strError = "Unsupported manifest format: " + std::to_string(manifest.format);
        return false;
    }

    std::string expectedNetwork = fTestNet ? "test" : "main";
    if (manifest.network != expectedNetwork) {
        strError = "Network mismatch: manifest says '" + manifest.network
                 + "', expected '" + expectedNetwork + "'";
        return false;
    }

    if (manifest.dbversion != DATABASE_VERSION) {
        strError = "DB version mismatch: manifest says "
                 + std::to_string(manifest.dbversion)
                 + ", binary expects " + std::to_string(DATABASE_VERSION);
        return false;
    }

    uint256 manifestHash(manifest.hash);
    if (manifestHash == 0) {
        strError = "Invalid hash in manifest: " + manifest.hash;
        return false;
    }

    if (!Checkpoints::IsKnownCheckpoint(manifest.height, manifestHash)) {
        strError = "Height " + std::to_string(manifest.height)
                 + " / hash " + manifest.hash
                 + " is not a known checkpoint";
        return false;
    }

    // ─── Signature verification (#11) ─────────────────────────────────────
    // Legacy pre-built indexes are never accepted without authentication.
    // This format is disabled below, but keep its verifier fail-closed so a
    // future caller cannot silently revive the old trust behavior.
    if (!manifest.signature.empty()) {
        // Build the message that was signed: "height||hash" (ASCII)
        std::string message = std::to_string(manifest.height) + "||" + manifest.hash;

        // Decode the hex-encoded signature (64 bytes for Ed25519)
        std::vector<unsigned char> sigBytes;
        if (manifest.signature.size() != 128) {  // 64 bytes hex = 128 chars
            strError = "Invalid signature length in manifest (expected 128 hex chars, got "
                     + std::to_string(manifest.signature.size()) + ")";
            return false;
        }
        for (size_t i = 0; i < manifest.signature.size(); i += 2) {
            auto hexVal = [](char c) -> int {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                return -1;
            };
            int hi = hexVal(manifest.signature[i]);
            int lo = hexVal(manifest.signature[i + 1]);
            if (hi < 0 || lo < 0) {
                strError = "Invalid hex in manifest signature";
                return false;
            }
            sigBytes.push_back((hi << 4) | lo);
        }

        // Snapshot signing public key (Ed25519, 32 bytes).
        // This is the public half of the key used to sign snapshots on the
        // bootstrap server. The private key never leaves the build machine.
        // To rotate: generate new keypair, update this constant, re-sign
        // all snapshots, update manifest files.
        static const unsigned char snapshotPubkey[32] = {
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
        };  // Placeholder: replace with actual pubkey when signing is deployed

        // Use OpenSSL Ed25519 verification
        EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
        if (!mdctx) {
            strError = "Failed to allocate EVP context for signature verification";
            return false;
        }

        EVP_PKEY* pkey = EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, nullptr,
                                                      snapshotPubkey, 32);
        if (!pkey) {
            EVP_MD_CTX_free(mdctx);
            strError = "Failed to load snapshot signing public key";
            return false;
        }

        int rc = EVP_DigestVerifyInit(mdctx, nullptr, nullptr, nullptr, pkey);
        if (rc != 1) {
            EVP_PKEY_free(pkey);
            EVP_MD_CTX_free(mdctx);
            strError = "Failed to init signature verification";
            return false;
        }

        rc = EVP_DigestVerify(mdctx,
                              sigBytes.data(), sigBytes.size(),
                              (const unsigned char*)message.data(), message.size());

        EVP_PKEY_free(pkey);
        EVP_MD_CTX_free(mdctx);

        if (rc == 1) {
            printf("Snapshot manifest signature VERIFIED\n");
        } else if (rc == 0) {
            strError = "Snapshot manifest signature INVALID — possible tampering detected";
            return false;
        } else {
            strError = "Snapshot manifest signature verification error";
            return false;
        }
    } else {
        strError = "Snapshot manifest has no signature";
        return false;
    }

    return true;
}

bool DownloadBootstrap(const std::string& host,
                       const fs::path& dataDir,
                       ProgressCallback progressFn,
                       std::string& strError)
{
    (void)host;
    (void)dataDir;
    (void)progressFn;
    strError = "Legacy file-list bootstrap is disabled; use a compiled-hash UTXO snapshot or sync from genesis";
    return false;

#if 0
    bool gotBlockFile = false;

    // FastImport removed (commit bdb7253). v2 UTXO snapshot is the ONLY
    // supported sync path. Skip the legacy tarball fallback entirely so we
    // never hit /triangles-bootstrap.tar.gz (404 since 2026-06-19 cleanup)
    // or /tri-bootstrap.tar.gz (also gone; was the URL in the old filelist.txt).
    // The remaining path below reads filelist.txt → downloads utxo-snapshot.bin.
    const bool noProxy = true;

    if (!gotBlockFile) {
        // Try filelist.txt — should contain only utxo-snapshot.bin (v2).
        std::string fallbackError;
        std::vector<std::string> files;
        if (!FetchFileList(host, files, fallbackError, noProxy)) {
            strError = "filelist.txt unavailable: " + fallbackError;
            return false;
        }

        for (size_t i = 0; i < files.size(); i++) {
            fs::path destPath = dataDir / files[i];
            fs::create_directories(destPath.parent_path());

            std::string urlPath = std::string(BASE_PATH) + files[i];
            if (!DownloadFile(host, urlPath, destPath, progressFn, strError, noProxy))
                return false;
        }

        gotBlockFile = fs::exists(dataDir / "blk0001.dat");
    }

    if (!gotBlockFile) {
        strError = "No blk0001.dat after download";
        return false;
    }

    // Check if the archive included a trusted pre-built index for the active
    // backend with a valid snapshot.manifest. If verified, keep it to skip the
    // multi-hour rebuild (fast-import removed; UTXO snapshot is the only sync path).
    fs::path chainDbPath = GetChainDataDir();
    fs::path database  = dataDir / "database";
    fs::path manifestPath = dataDir / "snapshot.manifest";

    bool keepIndex = false;

    if (fs::exists(manifestPath) && fs::exists(chainDbPath)) {
        SnapshotManifest manifest;
        std::string manifestError;

        if (ParseManifest(manifestPath, manifest, manifestError)) {
            printf("Bootstrap: snapshot.manifest found (format=%d, network=%s, "
                   "height=%d, dbversion=%d)\n",
                   manifest.format, manifest.network.c_str(),
                   manifest.height, manifest.dbversion);

            if (VerifyManifest(manifest, manifestError)) {
                printf("Bootstrap: manifest verified - keeping pre-built index "
                       "(height %d, checkpoint match)\n", manifest.height);
                keepIndex = true;
            } else {
                printf("Bootstrap: manifest verification failed: %s\n",
                       manifestError.c_str());
            }
        } else {
            printf("Bootstrap: cannot parse snapshot.manifest: %s\n",
                   manifestError.c_str());
        }
    }

    if (!keepIndex) {
        // No valid manifest or verification failed - delete the index.
        // The block index will be rebuilt from the UTXO snapshot on next startup.
        printf("Bootstrap: removing extracted %s/ (will rebuild index from blk0001.dat)\n",
               GetChainDataDir().filename().string().c_str());
        if (fs::exists(chainDbPath))
            fs::remove_all(chainDbPath);
    }

    // Always remove BDB database/ dir (wallet environment from another machine)
    if (fs::exists(database))
        fs::remove_all(database);

    // Clean up manifest file (not needed after verification)
    if (fs::exists(manifestPath))
        fs::remove(manifestPath);

    return true;
#endif
}

#if 0
namespace {

// Try to find the canonical UTXO snapshot entry in the bootstrap server's
// manifest.json. Looks for an entry of type "utxo_snapshot" and extracts
// its filename + expected SHA256. Returns true on success.
//
// We deliberately do a simple substring scan rather than full JSON parsing:
// the manifest is operator-controlled, the format is stable, and adding a
// JSON dependency for ~50 lines of code isn't worth it.
//
// On failure, the caller falls back to the legacy "utxo-snapshot.bin" URL,
// which the bootstrap server symlinks to the canonical file.
// Trusted signer addresses for snapshot manifests. A snapshot is accepted
// iff its manifest's signing_address matches one of these AND its signature
// verifies under Triangles' compact-message protocol.
//
// Design A: single-slot runtime override via RPC. The previous publisher
// is dropped atomically on every set. The built-in fallback below is
// always consulted if no runtime override is set, so a fresh daemon still
// verifies old snapshots without operator intervention.

// Built-in fallback (read-only, compiled in).
static const char* BUILTIN_TRUSTED_SNAPSHOT_SIGNERS[] = {
    "TG8f76yktTxDrT7JJymY3wVAusXiD3fVvX",  // Sami's legacy snapshot publisher key
};
static const size_t NUM_BUILTIN_TRUSTED_SNAPSHOT_SIGNERS =
    sizeof(BUILTIN_TRUSTED_SNAPSHOT_SIGNERS) / sizeof(BUILTIN_TRUSTED_SNAPSHOT_SIGNERS[0]);

// Runtime override. Empty string = no override, use built-in fallback.
static std::string g_activeTrustedSnapshotPublisher;
static std::mutex g_trustedPublisherMutex;
static const char* SNAPSHOT_PUBLISHER_FILE = "snapshot-publisher.json";

} // anonymous namespace (helpers above are file-private)

// PUBLIC API — declared in bootstrap.h inside namespace Bootstrap.
// These MUST NOT be inside an anonymous namespace or the linker can't
// resolve Bootstrap::GetActiveTrustedSnapshotPublisher calls from
// rpcblockchain.cpp / init.cpp. (PR #26 bug: left the anon-namespace
// open across these definitions.)

std::string GetActiveTrustedSnapshotPublisher()
{
    std::lock_guard<std::mutex> lock(g_trustedPublisherMutex);
    return g_activeTrustedSnapshotPublisher;
}

static void SetActiveTrustedSnapshotPublisherUnlocked(const std::string& addr)
{
    g_activeTrustedSnapshotPublisher = addr;
}

// Load runtime override from <datadir>/snapshot-publisher.json.
// Called once at startup from init.cpp.
void LoadTrustedSnapshotPublisher()
{
    fs::path filePath = GetDataDir(true) / SNAPSHOT_PUBLISHER_FILE;
    if (!fs::exists(filePath))
        return;

    std::ifstream f(filePath.string().c_str());
    if (!f) return;

    std::stringstream ss; ss << f.rdbuf();
    std::string json = ss.str();

    // Minimal JSON parse: "address":"<addr>"
    size_t keyPos = json.find("\"address\"");
    if (keyPos == std::string::npos) return;
    size_t colonPos = json.find(':', keyPos);
    if (colonPos == std::string::npos) return;
    size_t q1 = json.find('"', colonPos);
    if (q1 == std::string::npos) return;
    size_t q2 = json.find('"', q1 + 1);
    if (q2 == std::string::npos) return;

    std::string addr = json.substr(q1 + 1, q2 - q1 - 1);
    if (addr.size() != 34 || addr[0] != 'T') {
        printf("Bootstrap: snapshot-publisher.json contains invalid address '%s', ignoring\n",
               addr.c_str());
        return;
    }

    {
        std::lock_guard<std::mutex> lock(g_trustedPublisherMutex);
        SetActiveTrustedSnapshotPublisherUnlocked(addr);
    }
    printf("Bootstrap: loaded trusted snapshot publisher override: %s\n", addr.c_str());
}

static bool PersistTrustedSnapshotPublisher(const std::string& addr)
{
    fs::path filePath = GetDataDir(true) / SNAPSHOT_PUBLISHER_FILE;
    std::ofstream f(filePath.string().c_str(), std::ios::trunc);
    if (!f) return false;
    f << "{\n"
      << "  \"address\": \"" << addr << "\",\n"
      << "  \"set_at\": " << GetTime() << ",\n"
      << "  \"note\": \"Set via triangles-cli settrustedv2snapshotpublisher. "
      << "Replace atomically; previous publisher is dropped.\"\n"
      << "}\n";
    return f.good();
}

bool SetTrustedSnapshotPublisher(const std::string& addr, std::string& strError)
{
    if (addr.size() != 34 || addr[0] != 'T') {
        strError = "settrustedv2snapshotpublisher: invalid address format (expected 34-char T-address)";
        return false;
    }
    {
        std::lock_guard<std::mutex> lock(g_trustedPublisherMutex);
        SetActiveTrustedSnapshotPublisherUnlocked(addr);
    }
    if (!PersistTrustedSnapshotPublisher(addr)) {
        strError = "settrustedv2snapshotpublisher: warning, could not persist to "
                   "snapshot-publisher.json (in-memory change is live for this session)";
        return true;
    }
    return true;
}

bool UnsetTrustedSnapshotPublisher(std::string& strError)
{
    {
        std::lock_guard<std::mutex> lock(g_trustedPublisherMutex);
        SetActiveTrustedSnapshotPublisherUnlocked(std::string());
    }
    fs::path filePath = GetDataDir(true) / SNAPSHOT_PUBLISHER_FILE;
    fs::remove(filePath);
    return true;
}
#endif

// Re-enter anonymous namespace for the remaining file-private helpers.
// (IsTrustedSnapshotSigner / VerifySignedMessage / ExtractJsonString are
// not declared in bootstrap.h, so they don't need Bootstrap:: linkage.)

namespace {

#if 0
bool IsTrustedSnapshotSigner(const std::string& addr)
{
    // 1. Runtime override (set via RPC).
    {
        std::lock_guard<std::mutex> lock(g_trustedPublisherMutex);
        if (!g_activeTrustedSnapshotPublisher.empty() &&
            addr == g_activeTrustedSnapshotPublisher)
            return true;
    }
    // 2. Built-in fallback (compiled in, read-only).
    for (size_t i = 0; i < NUM_BUILTIN_TRUSTED_SNAPSHOT_SIGNERS; ++i)
        if (addr == BUILTIN_TRUSTED_SNAPSHOT_SIGNERS[i])
            return true;
    return false;
}

// Verify a Triangles signed-message compact signature. Returns true iff:
//   - The address is valid
//   - The signature is valid base64
//   - The compact signature recovers to a public key whose hash160 matches
//     the address's keyID
//   - The hash being verified is Hash(strMessageMagic || message)
//
// Mirrors verifymessage RPC. Caller separately checks trust.
bool VerifySignedMessage(const std::string& strAddress,
                         const std::string& strSignatureB64,
                         const std::string& strMessage,
                         std::string& strError)
{
    CTrianglesAddress addr(strAddress);
    if (!addr.IsValid()) {
        strError = "Invalid signer address: " + strAddress;
        return false;
    }
    CKeyID keyID;
    if (!addr.GetKeyID(keyID)) {
        strError = "Address does not refer to a key: " + strAddress;
        return false;
    }

    bool fInvalid = false;
    std::vector<unsigned char> vchSig = DecodeBase64(strSignatureB64.c_str(), &fInvalid);
    if (fInvalid) {
        strError = "Malformed base64 in signature";
        return false;
    }

    CDataStream ss(SER_GETHASH, 0);
    ss << strMessageMagic;
    ss << strMessage;

    CKey key;
    if (!key.SetCompactSignature(Hash(ss.begin(), ss.end()), vchSig)) {
        strError = "Signature does not verify (recovered key mismatch or malformed sig)";
        return false;
    }
    if (key.GetPubKey().GetID() != keyID) {
        strError = "Signature recovered to a different key than the claimed signer";
        return false;
    }
    return true;
}

// Extract a string field value from a small JSON object (subset).
std::string ExtractJsonString(const std::string& json, const std::string& field)
{
    std::string key = "\"" + field + "\"";
    size_t pos = json.find(key);
    if (pos == std::string::npos) return "";
    pos += key.size();
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == ':' || json[pos] == '\t'))
        pos++;
    if (pos >= json.size() || json[pos] != '\"') return "";
    pos++;
    size_t end = json.find('\"', pos);
    if (end == std::string::npos) return "";
    return json.substr(pos, end - pos);
}

bool FindCanonicalSnapshotInManifest(const std::string& manifestText,
                                     std::string& outFilename,
                                     std::string& outSha256,
                                     std::string& outManifestFilename,
                                     std::string& strError)
{
    // Look for the "utxo_snapshot" file entry, e.g.:
    //   "utxo-snapshot-2207680.utx": {
    //     ...
    //     "type": "utxo_snapshot",
    //     "sha256": "eeefe107...",
    //     ...
    //   }
    size_t typePos = manifestText.find("\"utxo_snapshot\"");
    if (typePos == std::string::npos) {
        strError = "manifest.json has no utxo_snapshot entry";
        return false;
    }

    // Walk backwards from the typePos to find the start of this file's block.
    // Format: "filename": { ... "type": "utxo_snapshot" ...
    // We scan for the nearest preceding '"' followed by ':' that introduces a
    // top-level file entry. Simple heuristic: find the line containing the
    // type marker, then search backwards for the file key.
    size_t entryStart = manifestText.rfind('"', typePos);
    if (entryStart == std::string::npos || entryStart == 0) {
        strError = "malformed manifest.json (no filename before utxo_snapshot entry)";
        return false;
    }
    // Skip the opening quote
    size_t filenameStart = entryStart + 1;
    size_t filenameEnd = manifestText.find('"', filenameStart);
    if (filenameEnd == std::string::npos) {
        strError = "malformed manifest.json (unterminated filename)";
        return false;
    }
    outFilename = manifestText.substr(filenameStart, filenameEnd - filenameStart);

    // Within this block, extract the sha256.
    // Walk forward from the typePos to find the matching closing brace of the
    // entry. (Manifest is shallow, so a naive brace-count is fine.)
    size_t braceStart = manifestText.find('{', filenameEnd);
    if (braceStart == std::string::npos) {
        strError = "malformed manifest.json (no body after filename)";
        return false;
    }
    int depth = 0;
    size_t bodyEnd = braceStart;
    for (size_t i = braceStart; i < manifestText.size(); ++i) {
        if (manifestText[i] == '{') depth++;
        else if (manifestText[i] == '}') {
            depth--;
            if (depth == 0) { bodyEnd = i; break; }
        }
    }
    if (depth != 0) {
        strError = "malformed manifest.json (unbalanced braces in entry)";
        return false;
    }
    std::string entry = manifestText.substr(braceStart, bodyEnd - braceStart);

    size_t shaPos = entry.find("\"sha256\"");
    if (shaPos == std::string::npos) {
        strError = "manifest entry has no sha256 field";
        return false;
    }
    size_t valStart = entry.find('"', shaPos + 8);
    if (valStart == std::string::npos) {
        strError = "malformed manifest.json (no sha256 value)";
        return false;
    }
    valStart++;
    size_t valEnd = entry.find('"', valStart);
    if (valEnd == std::string::npos) {
        strError = "malformed manifest.json (unterminated sha256 value)";
        return false;
    }
    outSha256 = entry.substr(valStart, valEnd - valStart);

    // Extract manifest filename (optional).
    outManifestFilename.clear();
    size_t manPos = entry.find("\"manifest\"");
    if (manPos != std::string::npos) {
        size_t mvStart = entry.find('\"', manPos + 10);
        if (mvStart != std::string::npos) {
            mvStart++;
            size_t mvEnd = entry.find('\"', mvStart);
            if (mvEnd != std::string::npos)
                outManifestFilename = entry.substr(mvStart, mvEnd - mvStart);
        }
    }

    return true;
}
#endif

// Read an entire file into a string. Empty string on error.
std::string ReadFileToString(const fs::path& path)
{
    FILE* f = fopen(path.string().c_str(), "rb");
    if (!f) return "";
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return ""; }
    fseek(f, 0, SEEK_SET);
    std::string s(sz, '\0');
    size_t nread = fread(&s[0], 1, sz, f);
    s.resize(nread);
    fclose(f);
    return s;
}

// Compute the SHA256 of a file, return as lowercase hex string.
std::string Sha256OfFile(const fs::path& path)
{
    FILE* f = fopen(path.string().c_str(), "rb");
    if (!f) return "";
    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    unsigned char buf[64 * 1024];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
        SHA256_Update(&ctx, buf, n);
    fclose(f);
    unsigned char out[SHA256_DIGEST_LENGTH];
    SHA256_Final(out, &ctx);
    static const char hex[] = "0123456789abcdef";
    std::string s(SHA256_DIGEST_LENGTH * 2, '0');
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        s[2*i]     = hex[(out[i] >> 4) & 0xF];
        s[2*i + 1] = hex[out[i] & 0xF];
    }
    return s;
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
