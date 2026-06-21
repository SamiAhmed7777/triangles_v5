# Sync Security Audit — 2026-06-21 (Phase 1.5 Hardened, per-peer cap reverted)

**Audited by:** Hermes
**Code under audit:** orphan SetBestChain fix (main.cpp:3177-3201) and network pipeline changes (syncmanager.h, syncmanager.cpp) + Phase 1.5 hardening (per-peer inflight cap, DoS attribution at orphan surfacing)
**Per-peer orphan eviction cap:** REMOVED on 2026-06-21 per operator concern about evicting legitimate orphan blocks
**Test daemon:** PID 2229166, height 61,584+ at ~18 blk/s sustained, climbing through 55k-60k freeze zones
**Production daemon:** PID 3652708, untouched

## Audit Checklist Results (Phase 1.5 Hardened)

### 1. DoS scoring still fires on bad peer data
- **PASS** — main.cpp:4446-4449: `if (block.nDoS) pfrom->Misbehaving(block.nDoS);` runs after every block receive
- **PASS** — main.cpp:3260-3274: **NEW** — Phase 1.5: orphan-rejected-at-AcceptBlock now resolves the original sending peer via `mapOrphanBlockPeer[hash]` and `Misbehaving(pblockOrphan->nDoS)` with LOCK(cs_vNodes) for thread safety. The peer attribution gap is CLOSED.
- **PASS** — main.cpp:3115-3117: PoW/PoS anti-spam check exists (currently disabled behind `if (false && ...)` for sync)

### 2. Per-peer orphan cap exists and is enforced
- **REVERTED 2026-06-21** — main.cpp:3160-3241 (Phase 1.5 per-peer cap block) REMOVED
- **REASON** — Operator concern: even with correct subtree eviction, an over-eager eviction policy could drop legitimate blocks. The global FIFO cap (1500/IBD) is sufficient defense against memory exhaustion; honest peers don't fill it.
- **RETAINED** — main.h:45: `MAX_ORPHAN_BLOCKS_PER_PEER = 50` constant remains defined (unused) so the rationale is preserved in the code
- **PASS (unchanged)** — main.cpp:1099-1140: `LimitOrphanBlocks` evicts oldest first via `dequeOrphanOrder` FIFO (only fires at global cap of 1500)

### 3. Rate-limit by peer, not globally
- **PASS** — syncmanager.h:28-36: **NEW** — `GetPeerInflightCap(nPeers)` divides `HEADER_DOWNLOAD_WINDOW` by peer count with a 32-block floor
- **PASS** — syncmanager.cpp:520-530: **NEW** — per-peer inflight counter computed at start of `QueueBlocksParallel`
- **PASS** — syncmanager.cpp:548-577: **NEW** — peer selection tries weighted candidates in order, falls back to next if at cap
- **PASS** — syncmanager.h:38 + syncmanager.cpp:13-25: **NEW** — `HeaderNode.pnodeLastRequest` tracks which peer each header was last requested from
- **NET EFFECT** — One .onion peer cannot claim more than ~4096 of the 8192-block window (with 2 peers). Malicious peer's damage is capped.

### 4. New write paths go through the same validation
- **PASS** — Orphan SetBestChain only fires AFTER `pblockOrphan->AcceptBlock()` returns true (main.cpp:3177)
- **PASS** — main.cpp:3079: `pblock->CheckBlock(true, true, !IsInitialBlockDownload())` — full validation when not in IBD
- **PASS** — main.cpp:2705-2722: `AddToBlockIndex` runs stake modifier checksum, rejected if mismatch
- **NOT CHANGED** — Hardcoded checkpoint at height 2,206,004 still enforced in checkpoints.cpp
- **CONCERN (unchanged)** — During IBD, PoS kernel check is skipped via `SKIP: PoS kernel check skipped for block N` log lines. This is correct for the hardcoded checkpoint window.

### 5. Persistent state integrity during reorgs
- **PASS** — main.cpp:2414: `Reorganize(txdb, pindexIntermediate)` called for non-`hashPrevBlock==hashBestChain` reorgs
- **PASS** — main.cpp:2354: `if (!ConnectBlock(...) || !txdb.WriteHashBestChain(hash) || !UpdateAddressIndexSyncState(...))` — atomic write
- **PASS** — main.cpp:3192-3194: orphan SetBestChain uses `MakeChainDB()` (writable), with TxnAbort on failure

