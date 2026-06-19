// Copyright (c) 2024-2025 Triangles developers
// Distributed under the MIT/X11 software license

#ifndef TRIANGLES_UTXOSNAPSHOT_H
#define TRIANGLES_UTXOSNAPSHOT_H

#include <string>
#include <filesystem>

// UTXO snapshot file magic bytes
static const unsigned int UTXO_SNAPSHOT_MAGIC = 0x53585455; // "UTXS" little-endian

// UTXO snapshot format version
//
// v1: headers (last N=2000) + UTXOs only. Node still needs blk0001.dat from
//     somewhere (legacy bootstrap tarball or P2P) to fully verify.
// v2: full chain archive. Adds a `blocks` section with raw blk0001.dat content
//     and writes ALL CDiskBlockIndex entries (every block from genesis to
//     snapshot tip). After loading a v2 snapshot, the node is fully
//     self-contained: it can serve any block to peers, fully verify the
//     chain, validate transactions, and resume syncing from the snapshot
//     tip forward. Replaces the legacy `tri-bootstrap.tar.gz` artifact.
static const unsigned int UTXO_SNAPSHOT_VERSION = 2;

// Number of block index entries to include in snapshot (v1 only).
// v2 always includes ALL headers — this constant is retained for the v1
// fallback path and as a CLI override for diagnostic snapshots.
static const unsigned int UTXO_SNAPSHOT_DEFAULT_HEADERS = 2000;

namespace UtxoSnapshot {

    // Create a UTXO snapshot from the current chain state.
    // (v2) Writes ALL CDiskBlockIndex entries + the raw blk0001.dat bytes +
    // all UTXOs to destPath. Output is a complete chain archive at the
    // current tip — a fresh node loading this is fully usable (can serve
    // blocks, fully verify, sync forward).
    // Returns true on success, sets strError on failure.
    bool DumpSnapshot(const std::filesystem::path& destPath,
                      unsigned int nHeaders,
                      std::string& strError);

    // Load a UTXO snapshot from a file into the data directory.
    // Writes:
    //   - all CDiskBlockIndex entries → chain DB
    //   - all UTXOs → chain DB
    //   - raw blk0001.dat bytes → dataDir/blk0001.dat
    //   - hashBestChain, dbformat → chain DB
    // The chain DB must NOT be open yet (call before LoadBlockIndex).
    // `requireCheckpoint` enforces that the snapshot tip is a known
    // checkpoint (for P2P-delivered snapshots). Local loads from a
    // trusted operator pass false.
    // v1 snapshots (headers+UTXOs only) still load via the partial-load
    // path; the blocks section is simply absent.
    // Returns true on success, sets strError on failure.
    bool LoadSnapshot(const std::filesystem::path& snapshotPath,
                      const std::filesystem::path& dataDir,
                      std::string& strError,
                      bool requireCheckpoint);

} // namespace UtxoSnapshot

#endif // TRIANGLES_UTXOSNAPSHOT_H
