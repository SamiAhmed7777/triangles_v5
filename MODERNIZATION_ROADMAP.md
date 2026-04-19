# Triangles Modernization Roadmap

**Goal:** Make TRI faster to sync, safer for wallets, and more useful as a currency — without breaking consensus.

**Invariant:** Any change that modifies block validation, stake modifier computation, transaction format, or signature verification MUST preserve exact consensus with existing v5.x nodes. When in doubt, test against a synced v5.8.1 node.

---

## Priority 1: Faster Syncing (High Impact, Low Risk)

### 1.1 Update Checkpoints (Easy, Immediate)
**Problem:** Last hardcoded checkpoint is at block 2,186,940. `IsInitialBlockDownload()` returns false past this point, causing orphan limit to drop from 4000 to 750 — exactly what caused the fork deadlock.

**Fix:** Add checkpoints every ~50,000 blocks up to current height (~2,207,000+).
```cpp
// src/checkpoints.cpp - add recent checkpoints
{2190000, uint256("...")},
{2195000, uint256("...")},
{2200000, uint256("...")},
{2205000, uint256("...")},
{2210000, uint256("...")},
```
**Risk:** None — checkpoints are only used for IBD detection and quick rejection of clearly wrong chains.

### 1.2 Increase Post-Checkpoint Orphan Limit (Easy)
**Problem:** 750 orphans after IBD is too low for a low-peer network. During the fork incident, 750 orphans filled up and the node deadlocked.

**Fix:**
```cpp
// src/main.h
static const unsigned int MAX_ORPHAN_BLOCKS = 2000;  // was 750
```
**Risk:** Slightly more memory usage during forks. Worth it for resilience.

### 1.3 Parallel Block Download (Medium Effort)
**Problem:** Current implementation downloads blocks sequentially from one peer at a time during IBD.

**Fix:** Increase batch sizes and allow concurrent block downloads from multiple peers:
```cpp
// src/main.cpp
// During IBD, request blocks from multiple peers simultaneously
unsigned int nGetDataBatchSize = IsInitialBlockDownload() ? 8000 : 1000;  // was 4000
```
**Risk:** Low — larger batch sizes are already proven in Bitcoin forks.

### 1.4 Header-First Sync (Medium Effort)
**Problem:** Node downloads full blocks before validating headers. A bad peer can waste bandwidth.

**Fix:** Download and validate all headers first (compact ~80 bytes each), then download full blocks only for the best chain.
- Separate `getheaders`/`headers` message handling
- Download blocks only for the best header chain
- Reduces wasted bandwidth during forks by 95%+

### 1.5 Bootstrap Over HTTPS with Resume (Easy)
**Problem:** Built-in bootstrap (`-bootstrap`) uses raw TCP and can't resume interrupted downloads.

**Fix:** The existing `bootstrap.cpp` already supports downloading. Add:
- Resume support (Range headers)
- SHA256 verification of downloaded archive
- Better progress reporting
- Fallback mirrors

---

## Priority 2: Wallet Safety (Critical)

### 2.1 Automatic Wallet Backup Before Dangerous Operations (Easy)
**Problem:** Corrupt wallet = lost funds. No automatic backup before risky operations.

**Fix:** In `walletdb.cpp`, before any rewrite:
```cpp
// Before wallet.dat rewrite, copy to wallet.dat.bak
if (boost::filesystem::exists(pathWallet)) {
    boost::filesystem::copy_file(pathWallet, pathWallet + ".bak",
        boost::filesystem::copy_option::overwrite_if_exists);
}
```

### 2.2 Detect and Report BDB Corruption (Easy)
**Problem:** BDB corruption silently corrupts wallet. User doesn't know until it's too late.

**Fix:** Add wallet integrity check on load:
```cpp
// In CWallet::LoadWallet()
// After opening, verify BDB environment is healthy
// If DB_RUNRECOVERY, auto-salvage and warn user
```

### 2.3 Wallet.dat Versioning (Medium Effort)
**Problem:** Single wallet.dat file. If it corrupts during write, funds are lost.

