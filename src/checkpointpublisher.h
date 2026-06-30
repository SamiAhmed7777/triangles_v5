// Copyright (c) 2026 The Triangles developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// Signed Checkpoint Publisher (Triangles v5.9.24)
//
// Background
// ----------
// Triangles' existing CSyncCheckpoint (src/checkpoints.cpp) is Bitcoin-era
// P2P-broadcast code that uses a HARDCODED master pubkey. That model does
// not match how the project actually operates today (one operator with
// multiple keys, snapshot publishing on the bootstrap server, no master
// hierarchy). Instead we layer a *new* signed-checkpoint scheme on top of
// the bootstrap server, using the same compact-message primitive the UTXO
// snapshot trust model already uses (see src/bootstrap.cpp:IsTrustedSnapshotSigner).
//
// Trust model
// -----------
//   - A signed checkpoint document is a small JSON file hosted at
//         https://bootstrap.cryptographic-triangles.org/signed-checkpoints.json
//   - It contains a list of (height, block_hash, unix_timestamp) entries,
//     followed by a single signing_address + signature covering the canonical
//     serialization of the entry list.
//   - The signing_address must appear in the trusted signers list
//     (Checkpoints::IsTrustedCheckpointSigner, see checkpoints.cpp). The
//     default trust list is the same as IsTrustedSnapshotSigner but kept
//     separate so they can be managed independently.
//   - Verification uses the existing CKey::SignCompact / SetCompactSignature
//     code path through the wallet's verifymessage-style flow — no new
//     cryptography is introduced.
//
// Producer
// --------
//   - The daemon operator runs `triangles-cli publishcheckpoint [interval]`
//     which builds the entry list from pindexBest, signs with the wallet's
//     default key, and writes the JSON document to a path the operator
//     uploads to the bootstrap server (or a cron job uploads automatically
//     when -autopublishcheckpoint is set).
//   - Default interval = every 5000 blocks; can be set to every N.
//   - The first entry is always the chain tip at publish time.
//
// Consumer
// --------
//   - On startup, the daemon can call
//     Checkpoints::LoadSignedCheckpoints(host, dataDir, strError)
//     which fetches, verifies, and merges the trusted entries into the
//     compiled-in mapCheckpoints (lower priority — compiled-in wins on
//     conflict to defend against remote-rollback).
//   - Checkpoints::IsKnownSignedCheckpoint(height, hash) returns true if
//     either compiled-in OR signed-remote knows about (height, hash).
//
// Relationship to existing code
// -----------------------------
//   - mapCheckpoints in src/checkpoints.cpp is UNCHANGED — the compiled-in
//     list is still the primary trust anchor.
//   - Signed checkpoints EXTEND the trust anchor with operator-published
//     ones, useful when the operator wants to publish a checkpoint at
//     height 2,210,000 without waiting for a code release.
//   - mapSnapshotHashes is unaffected.

#ifndef TRIANGLES_CHECKPOINT_PUBLISHER_H
#define TRIANGLES_CHECKPOINT_PUBLISHER_H

#include <string>
#include <vector>
#include <cstdint>

namespace Checkpoints {

// One signed checkpoint entry. Compact, serializable, no JSON inside the
// struct — JSON wrapping happens in the publisher.
struct SignedCheckpoint {
    int nHeight;                  // block height
    std::string hashHex;          // block hash, lowercase hex, NO 0x prefix, NO leading zeros
    int64_t nTimestamp;           // unix seconds when published (signed over)
};

// Result of a publish or verify operation. Used for human-readable errors
// and structured logging.
struct SignedCheckpointResult {
    bool ok;                      // overall success
    std::string error;            // populated if !ok
    int nEntriesWritten;          // for publish: how many entries went into the JSON
    int nEntriesVerified;         // for verify: how many entries passed signature check
};

// Default URL for the bootstrap server's signed-checkpoints document.
static const char* SIGNED_CHECKPOINTS_URL =
    "https://bootstrap.cryptographic-triangles.org/signed-checkpoints.json";

// Default local output path the daemon writes to on publish.
static const char* SIGNED_CHECKPOINTS_DEFAULT_OUT =
    "/var/www/triangles-bootstrap/signed-checkpoints.json";

// ---- Producer ----

// Build the JSON document for the entries [heights[0], heights[1], ...]
// (in DESCENDING order — tip first) using the wallet's default key.
// Returns true on success; outJson/outputPath written. Wallet must be
// unlocked (signmessage requires it).
//
// This is the in-process builder used by both:
//   - The triangles-cli `publishcheckpoint` RPC command
//   - The daemon's auto-publish loop when -autopublishcheckpoint is set
bool BuildSignedCheckpointsJson(
    const std::vector<SignedCheckpoint>& entries,
    const std::string& signingAddress,
    const std::string& signatureBase64,
    const std::string& message,
    std::string& outJson,
    std::string& strError);

// Canonical (deterministic) serialization of the entry list. The signature
// is over this exact byte sequence — both producer and consumer MUST use
// this function so verification is reproducible across platforms.
std::string SerializeEntriesForSigning(const std::vector<SignedCheckpoint>& entries);

// ---- Consumer ----

// Fetch the signed-checkpoints document from the bootstrap server, parse
// it, verify the signature, and return the verified entries. Does NOT
// merge into mapCheckpoints — caller decides what to do with the entries.
//
// onDiskPath: optional. If non-empty and the file already exists locally,
// skip the network fetch and verify the on-disk copy. This makes startup
// robust against bootstrap-server outages.
bool LoadSignedCheckpoints(
    const std::string& host,
    const std::string& onDiskPath,
    std::vector<SignedCheckpoint>& outEntries,
    std::string& outSigningAddress,
    std::string& strError);

// Verify the signature on a parsed JSON document. Pure function — no
// network, no filesystem.
bool VerifySignedCheckpoints(
    const std::string& jsonText,
    std::vector<SignedCheckpoint>& outEntries,
    std::string& outSigningAddress,
    std::string& strError);

// Is the given signing address in the trusted signers list? Mirrors
// Bootstrap::IsTrustedSnapshotSigner but kept separate for independent
// governance.
bool IsTrustedCheckpointSigner(const std::string& addr);

// ---- Merged lookup ----

// Is (height, hash) known to either the compiled-in OR the
// signed-remote set? This is what AcceptBlock / fork-detection should call.
bool IsKnownSignedCheckpoint(int nHeight, const std::string& hashHex);

// Inject loaded entries into the in-memory signed-checkpoint cache. Called
// by init.cpp after LoadSignedCheckpoints returns successfully. Subsequent
// IsKnownSignedCheckpoint() calls will return true for any (height, hash)
// in the loaded set.
void AddSignedCheckpoints(const std::vector<SignedCheckpoint>& entries);

// Clear the in-memory cache (used at reorg boundaries and in tests).
void ClearSignedCheckpoints();

} // namespace Checkpoints

#endif // TRIANGLES_CHECKPOINT_PUBLISHER_H