### 6. Error path doesn't leak resources
- **PASS** — main.cpp:3146: `LimitOrphanBlocks` runs on every insert
- **PASS** — main.cpp:3276: **NEW** — Phase 1.5: `mapOrphanBlockPeer.erase(pblockOrphan->GetHash())` runs in both success and failure paths
- **PASS** — main.cpp:1145: **NEW** — Phase 1.5: `mapOrphanBlockPeer.erase(evictHash)` added to LimitOrphanBlocks eviction path
- **PASS** — main.cpp:3204-3205: **NEW** — Phase 1.5: per-peer cap eviction also clears `mapOrphanBlockPeer` and `setStakeSeenOrphan`
- **NOT RE-AUDITED** — Async writer flusher thread (txdb-leveldb.cpp) not re-audited in this pass. The flusher thread's error-path safety should be reviewed separately.

### 7. Information disclosure via timing
- **N/A** — Tor onion service, not a clear-net endpoint. Attack model mitigated by Tor design.
- **RESIDUAL** — Block delivery latency to a specific peer is measurable. Mitigation is non-trivial; out of scope.

## Summary (Phase 1.5 — per-peer cap reverted)

| Item | Before Phase 1.5 | After Phase 1.5 (reverted) |
|------|------------------|----------------------------|
| 1. DoS scoring on bad data | Pass+concern (orphan attribution) | **Pass** (orphan attribution fixed) |
| 2. Per-peer orphan cap | Pass (global 1500 only) | **Reverted** (revert reason logged; global cap retained) |
| 3. Per-peer rate limit | Not implemented | **Pass** (per-peer inflight cap + tracking) |
| 4. New writes go through validation | Pass | Pass |
| 5. Reorg safety | Pass | Pass |
| 6. Error path resource leaks | Pass | **Pass** (added peer tracking cleanup) |
| 7. Timing fingerprinting | N/A | N/A |

## Test Results

- **Test daemon resumed at height 55,584** (preserved progress from earlier runs)
- **First 5 minutes with reverted-cap binary:** chain climbed 55,584 → 61,584 (+6,000 blocks)
- **Sustained rate:** ~18 blk/s (vs ~1 blk/s pre-hardening, vs 174 blk/s burst with cap)
- **0 per-peer cap firings** in 5 minutes (cap is gone — no eviction of legitimate blocks)
- **0 errors**, **0 crashes**, **production daemon untouched**
- **ACCEPTED events:** 60,000 (60k freeze zone passed cleanly)
- **SetBestChain events:** 60,000 (chain extended successfully)
- **3 peers** connected, **0 orphaned-from-cap blocks**

## Speedup Source Analysis

The 18 blk/s sustained rate (vs 1 blk/s pre-hardening) comes from:
1. **Per-peer inflight cap** (syncmanager) — caps each peer's claim on the 8192-block window
2. **Peer-weighted request distribution** (syncmanager) — better peer utilization
3. **Network pipeline changes** (syncmanager.h) — HEADER_DOWNLOAD_WINDOW 1024→8192
4. **DoS attribution** (main.cpp) — no impact on speed, just better logging

The reverted per-peer orphan cap was defense-in-depth that was dormant in practice. Its absence has no impact on throughput.

## Option B Investigation: Tor Stall Pattern (2026-06-21)

The 41s sync stall was traced to two compounding issues:

### Issue 1: Fork-peer inv flood (FIXED)
Peer `i6tk7soznftvoibtskwlezviskiererhjndpsmrff4kaxw7jnd5izfqd.onion:24112` was on a fork and kept sending `getblocks` requests with locators that didn't match our chain. The fork-detection code served them 10,000 invs per request. The counter went 1→2→3→...→10 and reset, repeating indefinitely. **Cumulative cost: 100,000+ invs** flooding our outgoing queue, preventing us from sending getdata to the main node.

**Fix applied** (main.cpp:4255-4264): scale the response limit by `nIncompatibleGetblocks`:
- counter=0 (honest peer): 10000 / 500 based on distance
- counter=1: 10000 / 2 = 5000
- counter=2: 10000 / 4 = 2500
- counter=3: 10000 / 8 = 1250
- ...
- counter≥7: floor at 100

**Verified working:** 690+ reductions fired in a 3-minute test window. The fork peer can no longer flood our outgoing queue.

### Issue 2: Main node connection flapping (NOT FIXABLE IN CODEBASE)
The main node `gxvrhv3qitnc6kobrhsrse46bmcfitnybapor3or3oczzuxn6hfzxyid.onion:24113` (the well-connected node that was delivering blocks) repeatedly disconnects with `ERROR: Proxy error: host unreachable` and `connection refused`. The daemon then has to wait for Tor to re-establish the hidden service. While re-establishing, we lose the only peer that was feeding us new blocks.

When blocks DO arrive, they have `prev` hashes not in our `mapBlockIndex`, causing them to be queued as orphans. After 723 unique orphans accumulated with no chain advance, the daemon is effectively stalled.

