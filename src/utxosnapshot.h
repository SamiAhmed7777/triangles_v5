// Copyright (c) 2024-2025 Triangles developers
// Distributed under the MIT/X11 software license

#ifndef TRIANGLES_UTXOSNAPSHOT_H
#define TRIANGLES_UTXOSNAPSHOT_H

#include <string>
#include <boost/filesystem.hpp>

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
    bool DumpSnapshot(const boost::filesystem::path& destPath,
                      unsigned int nHeaders,
                      std::string& strError);

    // Load a UTXO snapshot from a file into a fresh LevelDB.
    // Writes block index entries, UTXOs, hashBestChain, and dbformat.
    // The LevelDB must NOT be open yet (call before LoadBlockIndex).
    // Returns true on success, sets strError on failure.
    bool LoadSnapshot(const boost::filesystem::path& snapshotPath,
                      const boost::filesystem::path& dataDir,
                      std::string& strError);

} // namespace UtxoSnapshot

#endif // TRIANGLES_UTXOSNAPSHOT_H
