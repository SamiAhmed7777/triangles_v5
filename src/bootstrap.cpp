// Copyright (c) 2024 Triangles developers
// Distributed under the MIT/X11 software license

#include "bootstrap.h"

#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/algorithm/string.hpp>

#include <fstream>
#include <sstream>
#include <cstdio>

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

} // namespace Bootstrap