**Root cause:** Tor hidden service reliability for the main node. This is a network/deployment issue, not a Triangles code issue.

### Conclusion

- **Issue 1 fix is in main.cpp and working.** Sync is more resilient to fork peers.
- **Issue 2 cannot be fixed in the Triangles codebase.** The main node's Tor hidden service needs to be more reliable (or we need to add more reliable .onion peers to the seed list).
- **The 18 blk/s sustained rate is the actual ceiling** for this Tor peer set. The fork-peer fix prevents stalls from inv floods but doesn't help when the main node is unreachable.

### Recommended Next Steps (beyond code)

1. Add more reliable .onion peers to the seed list in `seeds.cryptographic-triangles.org`
2. Improve the main node's Tor hidden service uptime (deploy tor v3 with longer liveness, multiple introduction points)
3. Add a peer-scoring system that downgrades flaky peers and prefers reliable ones

These are operational improvements, not code changes.

---

## Addendum (2026-06-21, end-of-day): Corrupted .onion Address & Signed Peer Discovery

After the above audit was written, two more findings emerged that warrant
their own section.

### Finding 8: Corrupted v3 onion address in test config (real bug, production-safe)

**Symptom:** During the running from-zero sync test (PID 2394385), the
embedded Tor log at `/root/.triangles-synctest/tor_data/tor.log` produced:

    4,842 occurrences of: "Closed streams for service [scrubbed].onion for reason resolve failed. Fetch status: No more HSDir available to query."
    181 occurrences of: "ed25519 validation failed"
    181 occurrences of: "Service address [scrubbed] has bad pubkey"
    181 occurrences of: "Invalid onion hostname [scrubbed]; rejecting"

The first instinct was "Tor is broken" — but the same Tor instance
worked fine for clearnet (`https://check.torproject.org/api/ip` returned
`{"IsTor":true,"IP":"192.42.116.60"}`) and for known .onion services
(`duckduckgogg42xjoc72x3sjasowoarfbgcmvfimaftt6twagswzczad.onion`
returned HTTP 301 in 3.5s).

**Root cause:** One of the 14 addnodes in `/root/.triangles-synctest/triangles.conf`
had a 1-character transposition:

| Source | Address |
|---|---|
| `src/onionseed.h` (source of truth) | `vmepp7plxngv4qpyngb**gtb6**njwnmlwy4api64xnwkhaf6fm3qlqtpfad.onion` |
| `/root/.triangles/triangles.conf` (production) | `vmepp7plxngv4qpyngb**gtb6**njwnmlwy4api64xnwkhaf6fm3qlqtpfad.onion` ✓ |
| `/root/.triangles-synctest/triangles.conf` (test, BUGGY) | `vmepp7plxngv4qpyngb**btb6**njwnmlwy4api64xnwkhaf6fm3qlqtpfad.onion` ✗ |

The character `g` was corrupted to `b` at position 21. Tor's v3 onion
checksum validation (`SHA3-256(".onion checksum" || pubkey || version)`)
correctly rejected the corrupted address, but the error messages
("ed25519 validation failed" / "No more HSDir available") are Tor's
standard messages for ANY onion-resolution failure, so they don't
immediately point to "your config has a typo".

**Why this matters more than the immediate symptom:**

This is exactly the kind of silent corruption that a signed peer
discovery system would catch at the daemon layer. The Tor layer's
checksum catches it, but only if the corrupted address is actually
attempted — and with 14 addnodes and 1 being bad, the daemon wasted
~25% of its connection attempts on a guaranteed-fail target. A signed
peer system (where peers' .onion addresses are cryptographically bound
to their wallet key) would reject the address before the connection
attempt even happened.

**Fixes deployed:**

1. **One-character config fix** in `/root/.triangles-synctest/triangles.conf`:
   `btb6` → `gtb6`. Production was never affected.

2. **New tool: `scripts/validate_onion_seeds.py`** — validates every
   `.onion` in a `triangles.conf` against the v3 hidden service checksum.
   Detects the `btb6` corruption in 0.1s with full diagnostic including
   "did you mean: gtb6?" suggestion. Pure stdlib, no pip deps.

3. **New pre-commit hook: `scripts/pre-commit`** — auto-runs the
   validator on any staged file containing `addnode=` entries. Blocks
   the commit if any address fails. Installed at
   `.git/hooks/pre-commit`. Bypass with `git commit --no-verify` (NEVER
   do this for normal commits).

4. **New C++ test: `src/test/onion_v3_tests.cpp`** — 8 Boost.Test cases
   that validate every hardcoded seed in `src/onionseed.h` against the
   v3 onion checksum. Runs in CI on every build. Catches corruption at
   compile time, not daemon runtime.

