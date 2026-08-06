// Copyright (c) 2024 Triangles developers
// Distributed under the MIT/X11 software license

#ifndef TRIANGLES_BOOTSTRAP_H
#define TRIANGLES_BOOTSTRAP_H

#include <string>
#include <vector>
#include <functional>
#include <filesystem>
#include <cstdint>

namespace Bootstrap {

    // Bootstrap server configuration
    inline constexpr const char* DEFAULT_HOST = "bootstrap.cryptographic-triangles.org";
    inline constexpr const char* BASE_PATH = "/";
    inline constexpr int PORT = 443;

    // Progress callback: (bytesDownloaded, totalBytes)
    typedef std::function<void(int64_t, int64_t)> ProgressCallback;

    // Check if data dir already has blockchain data
    bool NeedsBootstrap(const std::filesystem::path& dataDir);

    // Download a file via HTTP GET, write to destPath.
    // If noProxy is true, bypass Tor SOCKS proxy and connect directly
    // (used for clearnet bootstrap downloads).
    // If portOverride is set (>0), uses that port instead of the default PORT.
    bool DownloadFile(const std::string& host, const std::string& urlPath,
                      const std::filesystem::path& destPath,
                      ProgressCallback progressFn,
                      std::string& strError,
                      bool noProxy = false,
                      int portOverride = -1,
                      int64_t maxDownloadBytes = 4LL * 1024 * 1024 * 1024);

    // Advertised identity of a snapshot listed by manifest.json.
    // The advertised SHA256 is accepted only when it matches the hash compiled
    // into checkpoints.cpp for the same height.
    struct RemoteSnapshot {
        std::string filename;
        std::string sha256;
        int height;
        std::string blockHash;
    };

    // Parse and validate the small, untrusted bootstrap manifest. This routine
    // performs no network I/O and is exposed so malformed-input behavior can
    // be covered by unit tests.
    bool ParseRemoteSnapshotManifest(const std::string& manifestText,
                                     RemoteSnapshot& snapshot,
                                     std::string& strError);

    // Download a UTXO snapshot and load it into a fresh txleveldb.
    // This is much faster than downloading the full bootstrap archive.
    // Returns true if snapshot was downloaded and loaded successfully.
    bool DownloadUtxoSnapshot(const std::string& host,
                              const std::filesystem::path& dataDir,
                              ProgressCallback progressFn,
                              std::string& strError);

} // namespace Bootstrap

#endif // TRIANGLES_BOOTSTRAP_H
