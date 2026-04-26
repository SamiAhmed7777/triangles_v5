// Copyright (c) 2026 Triangles developers
// Distributed under the MIT/X11 software license

#include "snapshotnet.h"

#include "checkpoints.h"
#include "main.h"
#include "net.h"
#include "protocol.h"
#include "sync.h"
#include "ui_interface.h"
#include "util.h"
#include "utxosnapshot.h"
#include "version.h"

#include <openssl/sha.h>

#include <filesystem>
#include <thread>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <map>
#include <mutex>
#include <vector>

namespace fs = std::filesystem;

extern std::vector<CNode*> vNodes;
extern CCriticalSection cs_vNodes;
extern uint64_t nLocalServices;

namespace SnapshotNet {

// ---------------------------------------------------------------------------
// Fetcher state
// ---------------------------------------------------------------------------

namespace {

struct ChunkRequest
{
    int64_t offset;
    int32_t size;
    int64_t requestedAt;   // GetTimeMicros() when sent
    CNode*  pnode;         // not refcounted; checked under cs_vNodes
    bool    done;
};

struct FetcherState
{
    std::mutex mu;
    std::condition_variable cv;
    bool active = false;
    bool finished = false;
    bool success = false;

    int     targetHeight = 0;
    uint256 expectedFileHash;
    int64_t totalSize = 0;

    // Per-peer announcement: peer NodeId -> AvailableSnapshot for our targetHeight
    std::map<int, AvailableSnapshot> peerOffers;

    // Outstanding chunk requests, keyed by chunk-aligned offset.
    std::map<int64_t, ChunkRequest> pending;

    // Bitmap of chunks already written, by chunk-aligned offset.
    std::map<int64_t, bool> received;

