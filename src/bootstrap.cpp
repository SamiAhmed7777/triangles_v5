// Copyright (c) 2024 Triangles developers
// Distributed under the MIT/X11 software license

#include "bootstrap.h"
#include "utxosnapshot.h"
#include "txdb.h"

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/algorithm/string.hpp>

#include <zlib.h>

#include "version.h"
#include "uint256.h"
#include "netbase.h"
#include "net.h"

#include <openssl/ssl.h>
#include <openssl/err.h>

#include <fstream>
#include <sstream>
#include <cstdio>
#include <cstring>
#include <cstdlib>

#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#endif

// Forward declarations to avoid pulling in heavy consensus headers
extern bool fTestNet;
namespace Checkpoints { bool IsKnownCheckpoint(int nHeight, const uint256& hash); }

namespace fs = boost::filesystem;

namespace Bootstrap {

bool NeedsBootstrap(const fs::path& dataDir)
{
    return !fs::exists(dataDir / "blk0001.dat");
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
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        hSocket = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (hSocket == INVALID_SOCKET)
            continue;

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
        // Skip cert verification — we verify data integrity via checkpoint hashes
        SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, nullptr);

        ssl = SSL_new(ctx);
        if (!ssl) {
            strError = "Failed to create SSL object";
            return false;
        }
        SSL_set_fd(ssl, (int)sock);
        SSL_set_tlsext_host_name(ssl, hostname.c_str()); // SNI

