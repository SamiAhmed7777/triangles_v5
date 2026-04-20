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
    static const char* BASE_PATH = "/";
    static const int PORT = 80;

    // Progress callback: (bytesDownloaded, totalBytes)
    typedef std::function<void(int64_t, int64_t)> ProgressCallback;

    // Check if data dir already has blockchain data
    bool NeedsBootstrap(const boost::filesystem::path& dataDir);

    // Download a single file via HTTP GET, write to destPath.
    // If noProxy is true, bypass Tor SOCKS proxy and connect directly
    // (used for clearnet bootstrap downloads).
    bool DownloadFile(const std::string& host, const std::string& urlPath,
                      const boost::filesystem::path& destPath,
                      ProgressCallback progressFn,
                      std::string& strError,
                      bool noProxy = false);

    // Fetch the file manifest (list of relative paths to download)
    bool FetchFileList(const std::string& host,
                       std::vector<std::string>& files,
                       std::string& strError,
                       bool noProxy = false);

    // Download bootstrap.tar.gz and extract to dataDir.
    // Falls back to filelist.txt + individual file download if tar.gz unavailable.
    bool DownloadBootstrap(const std::string& host,
                           const boost::filesystem::path& dataDir,
                           ProgressCallback progressFn,
                           std::string& strError);

    // Snapshot manifest (parsed from snapshot.manifest in bootstrap archive)
    struct SnapshotManifest {
        int format;            // format version, must be 1
        std::string network;   // "main" or "test"
        int height;            // block height of the snapshot tip
        std::string hash;      // block hash at that height (hex, no 0x prefix)
        int dbversion;         // DATABASE_VERSION the txleveldb was built with
    };

    // Parse a snapshot.manifest file into a SnapshotManifest struct.
    bool ParseManifest(const boost::filesystem::path& manifestPath,
                       SnapshotManifest& manifest,
                       std::string& strError);

    // Verify a parsed manifest against compiled-in checkpoints and config.
    bool VerifyManifest(const SnapshotManifest& manifest,
                        std::string& strError);

} // namespace Bootstrap

#endif // TRIANGLES_BOOTSTRAP_H
