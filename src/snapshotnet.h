// Copyright (c) 2026 Triangles developers
// Distributed under the MIT/X11 software license

#ifndef TRIANGLES_SNAPSHOTNET_H
#define TRIANGLES_SNAPSHOTNET_H

#include "uint256.h"
#include "serialize.h"

#include <boost/filesystem.hpp>
#include <string>
#include <vector>

class CNode;
class CDataStream;

namespace SnapshotNet {

// Maximum bytes returned per snapshot chunk reply. Sized for Tor cell efficiency
// (Tor sends 514-byte cells; ~256 KB amortizes overhead without exceeding the
// 32 MB peer send buffer when many chunks are queued).
static const int32_t SNAPSHOT_CHUNK_MAX = 256 * 1024;

// One advertised snapshot a peer can serve.
struct AvailableSnapshot
{
    int height;
    uint256 fileHash;
    int64_t totalSize;

    AvailableSnapshot() : height(0), fileHash(0), totalSize(0) {}

    IMPLEMENT_SERIALIZE
    (
        READWRITE(height);
        READWRITE(fileHash);
        READWRITE(totalSize);
    )
};

// Initial snapshot fetch on a fresh install.
//   - Picks the latest checkpoint height with a published snapshot hash.
//   - Polls connected peers for matching snapshots.
//   - Stripes chunk requests across peers in parallel.
//   - Verifies the full file SHA256 against the compiled-in snapshot hash.
//   - Writes the result to dataDir/utxo-snapshot.bin.
//
// Blocks for up to timeoutSec waiting for peers + transfer. Returns true if a
// verified snapshot was written, false on timeout/no peer/verification fail.
bool TryFetchSnapshot(const boost::filesystem::path& dataDir,
                      int timeoutSec,
                      std::string& strError);

// Server-side message dispatch. Called from main.cpp ProcessMessage.
// Returns true if strCommand was a snapshot-protocol message (handled or
// rejected for malformed input).
bool ProcessSnapshotMessage(CNode* pfrom,
                            const std::string& strCommand,
                            CDataStream& vRecv);

// Generate dataDir/utxo-snapshot.bin from the current chain if our tip is past
// the latest checkpoint height with a published snapshot hash and the file does
// not already exist. Safe to call repeatedly; no-op when conditions aren't met.
// Sets the NODE_SNAPSHOT service flag on success.
void EnsureLocalSnapshot();

// Returns true when this node holds a verified snapshot file ready to serve.
bool HasServableSnapshot();

} // namespace SnapshotNet

#endif // TRIANGLES_SNAPSHOTNET_H