        if (SSL_connect(ssl) != 1) {
            unsigned long err = ERR_get_error();
            char errBuf[256];
            ERR_error_string_n(err, errBuf, sizeof(errBuf));
            strError = "TLS handshake failed with " + hostname + ": " + errBuf;
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
                  int portOverride)
{
    try {
        std::string currentHost = host;
        std::string currentPath = urlPath;
        int currentPort = (portOverride > 0) ? portOverride : PORT;
        bool useSSL = false;
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
                boost::trim(location);

                // Parse redirect URL — supports http://, https://, and relative paths
                if (location.compare(0, 7, "http://") == 0 ||
                    location.compare(0, 8, "https://") == 0) {
                    if (!ParseAbsoluteUrl(location, useSSL, currentHost,
                                          currentPort, currentPath)) {
                        strError = "Unsupported redirect location: " + location;
                        return false;
                    }
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

            fwrite(chunk, 1, n, file);
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
    if (!DownloadFile(host, urlPath, tmpPath, nullptr, strError, noProxy))
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
        boost::trim(line);
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

namespace {

// Parse a tar octal field (ASCII octal, null/space terminated)
static int64_t ParseTarOctal(const char* field, size_t len)
{
    int64_t result = 0;
    for (size_t i = 0; i < len && field[i] != '\0' && field[i] != ' '; i++) {
        if (field[i] < '0' || field[i] > '7') continue;
        result = (result << 3) | (field[i] - '0');
    }
    return result;
}

// Extract a tar.gz file to a destination directory
static bool ExtractTarGz(const fs::path& tarGzPath,
                          const fs::path& destDir,
                          std::string& strError)
{
    gzFile gz = gzopen(tarGzPath.string().c_str(), "rb");
    if (!gz) {
        strError = "Cannot open " + tarGzPath.string();
        return false;
    }

    gzbuffer(gz, 262144); // 256 KB buffer for performance

    char header[512];

    while (true) {
        int bytesRead = gzread(gz, header, 512);
        if (bytesRead == 0) break; // EOF
        if (bytesRead != 512) {
            strError = "Truncated tar header";
            gzclose(gz);
            return false;
        }

        // End-of-archive marker (zero block)
        bool allZero = true;
        for (int i = 0; i < 512; i++) {
            if (header[i] != 0) { allZero = false; break; }
        }
        if (allZero) break;

        // Parse filename: name (offset 0, 100 bytes) + optional prefix (offset 345, 155 bytes)
        char name[101] = {0};
        char prefix[156] = {0};
        memcpy(name, header, 100);
        memcpy(prefix, header + 345, 155);

        std::string fullName;
        if (prefix[0] != '\0')
            fullName = std::string(prefix) + "/" + std::string(name);
        else
            fullName = std::string(name);

        // Security: reject absolute paths and path traversal
        if (fullName.empty() || fullName[0] == '/' || fullName.find("..") != std::string::npos) {
            strError = "Unsafe path in tar archive: " + fullName;
            gzclose(gz);
            return false;
        }

        char typeflag = header[156];
        int64_t fileSize = ParseTarOctal(header + 124, 12);

        if (typeflag == '5' || (!fullName.empty() && fullName.back() == '/')) {
            // Directory entry
            fs::create_directories(destDir / fullName);
        } else if (typeflag == '0' || typeflag == '\0') {
            // Regular file
            fs::path filePath = destDir / fullName;
            fs::create_directories(filePath.parent_path());

            FILE* outFile = fopen(filePath.string().c_str(), "wb");
            if (!outFile) {
                strError = "Cannot create file: " + filePath.string();
                gzclose(gz);
                return false;
            }

            int64_t remaining = fileSize;
            char buf[65536];
            while (remaining > 0) {
                int toRead = (remaining > (int64_t)sizeof(buf)) ? (int)sizeof(buf) : (int)remaining;
                int n = gzread(gz, buf, toRead);
                if (n <= 0) {
                    fclose(outFile);
                    strError = "Truncated tar data for: " + fullName;
                    gzclose(gz);
                    return false;
                }
                fwrite(buf, 1, n, outFile);
                remaining -= n;
            }
            fclose(outFile);

            // Skip padding to next 512-byte boundary
            int64_t pad = (512 - (fileSize % 512)) % 512;
            if (pad > 0) {
                char padBuf[512];
                if (gzread(gz, padBuf, (unsigned)pad) != (int)pad) {
                    strError = "Truncated tar padding for: " + fullName;
                    gzclose(gz);
                    return false;
                }
            }
        } else {
            // Unknown entry type - skip its data
            int64_t totalSkip = fileSize + ((512 - (fileSize % 512)) % 512);
            char skipBuf[512];
            while (totalSkip > 0) {
                int toRead = (totalSkip > 512) ? 512 : (int)totalSkip;
                if (gzread(gz, skipBuf, toRead) != toRead) break;
                totalSkip -= toRead;
            }
        }
    }

    gzclose(gz);
    return true;
}

} // anonymous namespace

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
        boost::trim(line);
        if (line.empty() || line[0] == '#')
            continue;

        size_t eq = line.find('=');
        if (eq == std::string::npos)
            continue;

        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);
        boost::trim(key);
        boost::trim(val);

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

    return true;
}

bool DownloadBootstrap(const std::string& host,
                       const fs::path& dataDir,
                       ProgressCallback progressFn,
                       std::string& strError)
{
    bool gotBlockFile = false;

    // Try downloading bootstrap.tar.gz first
    // Bootstrap server is on clearnet — bypass Tor proxy for DNS + HTTP
    const bool noProxy = true;
    fs::path tmpTarGz = dataDir / "bootstrap.tar.gz.tmp";
    std::string tarUrl = std::string(BASE_PATH) + "triangles-bootstrap.tar.gz";

    printf("DownloadBootstrap(): attempting tar.gz download from %s%s\n", host.c_str(), tarUrl.c_str());
    bool tarDownloaded = DownloadFile(host, tarUrl, tmpTarGz, progressFn, strError, noProxy);
    printf("DownloadBootstrap(): tarDownloaded=%d result=%s\n", tarDownloaded, strError.c_str());

    if (tarDownloaded) {
        bool extractOk = ExtractTarGz(tmpTarGz, dataDir, strError);
        fs::remove(tmpTarGz);

        if (extractOk && fs::exists(dataDir / "blk0001.dat"))
            gotBlockFile = true;
        // If extraction failed, fall through to legacy path
    }

    if (!gotBlockFile) {
        // Fallback: try filelist.txt + individual file downloads
        std::string fallbackError;
        std::vector<std::string> files;
        if (!FetchFileList(host, files, fallbackError, noProxy)) {
            if (!tarDownloaded)
                strError = strError + " (fallback also failed: " + fallbackError + ")";
            else
                strError = "Extraction failed: " + strError + " (fallback also failed: " + fallbackError + ")";
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
    // multi-hour FastImportBlockFile() rebuild.
    fs::path chainDbPath = dataDir / GetActiveChainDbDirName();
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
        // FastImportBlockFile() will rebuild from blk0001.dat on next startup.
        printf("Bootstrap: removing extracted %s/ (will rebuild index from blk0001.dat)\n", GetActiveChainDbDirName());
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
}

bool DownloadUtxoSnapshot(const std::string& host,
                          const fs::path& dataDir,
                          ProgressCallback progressFn,
                          std::string& strError)
{
    const bool noProxy = true;
    const char* snapshotFilename = "utxo-snapshot.bin";

    // Download utxo-snapshot.bin to a temp file
    fs::path tmpPath = dataDir / "utxo-snapshot.bin.tmp";
    std::string urlPath = std::string(BASE_PATH) + snapshotFilename;

    printf("Bootstrap: downloading UTXO snapshot from %s%s...\n", host.c_str(), urlPath.c_str());

    if (!DownloadFile(host, urlPath, tmpPath, progressFn, strError, noProxy)) {
        fs::remove(tmpPath);
        return false;
    }

    printf("Bootstrap: UTXO snapshot downloaded, loading into database...\n");

    // Load the snapshot into a fresh active chain DB
    if (!UtxoSnapshot::LoadSnapshot(tmpPath, dataDir, strError)) {
        fs::remove(tmpPath);
        return false;
    }

    // Clean up the temp file
    fs::remove(tmpPath);

    printf("Bootstrap: UTXO snapshot loaded successfully.\n");
    return true;
}

} // namespace Bootstrap