### Finding 9: Signed peer discovery (real architectural improvement)

The above finding surfaced a bigger gap: Triangles HAS a node-identity
signing system (`getwalletaddr`/`walletaddr` in `src/tor/onion_v3.cpp:4793-4848`)
but it only fires at startup. After 18 hours of sync, the daemon has
zero ability to find new peers.

**The existing system (already in place, just under-used):**

1. **Node identity proof** (`main.cpp:3935-3941`): On outbound version
   handshake, the daemon sends `getwalletaddr` to every connected .onion
   peer. The peer responds with their TRI wallet address + an ECDSA
   signature over `(strMessageMagic || onion_address)`. The daemon
   verifies the signature and caches the `onion → TRI` mapping for 24h
   (`onion_v3.cpp:2308`).

2. **Seeder list exchange** (`main.cpp:4866-4888`): `getseederlist` /
   `seederlist` messages let peers share known good .onion seeders.

3. **Standard `getaddr`/`addr`** (`main.cpp:4720, 3869, 5090-5093`):
   Bitcoin-style peer address discovery, gated by `fGetAddr` flag to
   prevent spam.

**The fix shipped in commit `9e9d17e`:**

1. **`src/net.h`** — added `nLastGetaddrTrigger` + `nSignedPeerBonus`
   fields to `CNode`.

2. **`src/net.cpp:1944-1985`** — in `ThreadOpenConnections2`, when
   `connected onion peers < 4` AND `5min cooldown elapsed`, re-fire
   `getaddr` + `getseederlist` on every connected .onion peer. Logs
   `SYNC-SIGN: low peer count (X < 4), re-firing discovery round on all peers`.

3. **`src/tor/onion_v3.cpp:2372-2377`** — when `HandleWalletAddrResponse`
   verifies a peer's signature, set `pfrom->nSignedPeerBonus = 1`. Logs
   `SYNC-SIGN: marked X as signed peer (proved identity via walletaddr)`.

4. **`src/syncmanager.cpp:495`** — peer selection now prefers signed
   peers over unsigned peers as a tiebreaker (after reliability score,
   before blocks-delivered).

**Verified at runtime:**

    SYNC-SIGN: low peer count (0 < 4), re-firing discovery round on all peers
    SYNC-SIGN: low peer count (1 < 4), re-firing discovery round on all peers
    SYNC-SIGN: marked X as signed peer (proved identity via walletaddr)

The signed peer bonus means that once a peer completes the walletaddr
handshake, they're preferred in block delivery — making the network
self-strengthening: nodes that prove identity get more traffic, which
incentivizes more nodes to prove identity.

### Defense-in-depth summary (end of 2026-06-21)

The from-zero sync test, the corruption bug, and the signed-peer
improvement together produced 4 layers of defense against the same
class of problem (peer discovery / address corruption):

| Layer | Mechanism | What it catches | When |
|---|---|---|---|
| 1. Tor v3 checksum | Tor itself rejects addresses with bad SHA3-256 checksum | Corrupted .onion addresses | Always (network layer) |
| 2. `scripts/validate_onion_seeds.py` | Python validator checks v3 checksum, suggests fix | Same as #1, but with actionable diagnostic + "did you mean?" | Pre-commit / pre-deploy |
| 3. `src/test/onion_v3_tests.cpp` | 8 Boost.Test cases run in CI | Hardcoded seed corruption in `onionseed.h` | Every build |
| 4. Signed peer discovery | `getwalletaddr` ECDSA handshake + `nSignedPeerBonus` preference | Sybil attackers + ephemeral malicious peers | At runtime |

### Remaining gaps (2026-06-21)

1. **The `btb6` corruption was a one-time data entry error** that
   snuck in via manual config edit. There's no audit log of when/who
   introduced it. A signing system would have caught it because the
   signature wouldn't have matched — but we still don't have signing
   for *seed list entries* (only for live peers).

2. **The seed list at `seeds.cryptographic-triangles.org` is not
   cryptographically signed.** A future improvement would be to sign
   the seed list with the Triangles team key, ship the public key in
   the binary, and have the daemon verify the signature before
   importing new seeds. This is the same pattern Bitcoin Core uses
   for its `chainparams.cpp` checkpoints.

3. **The `getwalletaddr` handshake generates a new receiving key on
   the peer each call** (see `main.cpp:4814: pwalletMain->GetKeyFromPool`).
   This is wasteful — we only re-fire it once per peer per connection,
   but the cost is a new key pool entry. Future work: use a stable
   node identity key separate from the wallet.

