# IBD Stall Bug Analysis — `triangles_v5`

**Date:** 2026-04-24
**Symptom:** Node syncs from genesis, accepts blocks normally up to a point (observed: ~6284), then permanently stalls. `askfor_queue=0`, `orphans=0`, no new blocks ever arrive.

---

## Root Cause: Header Sync Cache Exhaustion Without Refill

The bug is a **broken feedback loop** between the header planner and block downloader. The node drains its header cache faster than it refills it, and once the cache is empty, the pipeline freezes with no recovery path.

### The Pipeline (how it should work)

```
getheaders → 2000 headers → mapHeaderSync → AskFor(MSG_BLOCK) → mapAskFor → getdata → block received → ProcessBlock → QueueHeaderSyncBlocksParallel (refill)
                                                                                              ↑
                                                                                              └── every 500 blocks: getheaders+getblocks to ALL peers
```

### The Bug Path (how it actually dies)

**Step 1: Initial header fetch**
- `version` handler sends `getblocks` + `getheaders` to peer
- Peer responds with up to 2000 headers → stored in `mapHeaderSync`
- `QueueHeaderSyncBlocksParallel(512)` queues up to 512 blocks via `AskFor()`

**Step 2: Blocks download and consume headers**
- Blocks arrive, `ProcessBlock()` accepts them
- Each accepted block calls `MarkHeaderSyncBlockAccepted()` which **erases** it from `mapHeaderSync`
- After accepting, `QueueHeaderSyncBlocksParallel(512)` tries to queue more from remaining `mapHeaderSync` entries

**Step 3: The critical gap — header cache runs dry**
- The header cache holds at most `MAX_HEADER_SYNC_CACHE = 15000` entries
- But `getheaders` only returns **2000 headers per batch**
- The `HEADER_DOWNLOAD_WINDOW = 512` means only 512 blocks are in-flight at once
- So blocks are consumed from the cache faster than headers are fetched
- Each accepted block erases its entry; if all 2000 headers are downloaded before the next `getheaders` fires...

**Step 4: Cache empties → pipeline dies**
- `mapHeaderSync` becomes empty
- `hashBestHeaderSync` is recomputed to `0` (by `RecomputeBestHeaderSync()`)
- The refill condition `if (hashBestHeaderSync != 0)` at line ~3599 evaluates **false**
- No more blocks are queued, ever

**Step 5: No recovery mechanism kicks in**
- The `ContinueHeaderSync()` call only fires when `vHeaders.size() >= 2000` (full batch)
- If the last batch was smaller (partial response, or exactly 2000 consumed), **no new `getheaders` is sent**
- The pipeline refill every 500 blocks only fires **when a block is received** — but no blocks are coming
- The stall detection sends `getblocks` (not `getheaders`), which produces `inv` messages → `AskFor()` for individual blocks
- But `getblocks` uses `CBlockLocator` with exponential spacing, which maps to an old block → peer sends `inv` for blocks we already have → walk-forward logic tries to progress but may loop or stall

### Why It's Worse on Fast Connections / Sync-from-Zero

- Blocks download fast (PoS blocks are tiny)
- All 2000 headers are consumed quickly
- The window between "all headers consumed" and "need more headers" is tiny
- On slow Tor connections, the 15-minute TTL eviction (`PruneHeaderSync`) adds a second failure mode: headers that took too long to download get evicted, creating gaps in `GetHeaderSyncDownloadPath()`

---

## Affected Code Locations