**Fix:** Implement copy-on-write wallet saves:
- Write new wallet data to `wallet.dat.new`
- Atomically rename `wallet.dat` → `wallet.dat.old`, `wallet.dat.new` → `wallet.dat`
- Keep last 3 wallet revisions
- On load, try wallet.dat first, fall back to wallet.dat.old if corrupt

### 2.4 Seed Phrase / HD Wallet (High Effort, High Impact)
**Problem:** Losing wallet.dat = losing everything. No recovery mechanism.

**Fix:** Implement BIP39/BIP44 HD wallet as optional upgrade:
- Generate 12/24-word seed phrase on new wallet creation
- Derive all keys from seed deterministically
- Import seed on any device to recover wallet
- Keep backward compatibility with existing non-HD wallets

---

## Priority 3: Network Resilience (Medium Impact)

### 3.1 Better Peer Management (Medium Effort)
**Problem:** Low peer counts (2-6) lead to fork divergence. No prioritization of reliable peers.

**Fix:**
- Peer reliability scoring (track which peers provide valid blocks)
- Prefer peers that are ahead and on the same chain
- Automatic disconnection of stale/forked peers
- Increase default `maxconnections` from 64 to 128

### 3.2 Compact Block Relay (High Effort)
**Problem:** Full blocks are sent even when the receiver likely already has most transactions.

**Fix:** Implement BIP 152 compact blocks:
- Send block header + short transaction IDs
- Receiver fills in from mempool, only requests missing transactions
- Reduces bandwidth by ~90% during normal operation

### 3.3 DNS Seed Infrastructure (Easy)
**Problem:** `dnsseed=0` when Tor-only means no automatic peer discovery.

**Fix:** Run a DNS seed server that resolves to known reliable onion addresses:
```
seed.cryptographic-triangles.org → returns onion addresses of healthy nodes
```

---

## Priority 4: User Experience (Medium Impact)

### 4.1 Progress Reporting for IBD (Easy)
**Problem:** Users see "downloading blocks..." with no useful progress indicator.

**Fix:**
- Report `headers` vs `blocks` progress separately
- Show estimated time remaining based on download speed
- Log progress every 1000 blocks (currently every 5000)
- Qt wallet: update progress bar more frequently

### 4.2 Staking Dashboard Improvements (Easy)
**Problem:** Qt wallet shows staking info but not clearly.

**Fix:**
- Show expected time to stake more prominently
- Display staking weight as percentage of network
- Notify when stake is found (system notification)
- Show "staking" indicator in system tray

### 4.3 Transaction Fee Estimation (Medium Effort)
**Problem:** No fee estimation. Users guess.

**Fix:** Track recent block inclusion rates by fee level, provide fee recommendations.

---

## Priority 5: Code Modernization (Low Urgency, Good Hygiene)

### 5.1 C++17/20 Features
- Replace raw pointers with smart pointers where safe
- Use `std::optional`, `std::string_view`, `std::filesystem`
- Replace boost::filesystem with std::filesystem (C++17)

### 5.2 Build System
- CMake is already in place (good)
- Add sanitizers (ASAN, UBSAN) to CI
- Static analysis with clang-tidy

### 5.3 Testing
- Current test coverage is thin
- Add unit tests for:
  - Checkpoint validation
  - Stake modifier computation
  - Bootstrap download/resume
  - Wallet BDB recovery
  - Orphan block handling

---

## What NOT to Change

These are consensus-critical and must remain identical:
- Block validation rules
- Stake modifier computation (`ComputeNextStakeModifier`)
- Transaction signature verification
- Block reward schedule
- PoW/PoS target computation
- Chain trust / difficulty adjustment
- Message serialization format
- Protocol version handshaking

Any change to these requires a coordinated network upgrade (hard fork).

---

## Implementation Order

1. **This week:** Update checkpoints (1.1), increase orphan limit (1.2), wallet backup before save (2.1)
2. **Next week:** Better progress reporting (4.1), increase batch size (1.3)
3. **Month 1:** Wallet versioning (2.3), bootstrap resume (1.5)
4. **Month 2:** Header-first sync (1.4), peer reliability (3.1)
5. **Month 3+:** HD wallet (2.4), compact blocks (3.2)
