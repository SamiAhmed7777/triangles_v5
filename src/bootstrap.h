// Copyright (c) 2024 Triangles developers
// Distributed under the MIT/X11 software license

#ifndef TRIANGLES_BOOTSTRAP_H
#define TRIANGLES_BOOTSTRAP_H

#include <string>
#include <vector>
#include <functional>
#include <boost/filesystem.hpp>

namespace Bootstrap {

    // Bootstrap server configuration
    static const char* DEFAULT_HOST = "bootstrap.cryptographic-triangles.org";
    static const char* FALLBACK_HOST = "194.233.88.206";
    static const char* BASE_PATH = "/";
    static const int PORT = 80;

    // Progress callback: (bytesDownloaded, totalBytes)
    typedef std::function<void(int64_t, int64_t)> ProgressCallback;

    // Check if data dir already has blockchain data
    bool NeedsBootstrap(const boost::filesystem::path& dataDir);

    // Download a single file via HTTP GET, write to destPath
    bool DownloadFile(const std::string& host, const std::string& urlPath,
                      const boost::filesystem::path& destPath,
                      ProgressCallback progressFn,
                      std::string& strError);

    // Fetch the file manifest (list of relative paths to download)
    bool FetchFileList(const std::string& host,
                       std::vector<std::string>& files,
                       std::string& strError);

    // Download bootstrap.tar.gz and extract to dataDir.
    // Falls back to filelist.txt + individual file download if tar.gz unavailable.
    bool DownloadBootstrap(const std::string& host,
                           const boost::filesystem::path& dataDir,
                           ProgressCallback progressFn,
                           std::string& strError);

} // namespace Bootstrap

#endif // TRIANGLES_BOOTSTRAP_H
