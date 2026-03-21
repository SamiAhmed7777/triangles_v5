// Copyright (c) 2024 Triangles developers
// Distributed under the MIT/X11 software license

#include "bootstrap.h"

#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/algorithm/string.hpp>

#include <zlib.h>

#include <fstream>
#include <sstream>
#include <cstdio>
#include <cstring>

namespace fs = boost::filesystem;
using boost::asio::ip::tcp;

namespace Bootstrap {

bool NeedsBootstrap(const fs::path& dataDir)
{
    return !fs::exists(dataDir / "blk0001.dat");
}

bool DownloadFile(const std::string& host, const std::string& urlPath,
                  const fs::path& destPath,
                  ProgressCallback progressFn,
                  std::string& strError)
{
    try {
        boost::asio::io_context io_context;
        tcp::resolver resolver(io_context);

        boost::system::error_code resolve_ec;
        tcp::resolver::results_type endpoints =
            resolver.resolve(host, std::to_string(PORT), resolve_ec);
        if (resolve_ec) {
            strError = "Cannot resolve host: " + host;
            return false;
        }

        tcp::socket socket(io_context);
        boost::asio::connect(socket, endpoints);

        // Send HTTP GET request
        std::string request =
            "GET " + urlPath + " HTTP/1.1\r\n"
            "Host: " + host + "\r\n"
            "Connection: close\r\n"
            "User-Agent: Triangles\r\n"
            "\r\n";
        boost::asio::write(socket, boost::asio::buffer(request));

        // Read response headers
        boost::asio::streambuf response_buf;
        boost::asio::read_until(socket, response_buf, "\r\n\r\n");

        std::istream response_stream(&response_buf);

        // Parse status line
        std::string http_version;
        unsigned int status_code = 0;
        response_stream >> http_version >> status_code;
        std::string status_message;
        std::getline(response_stream, status_message);

        if (status_code != 200) {
            strError = "HTTP error " + std::to_string(status_code) + " for " + urlPath;
            return false;
        }

        // Parse headers for Content-Length
        int64_t content_length = 0;
        std::string header_line;
        while (std::getline(response_stream, header_line) && header_line != "\r") {
            std::string lower_header = header_line;
            std::transform(lower_header.begin(), lower_header.end(),
                           lower_header.begin(), ::tolower);
            if (lower_header.find("content-length:") == 0) {
                content_length = std::stoll(header_line.substr(header_line.find(':') + 1));
            }
        }

        // Open output file
        FILE* file = fopen(destPath.string().c_str(), "wb");
        if (!file) {
            strError = "Cannot create file: " + destPath.string();
            return false;
        }

        int64_t bytes_written = 0;

        // Write any data remaining in the header buffer (body starts here)
        if (response_buf.size() > 0) {
            std::istreambuf_iterator<char> eos;
            std::string remaining(std::istreambuf_iterator<char>(response_stream), eos);
            if (!remaining.empty()) {
                fwrite(remaining.data(), 1, remaining.size(), file);
                bytes_written += remaining.size();
            }
        }

        // Read remaining body in chunks
        std::vector<char> chunk(65536); // 64 KB
        boost::system::error_code ec;
        int64_t last_progress = 0;

        while (true) {
            size_t n = socket.read_some(boost::asio::buffer(chunk), ec);
            if (n > 0) {
                fwrite(chunk.data(), 1, n, file);
                bytes_written += n;

                // Report progress every 256 KB
                if (progressFn && (bytes_written - last_progress >= 262144)) {
                    last_progress = bytes_written;
                    progressFn(bytes_written, content_length);
                }
            }
            if (ec == boost::asio::error::eof)
                break;
            if (ec) {
                fclose(file);
                fs::remove(destPath);
                strError = "Network error: " + ec.message();
                return false;
            }
        }

        fclose(file);

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
                   std::string& strError)
{
    // Download filelist.txt to a temp file
    fs::path tmpPath = fs::temp_directory_path() / "triangles_bootstrap_filelist.txt";

    std::string urlPath = std::string(BASE_PATH) + "filelist.txt";
    if (!DownloadFile(host, urlPath, tmpPath, nullptr, strError))
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

bool DownloadBootstrap(const std::string& host,
                       const fs::path& dataDir,
                       ProgressCallback progressFn,
                       std::string& strError)
{
    bool gotBlockFile = false;

    // Try downloading bootstrap.tar.gz first
    fs::path tmpTarGz = dataDir / "bootstrap.tar.gz.tmp";
    std::string tarUrl = std::string(BASE_PATH) + "bootstrap.tar.gz";

    bool tarDownloaded = DownloadFile(host, tarUrl, tmpTarGz, progressFn, strError);

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
        if (!FetchFileList(host, files, fallbackError)) {
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
            if (!DownloadFile(host, urlPath, destPath, progressFn, strError))
                return false;
        }

        gotBlockFile = fs::exists(dataDir / "blk0001.dat");
    }

    if (!gotBlockFile) {
        strError = "No blk0001.dat after download";
        return false;
    }

    // Remove any extracted txleveldb/ and database/ - they were built on
    // a different machine and won't work here. FastImportBlockFile() will
    // rebuild the index directly from blk0001.dat on next startup.
    fs::path txleveldb = dataDir / "txleveldb";
    fs::path database = dataDir / "database";
    if (fs::exists(txleveldb))
        fs::remove_all(txleveldb);
    if (fs::exists(database))
        fs::remove_all(database);

    return true;
}

} // namespace Bootstrap