| File | Line(s) | Issue |
|------|---------|-------|
| `main.cpp` | 137 | `MAX_HEADER_SYNC_CACHE = 15000` — cache is large but `getheaders` only returns 2000 |
| `main.cpp` | 138 | `HEADER_DOWNLOAD_WINDOW = 512` — window is smaller than header batch |
| `main.cpp` | 298-345 | `AddHeaderSyncNode()` / `PruneHeaderSync()` — TTL eviction can create gaps in the download path |
| `main.cpp` | 380-396 | `GetHeaderSyncDownloadPath()` — walks back from tip; **breaks on first gap** in `mapHeaderSync` chain |
| `main.cpp` | 455-461 | `MarkHeaderSyncBlockAccepted()` — erases from `mapHeaderSync`, may set `hashBestHeaderSync = 0` |
| `main.cpp` | 3599-3606 | Block-accepted refill — **guarded by `hashBestHeaderSync != 0`**, skips when cache is empty |
| `main.cpp` | 4972-4976 | `getheaders` continuation — **only fires on full batch** (`vHeaders.size() >= 2000`) |
| `main.cpp` | 5110-5121 | Pipeline refill every 500 blocks — **only fires when blocks arrive**, useless during stall |
| `main.cpp` | 5952-5980 | Stall detection — sends `getblocks` (not `getheaders`), can't restart header planner |

---

## Fix Options

### Fix A: Refill headers when cache runs dry (minimal, targeted)

In the block-accepted handler, when `hashBestHeaderSync == 0`, send `getheaders` to all peers:

```cpp
// After the existing refill (line ~3599)
if (hashBestHeaderSync == 0)
{
    // Header cache exhausted — request more headers from all peers
    LOCK(cs_vNodes);
    for (CNode* pnode : vNodes)
    {
        if (!pnode->fClient && pnode->nVersion != 0)
        {
            pnode->pindexLastGetHeadersBegin = NULL;
            pnode->PushGetHeaders(pindexBest, uint256(0));
        }
    }
}
```

### Fix B: Also send getheaders in stall detection (defense in depth)

In the stall handler (line ~5968), alongside the `getblocks`, also send `getheaders`:

```cpp
pto->PushGetBlocks(...);  // existing
pto->pindexLastGetHeadersBegin = NULL;
pto->PushGetHeaders(pindexBest, uint256(0));  // ADD THIS
```

### Fix C: Don't let the header cache fully drain (robustness)

In `QueueHeaderSyncBlocksParallel()`, stop consuming the last N entries from the cache to keep the chain intact. When only `HEADER_DOWNLOAD_WINDOW` entries remain, trigger a `getheaders` continuation before consuming more.

### Fix D: Periodic getheaders in SendMessages loop (most robust)

Add a periodic `getheaders` request in the `SendMessages` loop, similar to how stall detection already sends periodic `getblocks`. This ensures headers are always being fetched regardless of block progress:

```cpp
// In SendMessages, alongside stall detection:
if (!pto->fClient && IsInitialBlockDownload() && hashBestHeaderSync == 0)
{
    static int64_t nLastHeaderRequest = 0;
    if (GetTime() - nLastHeaderRequest >= 30)
    {
        pto->pindexLastGetHeadersBegin = NULL;
        pto->PushGetHeaders(pindexBest, uint256(0));
        nLastHeaderRequest = GetTime();
    }
}
```

---

## Recommended Fix

**Fix A + Fix B together** — minimal code change, covers both the block-accepted path and the stall recovery path. Fix D adds belt-and-suspenders protection in the main loop.

---

## Secondary Issue: TTL Eviction Creating Path Gaps

`PruneHeaderSync()` evicts entries older than 15 minutes. If block download is slow (Tor, slow peers), entries at the beginning of the download path can be evicted while entries at the end still exist. `GetHeaderSyncDownloadPath()` walks back from the tip and **breaks on the first missing entry**, making the entire tail of the cache unreachable.

**Fix:** In `GetHeaderSyncDownloadPath()`, skip gaps instead of breaking:

```cpp
while (hashTip != 0 && !mapBlockIndex.count(hashTip))
{
    auto mi = mapHeaderSync.find(hashTip);
    if (mi == mapHeaderSync.end())
        break;  // Currently breaks — could skip to next known ancestor instead
    vPath.push_back(hashTip);
    hashTip = mi->second.header.hashPrevBlock;
}
```

This is a secondary concern but contributes to cache exhaustion on high-latency connections.
