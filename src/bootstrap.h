// Copyright (c) 2024 Triangles developers
// Distributed under the MIT/X11 software license

#ifndef TRIANGLES_BOOTSTRAP_H
#define TRIANGLES_BOOTSTRAP_H

#include <string>
#include <vector>
#include <functional>
#include <filesystem>

namespace Bootstrap {

    // Bootstrap server configuration
    static const char* DEFAULT_HOST = "bootstrap.cryptographic-triangles.org";
    static const char* BASE_PATH = "/";
    static const int PORT = 80;

    // Progress callback: (bytesDownloaded, totalBytes)
    typedef std::function<void(int64_t, int64_t)> ProgressCallback;

    // Check if data dir already has blockchain data
    bool NeedsBootstrap(const std::filesystem::path& dataDir);

    // Download a single file via HTTP GET, write to destPath.
    // If noProxy is true, bypass Tor SOCKS proxy and connect directly
    // (used for clearnet bootstrap downloads).
    // If portOverride is set (>0), uses that port instead of the default PORT.
    bool DownloadFile(const std::string& host, const std::string& urlPath,
                      const std::filesystem::path& destPath,
                      ProgressCallback progressFn,
                      std::string& strError,
                      bool noProxy = false,
                      int portOverride = -1);

    // Fetch the file manifest (list of relative paths to download)
    bool FetchFileList(const std::string& host,
                       std::vector<std::string>& files,
                       std::string& strError,
                       bool noProxy = false);

    // Download bootstrap.tar.gz and extract to dataDir.
    // Falls back to filelist.txt + individual file download if tar.gz unavailable.
    bool DownloadBootstrap(const std::string& host,
                           const std::filesystem::path& dataDir,
                           ProgressCallback progressFn,
                           std::string& strError);

    // Snapshot manifest (parsed from snapshot.manifest in bootstrap archive)
    struct SnapshotManifest {
        int format;            // format version, must be 1
        std::string network;   // "main" or "test"
        int height;            // block height of the snapshot tip
        std::string hash;      // block hash at that height (hex, no 0x prefix)
        int dbversion;         // DATABASE_VERSION the txleveldb was built with
        std::string signature; // Ed25519 signature of (height || hash), hex-encoded (empty if unsigned)
    };

    // Parse a snapshot.manifest file into a SnapshotManifest struct.
    bool ParseManifest(const std::filesystem::path& manifestPath,
                       SnapshotManifest& manifest,
                       std::string& strError);

    // Verify a parsed manifest against compiled-in checkpoints and config.
    bool VerifyManifest(const SnapshotManifest& manifest,
                        std::string& strError);

    // Download a UTXO snapshot and load it into a fresh txleveldb.
    // This is much faster than downloading the full bootstrap archive.
    // Returns true if snapshot was downloaded and loaded successfully.
    bool DownloadUtxoSnapshot(const std::string& host,
                              const std::filesystem::path& dataDir,
                              ProgressCallback progressFn,
                              std::string& strError);

    // ===================================================================
    // Trusted snapshot publisher — RPC-driven single-slot rotation
    // ===================================================================
    // Returns the currently active trusted publisher, or empty string if
    // only the built-in fallback is in effect.
    std::string GetActiveTrustedSnapshotPublisher();

    // Atomically replaces the active publisher. The previous one is dropped
    // immediately (Design A: single-slot, no grace period). Persists to
    // <datadir>/snapshot-publisher.json so the choice survives restarts.
    bool SetTrustedSnapshotPublisher(const std::string& addr,
                                     std::string& strError);

    // Clears the runtime override and reverts to the built-in fallback
    // list. Also removes snapshot-publisher.json from disk.
    bool UnsetTrustedSnapshotPublisher(std::string& strError);

    // Called once at daemon startup (from init.cpp) to load any persisted
    // runtime override.
    void LoadTrustedSnapshotPublisher();

} // namespace Bootstrap

#endif // TRIANGLES_BOOTSTRAP_H
