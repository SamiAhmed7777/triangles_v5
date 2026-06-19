// Copyright (c) 2024-2025 Triangles developers
// Distributed under the MIT/X11 software license

#ifndef TRIANGLES_UTXOSNAPSHOT_H
#define TRIANGLES_UTXOSNAPSHOT_H

#include <string>
#include <filesystem>

// UTXO snapshot file magic bytes
static const unsigned int UTXO_SNAPSHOT_MAGIC = 0x53585455; // "UTXS" little-endian

// UTXO snapshot format version
static const unsigned int UTXO_SNAPSHOT_VERSION = 1;

// Number of block index entries to include in snapshot (covers difficulty,
// median time, stake modifier, and reorg depth requirements)
static const unsigned int UTXO_SNAPSHOT_DEFAULT_HEADERS = 2000;

namespace UtxoSnapshot {

    // Create a UTXO snapshot from the current chain state.
    // Writes last nHeaders block index entries + all UTXOs to destPath.
    // Returns true on success, sets strError on failure.
    bool DumpSnapshot(const std::filesystem::path& destPath,
                      unsigned int nHeaders,
                      std::string& strError);

    // Load a UTXO snapshot from a file into a fresh LevelDB.
    // Writes block index entries, UTXOs, hashBestChain, and dbformat.
    // The LevelDB must NOT be open yet (call before LoadBlockIndex).
    // `requireCheckpoint` enforces that the snapshot tip is a known
    // checkpoint (for P2P-delivered snapshots). Local loads from a
    // trusted operator pass false.
    // Returns true on success, sets strError on failure.
    bool LoadSnapshot(const std::filesystem::path& snapshotPath,
                      const std::filesystem::path& dataDir,
                      std::string& strError,
                      bool requireCheckpoint);

} // namespace UtxoSnapshot

#endif // TRIANGLES_UTXOSNAPSHOT_H
