// Copyright (c) 2024-2025 Triangles developers
// Distributed under the MIT/X11 software license

#ifndef TRIANGLES_UTXOSNAPSHOT_H
#define TRIANGLES_UTXOSNAPSHOT_H

#include <string>
#include <filesystem>

// UTXO snapshot file magic bytes
static const unsigned int UTXO_SNAPSHOT_MAGIC = 0x53585455; // "UTXS" little-endian

// UTXO snapshot format version
// v1: original (headers + UTXOs)
// v2: + embeds raw blk0001.dat for full self-contained bootstrap
// v3: + carries setStakeSeen entries (prevoutStake, nStakeTime) so a
//     snapshot-loaded node has the recent PoS stake-collision set restored
//     immediately, without needing to walk the last N blocks on startup.
//     Required for the anti-spam "too little proof-of-stake" check in
//     ProcessBlock to work correctly post-snapshot-bootstrap, since
//     LoadBlockIndex only walks 500 blocks back from pindexBest.
static const unsigned int UTXO_SNAPSHOT_VERSION = 3;

namespace UtxoSnapshot {

    // Create a UTXO snapshot from the current chain state.
    // Writes the complete block index + all UTXOs to destPath. A complete
    // index lets snapshot-loaded peers serve fresh nodes from genesis.
    // Returns true on success, sets strError on failure.
    bool DumpSnapshot(const std::filesystem::path& destPath,
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