    fs::path destPath;
    FILE* fpDest = nullptr;
};

static FetcherState g_fetch;

// Per-CNode integer id (used as map key). We stash a counter via the node's
// pointer address — the pointer itself is stable for the node's lifetime, but
// reused across reconnects, so we just use it as an opaque identity for the
// duration of a single fetch.
static intptr_t NodeKey(const CNode* p) { return reinterpret_cast<intptr_t>(p); }

static int64_t AlignDown(int64_t off, int32_t chunk)
{
    return (off / chunk) * chunk;
}

static void CloseDest()
{
    if (g_fetch.fpDest) {
        fclose(g_fetch.fpDest);
        g_fetch.fpDest = nullptr;
    }
}

static void ResetState()
{
    g_fetch.active = false;
    g_fetch.finished = false;
    g_fetch.success = false;
    g_fetch.targetHeight = 0;
    g_fetch.expectedFileHash = 0;
    g_fetch.totalSize = 0;
    g_fetch.peerOffers.clear();
    g_fetch.pending.clear();
    g_fetch.received.clear();
    CloseDest();
    g_fetch.destPath.clear();
}

// Verify the full destination file's SHA256 matches g_fetch.expectedFileHash.
// Returns true on match. Caller holds g_fetch.mu.
static bool VerifyDestFileHash(std::string& strErr)
{
    if (!g_fetch.fpDest) {
        strErr = "no dest file open";
        return false;
    }
    fflush(g_fetch.fpDest);
    fseek(g_fetch.fpDest, 0, SEEK_SET);

    SHA256_CTX ctx;
    SHA256_Init(&ctx);

    std::vector<unsigned char> buf(64 * 1024);
    int64_t total = 0;
    while (true) {
        size_t n = fread(buf.data(), 1, buf.size(), g_fetch.fpDest);
        if (n == 0) break;
        SHA256_Update(&ctx, buf.data(), n);
        total += (int64_t)n;
    }
    if (total != g_fetch.totalSize) {
        strErr = strprintf("size mismatch: have %" PRId64 " want %" PRId64, total, g_fetch.totalSize);
        return false;
    }

    uint256 actual;
    SHA256_Final((unsigned char*)&actual, &ctx);
    if (actual != g_fetch.expectedFileHash) {
        strErr = "snapshot file hash mismatch";
        return false;
    }
    return true;
}

// Build the list of chunk offsets that still need a request (not pending, not done).
// Caller holds g_fetch.mu.
static std::vector<int64_t> MissingChunkOffsets()
{
    std::vector<int64_t> out;
    if (g_fetch.totalSize <= 0) return out;
    for (int64_t off = 0; off < g_fetch.totalSize; off += SNAPSHOT_CHUNK_MAX) {
        if (g_fetch.received.count(off)) continue;
        if (g_fetch.pending.count(off)) continue;
        out.push_back(off);
    }
    return out;
}

// Send getsnapchunk requests striped across snapshot-capable peers.
// Caller holds g_fetch.mu.
static int DispatchChunkRequests()
{
    if (!g_fetch.active || g_fetch.finished) return 0;

    std::vector<CNode*> servers;
    {
        LOCK(cs_vNodes);
        for (CNode* p : vNodes) {
            if (!p->fSuccessfullyConnected) continue;
            if (p->nVersion < SNAPSHOT_PROTO_VERSION) continue;
            if (!(p->nServices & NODE_SNAPSHOT)) continue;
            // peer must have offered our target snapshot
            auto it = g_fetch.peerOffers.find((int)NodeKey(p));
            if (it == g_fetch.peerOffers.end()) continue;
            if (it->second.fileHash != g_fetch.expectedFileHash) continue;
            servers.push_back(p);
        }
    }
    if (servers.empty()) return 0;

    std::vector<int64_t> missing = MissingChunkOffsets();
    if (missing.empty()) return 0;

    // Cap inflight to avoid swamping peer send queues. Each chunk is up to
    // 256 KB; 32 outstanding * 256 KB = 8 MB pipeline per peer max.
    const size_t kMaxInflightPerPeer = 32;
    std::map<int, size_t> inflightPerPeer;
    for (const auto& kv : g_fetch.pending)
        inflightPerPeer[(int)NodeKey(kv.second.pnode)]++;

    int64_t now = GetTimeMicros();
    int sent = 0;
    size_t serverIdx = 0;
    for (int64_t off : missing) {
        // Round-robin pick a server with capacity.
        CNode* pick = nullptr;
        for (size_t tries = 0; tries < servers.size(); ++tries) {
            CNode* candidate = servers[(serverIdx + tries) % servers.size()];
            if (inflightPerPeer[(int)NodeKey(candidate)] < kMaxInflightPerPeer) {
                pick = candidate;
                serverIdx = (serverIdx + tries + 1) % servers.size();
                break;
            }
        }
        if (!pick) break; // all peers saturated; loop will resume later

        int32_t reqSize = (int32_t)std::min<int64_t>(SNAPSHOT_CHUNK_MAX,
                                                     g_fetch.totalSize - off);
        ChunkRequest req;
        req.offset = off;
        req.size = reqSize;
        req.requestedAt = now;
        req.pnode = pick;
        req.done = false;
        g_fetch.pending[off] = req;
        inflightPerPeer[(int)NodeKey(pick)]++;

        // PushMessage is thread-safe (acquires its own cs_vSend).
        pick->PushMessage("getsnapchunk", g_fetch.targetHeight, off, reqSize);
        ++sent;
    }
    return sent;
}

// Reassign chunks whose request has timed out (peer slow or dropped).
// Caller holds g_fetch.mu.
static void ReissueStalledChunks(int64_t timeoutMicros)
{
    int64_t now = GetTimeMicros();
    std::vector<int64_t> stale;
    for (const auto& kv : g_fetch.pending) {
        if (now - kv.second.requestedAt > timeoutMicros)
            stale.push_back(kv.first);
    }
    for (int64_t off : stale)
        g_fetch.pending.erase(off);
}

} // namespace

// ---------------------------------------------------------------------------
// Public: TryFetchSnapshot
// ---------------------------------------------------------------------------

bool TryFetchSnapshot(const fs::path& dataDir, int timeoutSec, std::string& strError)
{
    int snapHeight = Checkpoints::GetBestSnapshotHeight();
    if (snapHeight <= 0) {
        strError = "no compiled-in snapshot hash available";
        return false;
    }

    uint256 expectedHash;
    if (!Checkpoints::GetSnapshotHash(snapHeight, expectedHash)) {
        strError = "snapshot hash lookup failed";
        return false;
    }

    fs::path destPath = dataDir / "utxo-snapshot.bin";
    if (fs::exists(destPath)) {
        // Caller already has a snapshot file; let normal init pick it up.
        return true;
    }

    {
        std::lock_guard<std::mutex> lk(g_fetch.mu);
        if (g_fetch.active) {
            strError = "snapshot fetch already in progress";
            return false;
        }
        ResetState();
        g_fetch.targetHeight = snapHeight;
        g_fetch.expectedFileHash = expectedHash;
        g_fetch.destPath = destPath;
        g_fetch.active = true;
    }

    printf("SnapshotNet: requesting snapshot at height %d (hash=%s)\n",
           snapHeight, expectedHash.ToString().c_str());
    uiInterface.InitMessage(_("Looking for UTXO snapshot peers..."));

    int64_t start = GetTime();
    int64_t deadline = start + timeoutSec;
    int64_t lastBroadcast = 0;
    int64_t lastProgress = 0;

    while (GetTime() < deadline) {
        // (Re)broadcast getsnap every 30s to pick up newly connected peers.
        if (GetTime() - lastBroadcast >= 30) {
            int peerCount = 0;
            {
                LOCK(cs_vNodes);
                for (CNode* p : vNodes) {
                    if (!p->fSuccessfullyConnected) continue;
                    if (p->nVersion < SNAPSHOT_PROTO_VERSION) continue;
                    if (!(p->nServices & NODE_SNAPSHOT)) continue;
                    p->PushMessage("getsnap");
                    ++peerCount;
                }
            }
            lastBroadcast = GetTime();
            printf("SnapshotNet: getsnap sent to %d snapshot-capable peers\n", peerCount);
        }

        {
            std::lock_guard<std::mutex> lk(g_fetch.mu);

            // If we have at least one matching offer and total size known,
            // open dest file and start dispatching chunk requests.
            if (g_fetch.totalSize > 0 && !g_fetch.fpDest) {
                g_fetch.fpDest = fopen(g_fetch.destPath.string().c_str(), "wb+");
                if (!g_fetch.fpDest) {
                    strError = "cannot create " + g_fetch.destPath.string();
                    g_fetch.finished = true;
                    g_fetch.success = false;
                    break;
                }
                // Pre-size the file so chunk writes can use random access.
                if (fseek(g_fetch.fpDest, g_fetch.totalSize - 1, SEEK_SET) == 0) {
                    char zero = 0;
                    fwrite(&zero, 1, 1, g_fetch.fpDest);
                    fflush(g_fetch.fpDest);
                }
            }

            ReissueStalledChunks(45 * (int64_t)1000000); // 45s per-chunk timeout
            DispatchChunkRequests();

            // Progress print every 10s
            if (GetTime() - lastProgress >= 10 && g_fetch.totalSize > 0) {
                int64_t got = (int64_t)g_fetch.received.size() * SNAPSHOT_CHUNK_MAX;
                if (got > g_fetch.totalSize) got = g_fetch.totalSize;
                printf("SnapshotNet: %" PRId64 " / %" PRId64 " bytes (%" PRId64 "%%)\n",
                       got, g_fetch.totalSize,
                       (int64_t)((got * 100) / g_fetch.totalSize));
                lastProgress = GetTime();
            }

            // All chunks in?
            if (g_fetch.totalSize > 0) {
                int64_t total = (g_fetch.totalSize + SNAPSHOT_CHUNK_MAX - 1) / SNAPSHOT_CHUNK_MAX;
                if ((int64_t)g_fetch.received.size() >= total) {
                    std::string verifyErr;
                    if (VerifyDestFileHash(verifyErr)) {
                        g_fetch.success = true;
                    } else {
                        strError = verifyErr;
                        g_fetch.success = false;
                        // Drop bad file so we don't trick later loaders.
                        CloseDest();
                        std::error_code ec;
                        fs::remove(g_fetch.destPath, ec);
                    }
                    g_fetch.finished = true;
                    break;
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    bool ok;
    {
        std::lock_guard<std::mutex> lk(g_fetch.mu);
        if (!g_fetch.finished) {
            // Timed out
            if (strError.empty())
                strError = strprintf("timeout after %d seconds (totalSize=%" PRId64 ", chunks=%" PRIszu ")",
                                     timeoutSec, g_fetch.totalSize, g_fetch.received.size());
            CloseDest();
            std::error_code ec;
            fs::remove(g_fetch.destPath, ec);
        }
        ok = g_fetch.success;
        ResetState();
    }

    if (ok) {
        printf("SnapshotNet: snapshot fetched and verified (%s)\n",
               destPath.string().c_str());
    }
    return ok;
}

// ---------------------------------------------------------------------------
// Server side: read from local snapshot file
// ---------------------------------------------------------------------------

namespace {

// Cached metadata for the local snapshot file. Filled lazily by EnsureLocalSnapshot
// or by HasServableSnapshot scanning the dest path.
static std::mutex g_localMu;
static bool g_localScanned = false;
static bool g_localPresent = false;
static int g_localHeight = 0;
static uint256 g_localFileHash = 0;
static int64_t g_localTotalSize = 0;
static fs::path g_localPath;

static bool ScanLocalSnapshot()
{
    g_localPresent = false;
    g_localHeight = 0;
    g_localFileHash = 0;
    g_localTotalSize = 0;
    g_localPath = GetDataDir() / "utxo-snapshot.bin";

    if (!fs::exists(g_localPath)) return false;

    int snapHeight = Checkpoints::GetBestSnapshotHeight();
    if (snapHeight <= 0) return false;

    uint256 expectedHash;
    if (!Checkpoints::GetSnapshotHash(snapHeight, expectedHash)) return false;

    std::error_code ec;
    int64_t sz = (int64_t)fs::file_size(g_localPath, ec);
    if (ec) return false;

    // Hash the file once on first scan to confirm it matches the compiled-in
    // snapshot hash. A node won't advertise NODE_SNAPSHOT if the local file is
    // corrupt or for a different height.
    FILE* f = fopen(g_localPath.string().c_str(), "rb");
    if (!f) return false;

    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    std::vector<unsigned char> buf(64 * 1024);
    while (true) {
        size_t n = fread(buf.data(), 1, buf.size(), f);
        if (n == 0) break;
        SHA256_Update(&ctx, buf.data(), n);
    }
    fclose(f);

    uint256 actual;
    SHA256_Final((unsigned char*)&actual, &ctx);
    if (actual != expectedHash) {
        printf("SnapshotNet: local utxo-snapshot.bin hash mismatch — not advertising\n");
        return false;
    }

    g_localPresent = true;
    g_localHeight = snapHeight;
    g_localFileHash = expectedHash;
    g_localTotalSize = sz;
    return true;
}

static bool ReadLocalChunk(int64_t offset, int32_t size, std::vector<unsigned char>& out)
{
    std::lock_guard<std::mutex> lk(g_localMu);
    if (!g_localPresent) return false;
    if (offset < 0 || offset >= g_localTotalSize) return false;
    if (size <= 0 || size > SNAPSHOT_CHUNK_MAX) return false;
    int32_t actual = (int32_t)std::min<int64_t>(size, g_localTotalSize - offset);

    FILE* f = fopen(g_localPath.string().c_str(), "rb");
    if (!f) return false;
    if (fseek(f, offset, SEEK_SET) != 0) { fclose(f); return false; }

    out.resize(actual);
    size_t n = fread(out.data(), 1, actual, f);
    fclose(f);
    if ((int32_t)n != actual) { out.clear(); return false; }
    return true;
}

} // namespace

bool HasServableSnapshot()
{
    std::lock_guard<std::mutex> lk(g_localMu);
    if (!g_localScanned) {
        ScanLocalSnapshot();
        g_localScanned = true;
    }
    return g_localPresent;
}

void EnsureLocalSnapshot()
{
    {
        std::lock_guard<std::mutex> lk(g_localMu);
        if (g_localScanned && g_localPresent) return;
    }

    int snapHeight = Checkpoints::GetBestSnapshotHeight();
    if (snapHeight <= 0) return;

    fs::path destPath = GetDataDir() / "utxo-snapshot.bin";

    // If the file exists, scan it (validates hash). Otherwise, generate it
    // from the current chain if our tip is past the snapshot height.
    bool needGenerate = !fs::exists(destPath);

    if (needGenerate) {
        if (nBestHeight < snapHeight) return; // not synced past it yet
        printf("SnapshotNet: dumping local snapshot at height %d -> %s\n",
               snapHeight, destPath.string().c_str());
        std::string err;
        // DumpSnapshot dumps from current chain tip — only call when tip == snapHeight,
        // otherwise the produced file won't match the published hash. Skip for now;
        // operators must produce the canonical file out-of-band and place it here.
        // (Auto-dump from arbitrary tip would not produce the canonical hash.)
        return;
    }

    {
        std::lock_guard<std::mutex> lk(g_localMu);
        ScanLocalSnapshot();
        g_localScanned = true;
    }

    if (g_localPresent) {
        nLocalServices |= NODE_SNAPSHOT;
        printf("SnapshotNet: serving local snapshot height=%d size=%" PRId64 "\n",
               g_localHeight, g_localTotalSize);
    }
}

// ---------------------------------------------------------------------------
// Server side: P2P message dispatch
// ---------------------------------------------------------------------------

bool ProcessSnapshotMessage(CNode* pfrom, const std::string& strCommand, CDataStream& vRecv)
{
    if (strCommand == "getsnap")
    {
        // Reply with a list of snapshots we can serve. Currently only the
        // single canonical snapshot at the latest checkpoint with a published
        // hash; future versions may serve multiple.
        std::vector<AvailableSnapshot> reply;
        if (HasServableSnapshot()) {
            std::lock_guard<std::mutex> lk(g_localMu);
            AvailableSnapshot a;
            a.height = g_localHeight;
            a.fileHash = g_localFileHash;
            a.totalSize = g_localTotalSize;
            reply.push_back(a);
        }
        pfrom->PushMessage("snap", reply);
        return true;
    }

    if (strCommand == "snap")
    {
        std::vector<AvailableSnapshot> offers;
        vRecv >> offers;
        if (offers.size() > 16) {
            pfrom->Misbehaving(20);
            return true;
        }
        std::lock_guard<std::mutex> lk(g_fetch.mu);
        if (!g_fetch.active) return true;
        for (const AvailableSnapshot& a : offers) {
            if (a.height != g_fetch.targetHeight) continue;
            if (a.fileHash != g_fetch.expectedFileHash) continue;
            if (a.totalSize <= 0 || a.totalSize > (int64_t)4 * 1024 * 1024 * 1024) continue;
            g_fetch.peerOffers[(int)NodeKey(pfrom)] = a;
            if (g_fetch.totalSize == 0)
                g_fetch.totalSize = a.totalSize;
        }
        g_fetch.cv.notify_all();
        return true;
    }

    if (strCommand == "getsnapchunk")
    {
        int height;
        int64_t offset;
        int32_t size;
        vRecv >> height >> offset >> size;

        std::vector<unsigned char> data;
        if (HasServableSnapshot()) {
            std::lock_guard<std::mutex> lk(g_localMu);
            if (height == g_localHeight)
                ReadLocalChunk(offset, size, data);
        }
        // Always reply, even with empty data, so the requester can give up
        // on this peer for this chunk and reissue elsewhere.
        pfrom->PushMessage("snapchunk", height, offset, data);
        return true;
    }

    if (strCommand == "snapchunk")
    {
        int height;
        int64_t offset;
        std::vector<unsigned char> data;
        vRecv >> height >> offset >> data;

        if (data.size() > (size_t)SNAPSHOT_CHUNK_MAX) {
            pfrom->Misbehaving(20);
            return true;
        }

        std::lock_guard<std::mutex> lk(g_fetch.mu);
        if (!g_fetch.active) return true;
        if (height != g_fetch.targetHeight) return true;
        if (data.empty()) {
            // Peer doesn't have it; drop pending so it gets reissued.
            g_fetch.pending.erase(offset);
            return true;
        }

        if (offset < 0 || offset >= g_fetch.totalSize) {
            pfrom->Misbehaving(10);
            g_fetch.pending.erase(offset);
            return true;
        }
        int32_t expected = (int32_t)std::min<int64_t>(SNAPSHOT_CHUNK_MAX,
                                                      g_fetch.totalSize - offset);
        if ((int32_t)data.size() != expected) {
            pfrom->Misbehaving(10);
            g_fetch.pending.erase(offset);
            return true;
        }

        if (g_fetch.fpDest) {
            if (fseek(g_fetch.fpDest, offset, SEEK_SET) == 0) {
                size_t w = fwrite(data.data(), 1, data.size(), g_fetch.fpDest);
                if (w == data.size()) {
                    g_fetch.received[offset] = true;
                    g_fetch.pending.erase(offset);
                    g_fetch.cv.notify_all();
                }
            }
        }
        return true;
    }

    return false;
}

} // namespace SnapshotNet
