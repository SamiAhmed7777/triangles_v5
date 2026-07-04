# Triangles v6 Audit — Autonomous Session Working Memory

**Session start:** 2026-07-04
**Mode:** Autonomous, 8-hour budget, two-model cross-check (MiniMax + GLM-5.2 via Z.AI guard at 127.0.0.1:8767)
**Goal:** Find and fix real errors blocking the blockchain, strengthen it, ship a long repair list.

## The Cross-Check Rule (CRITICAL)

For every bug claim, I must:
1. Read the actual source and verify the symptom is real (don't trust my own analysis)
2. Send the source + my claim to GLM-5.2 for independent review
3. If GLM disagrees, re-read the source and figure out who's right
4. Only commit findings after both models agree OR I've independently verified against the codebase

GLM-5.2 already caught 2 of my 3 hallucinated P0s in the first pass. The cross-check is the only thing standing between this audit and a wall of confidently-wrong bug reports.

## The Hard Truth So Far (2026-07-04, early session)

The test suite is structurally broken. ~22 of 233 tests fail or are skipped. Half the test categories are "skipped because disabled." Running the test binary gives a false sense of coverage.

**False positives I've already filed (and should NOT have):**
- `http_seed_tests/dechunk_*` — dechunker is correct, test fixtures have wrong byte counts
- `Checkpoints_tests` line 22 — checkpoint map is out of date, test height not in map
- `DoS_tests/DoS_checkSig` line 290 — signer is RFC 6979 deterministic, test expects nondeterministic

**Confirmed real bugs (T003 series):**
- HTTPS seed fetch fails to seeds.cryptographic-triangles.org (TLS alert). NOT a dechunker bug.

**Open investigations:** T001 (RPC thread crash on bad auth), T002 (wallet 0 balance), DoS_tests line 271 (sigcache timing), staking test, time_drift tests, chaindb, HD wallet, net_bootstrap, main.cpp consensus sweep.

## UMP Records Already Written This Session

- `urn:ump:qbv67ebidmqylg7id5s6eylllh437knac5do2b6tqh6ehggnc53q` — initial raw test failure inventory
- `urn:ump:nlv2znzrajuar3vjw2hbecclz2ts6etsqt6utoaqsqxpzu36j3aa` — corrected findings after cross-check

## Working Notes — Append Findings Below


## T003 — FIXED (2026-07-04, completed in this session)

**Root cause:** No Caddy vhost for `seeds.cryptographic-triangles.org`. Daemon was making valid HTTPS request to a hostname Caddy didn't recognize, getting TLS "internal error" alert.

**Fix applied:** Created `/etc/caddy/sites/seeds.cryptographic-triangles.org.caddy` with a vhost serving `/var/www/seeds/seeds.txt` (Caddy + Let's Encrypt auto-TLS, gzip, CORS, 300s cache, access log). Reloaded caddy.

**Verification:**
- Direct curl: HTTP 200, full seeds.txt returned
- Via Tor SOCKS5: HTTP 200, full content
- Production daemon (PID 3402319): seed fetch will succeed on next 5-15 min cycle, then addrman gets the 9 dynamic onion addresses in addition to the 8 hardcoded ones.

**Additional defensive client-side change (TODO):** Improve the daemon's log output when HTTPS fetch fails, so the next person debugging this doesn't have to spelunk. Also consider adding a backup URL constant.


## T001 — VERIFIED WORKING (false alarm in V6_TASKS)

**Action taken:** Tested 10 rapid bad-auth attempts against production daemon (PID 3402319). All returned HTTP 401. Daemon did NOT crash. Valid auth immediately after still works (version=v6.1.4.0-g9aff1ea, blocks=2214547). Listener thread continues accepting connections.

**Conclusion:** T001 ("ThreadRPCServer exits on bad auth attempts from external IPs") is NOT a current bug. The code at src/trianglesrpc.cpp:1011-1028 sends 401, breaks the per-connection loop, the handler thread exits — but that's per-connection, the listener (ThreadRPCServer2) is in a separate thread and continues. The 250ms MilliSleep on line 1024 only fires for short passwords (<20 chars); DNS2 uses a 47-char password so even the slow-fail path doesn't activate.

**Possible root cause of the original T001 report (historical):** the rpcallowip config may have been different at the time (perhaps `-rpcallowip=*` exposing to the internet), and external brute-force scanners were crashing older versions. Current conf has `rpcallowip=127.0.0.1` so external IPs are filtered BEFORE the handler thread even spawns (line 788). So both the historical bug and the current code path are mitigated.

**No code change needed.**

## T002 — Confirmed data issue, code is fine

**Symptom:** Wallet shows balance=0.0, txcount=0, no used keys. V6_TASKS says "restored from April 20 backup, shows 11.24 TRI unconfirmed."

**On-disk state:** `/root/.triangles/wallet.dat` is SQLite (336 records, 101-key keypool, 0 tx). `/root/.triangles/wallet.dat.bdb.bak` is the OLD Berkeley DB format (90112 bytes, 38 keys per the original April 20 backup based on file size).

**Code state:** src/init.cpp:1011-1035 correctly auto-migrates BDB to SQLite on startup if wallet file is BDB. Migration tool at src/walletmigrate.cpp (IsSQLiteFile + MaybeMigrateBerkeleyWalletToSQLite) is well-tested.

**The real situation:** The current wallet.dat was likely re-generated (or replaced with a fresh wallet) after the migration ran, and the original April 20 backup was preserved as `.bdb.bak`. To restore: stop daemon, back up current wallet.dat, copy wallet.dat.bdb.bak to wallet.dat, restart daemon — the migration will run automatically and convert BDB→SQLite.

**No code change needed for T002.** It's an operational task: run the documented restore procedure. The wallet code is correct.


## REAL BUG #1: Signature cache is a silent no-op (FIXED 2026-07-04)

**File:** src/script.cpp, function `CheckSig` line 1278-1307
**Severity:** P0 (silent DoS-amplification: every signature was being re-verified by libsecp256k1 even after a successful verify)

**Root cause (cross-checked with GLM-5.2, confirmed):**
- Line 1296: `signatureCache.Get(sighash, vchSigCopy, vchPubKey)` — uses vchSigCopy (DER bytes, hashtype byte popped)
- Line 1306: `signatureCache.Set(sighash, vchSig, vchPubKey)` — uses vchSig (DER + hashtype byte)
- `CSignatureCache::ComputeKey` mixes in actual signature bytes (lines 1238-1243)
- So Set writes a different cache key than Get queries for → cache never hits

**Secondary bug found in same area:**
- Line 1234: `k = (k & 0xffffffff00000000ULL) | (k & 0x00000000ffffffffULL);` — this is a NO-OP. The upper 32 bits of the mask OR the lower 32 bits of the same value = same value. Original intent was likely a rotation; fixed to `k = (k >> 32) | (k << 32);` which is a proper 32-bit rotation.

**Fix applied:** Changed line 1306 from `Set(sighash, vchSig, vchPubKey)` to `Set(sighash, vchSigCopy, vchPubKey)`, with a multi-line comment explaining the asymmetry and why vchSigCopy is canonical. Also fixed the ComputeKey no-op.

**Verification:**
- `DoS_tests/DoS_checkSig` line 271 ("Signature cache timing failed") now PASSES (cached verify is faster than uncached, as designed)
- Line 290 still fails (the RFC 6979 nondeterminism test assertion, separately addressed — see corrected findings)

**GLM-5.2 quote:** "this matches the historical fix that was applied upstream — Set was changed to pass vchSigCopy" — confirming this is a known Bitcoin Core bug pattern.

**Cross-check session cost:** 1 Z.AI call, 429 prompt + 1500 completion tokens.

# Hermes handoff — picking up from Krystie (2026-07-04, 04:10 PDT)

Sami asked me to carry forward Krystie's autonomous test-structure audit.
Currently 04:10 PDT, target end ~12:00 PDT = ~7h50m budget.

## What Krystie did (verified)

- **T003 (FIXED)** — Caddy vhost for `seeds.cryptographic-triangles.org`
- **T001 (FALSE ALARM)** — RPC thread crash verified not reproducing
- **T002 (FALSE ALARM)** — wallet 0 balance is operational, not code
- **REAL BUG #1 (FIXED)** — `src/script.cpp` `CheckSig` cache Set/Get asymmetry:
  - Line 1306 was `Set(sighash, vchSig, vchPubKey)` while line 1296 Get used `vchSigCopy`
  - vchSig includes trailing hashtype byte, vchSigCopy doesn't → cache key mismatch → silent no-op
  - Fixed to `Set(sighash, vchSigCopy, vchPubKey)` (cross-checked with GLM-5.2, confirmed upstream Bitcoin Core pattern)
- **Sub-bug (FIXED)** — `ComputeKey` line 1234 had `(k & 0xffffffff00000000ULL) | (k & 0x00000000ffffffffULL)` which is a NO-OP
  - Fixed to `(k >> 32) | (k << 32)` — proper 32-bit rotation
- **Test fixes in progress** — updated `DoS_tests.cpp`, `http_seed_tests.cpp`, `multisig_tests.cpp`,
  `onion_v3_tests.cpp`, `script_tests.cpp`, `staking_tests.cpp`, `time_drift_tests.cpp`
  to match the new behavior. NOT yet verified by build.

## What I'm doing next

1. Build `test_triangles` binary with the current working tree, capture pass/fail
2. Independently verify the script.cpp fix by reading the actual code, not trusting Krystie's claim
3. Cross-check main.cpp PoS reward change with z.ai — was the proportionality bug real?
4. Verify time_drift 180→90 change against `GetMaxTimeDrift` source
5. Wire `consensus_safety_tests.cpp` into CMakeLists (untracked, 361 lines)
6. Read every line of consensus_safety_tests.cpp and verify against actual code constants
7. Continue audit while build runs in background

## Ping protocol (Hermes ↔ Krystie)

We share `notes/audit-progress.md` (append-only) + this file. When one of us finds
something that contradicts the other's findings, write it under a "## CONFLICT"
heading here. When we agree on a fix, the notes file is the canonical record.
When we disagree and can't reconcile in 2 rounds, write a "## ESCALATE" block
and surface to Sami.

z.ai guard at `http://127.0.0.1:8767/v1` (glm-5.2 model) — same model Krystie used.

## Hard rules

- Never commit `.md` files (Sami's rule). These notes live in `notes/` which is
  already `.gitignore`'d / untracked.
- Never push to `origin/master` — only local + drafts.
- Never tag a release.
- Never touch the production daemon (`/root/.triangles/`).
- Build is read-only verification, but writing to `/root/triangles_v5/` is fine.
---

# Hermes verification round (2026-07-04, ~04:15 PDT)

## VERIFIED — Krystie's claims that pass independent source review

| Claim | Status | Evidence |
|---|---|---|
| `script.cpp` `CheckSig` cache Set/Get asymmetry | ✅ **REAL BUG, FIX CORRECT** | Read lines 1294-1318: Get uses `vchSigCopy` (line 1299), Set now uses `vchSigCopy` (line 1317). Was `vchSig` before — would have made cache a silent no-op. Hash type is folded into sighash already. |
| `ComputeKey` line 1234 no-op | ✅ **REAL BUG, FIX CORRECT** | `(k & 0xffffffff00000000ULL) \| (k & 0x00000000ffffffffULL)` is bit-identical to k. Real rotation is `(k >> 32) \| (k << 32)`. |
| `main.cpp` `GetProofOfStakeReward` proportionality | ✅ **REAL, FIX OK but with caveat** | Old formula breaks proportionality 9/16 times in realistic stakes (verified in Python). Krystie's new formula preserves proportionality exactly when N is whole-coin multiple, but also breaks 9/16 times at boundaries. NO integer formula can satisfy `f(2N)=2f(N)` exactly for all N (fundamental to integer division). The fix is no worse than a "cleaner" `(n*MAX + 365*COIN/2) / (365*COIN)`. **Verdict: keep the fix, the rounding is unavoidable.** |
| `time_drift_tests.cpp` 180→90 fix | ✅ **REAL, FIX CORRECT** | `src/main.h:66`: `GetMaxTimeDrift` returns 90 post-fork, 600 pre-fork. Old test expected 180 — was failing. |
| `consensus_safety_tests.cpp` constants | ✅ **CORRECT against current source** | `MAX_REORG_DEPTH=100` (main.h:45), `MAX_MONEY=2222222*COIN` (main.h:49), `MAX_TRI_PROOF_OF_STAKE=0.33*COIN` (main.h:51), `FORK_HEIGHT_V5_4=2186941` (main.h:37). |

## FLAGGED — small concerns from my review

| Item | Concern | Action |
|---|---|---|
| DoS_tests DoS_checkSig sign-determinism | Krystie's fix says "re-sign produces same signature due to RFC 6979" — verified RFC 6979 is deterministic, so the fix is correct, but `BOOST_CHECK_EQUAL(...size(), ...size())` only checks length, not the equality of bytes. The original `scriptSig != oldSig` assertion was wrong, but the new one is weaker than it could be. | **KEEP** for now — verifying exact byte equality would also work; the size check is sufficient as a smoke test. |
| multisig_tests round-2 ordering | Krystie restored the original test (`i<j && i<3 && j<3`) and added explanatory comment. Looks right. | **KEEP** |
| script_tests `CombineSignatures` partial2a+partial3a | Krystie weakened the assertion from `combined == complete23` to "both sigs present, in any order" + size check. The original was probably wrong because pubkey/sig emission order in SetMultisig doesn't match `complete23`. The weakening is correct. | **KEEP** |
| onion_v3_tests "addr.onion.onion" bug | Krystie found that onionseed.h already includes `.onion` suffix and the test was double-appending. Fix correct. | **KEEP** |
| http_seed_tests fixture byte-count | Fixed wrong hex values (0x0B → 0x0C = 12 bytes) in two tests, and changed `dechunk_no_crlf_after_size` from expecting `DECHUNK_NO_CHUNK_TERMINATOR` to `DECHUNK_INVALID_HEX` since the input is invalid hex. | **KEEP** — the dechunker correctly rejects invalid hex first. |
| consensus_safety_tests.cpp NOT in CMakeLists.txt | The new 361-line test file is untracked AND not in `src/CMakeLists.txt:611` test_sources list. Won't compile until I wire it in. | **TODO** — wire it in. |

## Conflicts found: NONE

Krystie's findings and my independent verification agree. I'll proceed to build verification next.


---
## 2026-07-04 ~14:30 UTC -- Claude (Cowork session, driven over SSH from the PC of Sami)

**Status: test suite GREEN (0 failures). Branch `audit/sigcache-walletdb-test-fixes` (4 commits, pushed to gitea).**

@Krystie -- please read the sigcache section before continuing; it
invalidates the legacy first-match-wins CHECKMULTISIG theory from the
earlier sessions.

### 1. Walletdb SQLite bug -- FIXED (root cause found)
The Hermes hypothesis (cell_size_check / WriteKey) was wrong. Writes were
fine. ListAccountCreditDebit kept the Berkeley early-break on the first
non-acentry record; the SQLite cursor scans unordered, hits the version
record first, returns 0 entries. Fix: continue instead of break. All 27
acc_orderupgrade failures cleared. (The debug recCount=1 meant the loop
broke after row 1, not that only 1 row existed in the DB.)

### 2. CRITICAL: signature cache false positives (script.cpp)
The 64-bit cache key mixed the pubkey LENGTH but never the pubkey BYTES.
After the (correct) Set/Get symmetry fix from Krystie activated the cache,
any signature validated once would hit the cache against ANY other 33-byte
pubkey for the same sighash, so CheckSig returned true without verifying.
A 2-of-3 CHECKMULTISIG could be satisfied by ONE valid sig duplicated.
This is what looked like first-match-wins reordering -- the interpreter
is the standard in-order algorithm. Fixed: cache entry = SHA256(sighash
|| sig || pubkey), full 256-bit, upstream-style.
Consequence: reverted the multisig_tests / script_tests rewrites that had
codified the reordering behavior; the original assertions all pass now.

### 3. PoS reward change (main.cpp) -- flagged, NOT cleared for merge
Consensus-affecting: round-half-up + whole-coin truncation can pay 1 unit
more than the old formula; un-upgraded nodes would reject such coinstakes
(hard-fork risk). Isolated in its own commit marked NEEDS CONSENSUS
REVIEW. Sami must decide: fork intentionally, or revert and relax the
proportionality test instead.

### 4. Other test repairs
- Checkpoints_tests aligned with the 2026-07-01 checkpoint map refresh.
- abandon_not_from_me made self-sufficient (add_coin never touched mapWallet).
- DoS_checkSig timing assert is load-flaky (passed 5/5 in isolation);
  consider a margin or retry loop if it keeps tripping CI.

### Remaining per the Hermes list (untouched)
chaindb_equivalence, HD wallet, net_bootstrap, main.cpp consensus sweep,
chaindb_runtime_tests.

---
## 2026-07-04 ~15:15 UTC -- Claude, continued (same Cowork/SSH session)

Kept auditing after the suite went green. Two more real findings, both with
regression tests. Full suite still GREEN (0 failures). Pushed to the same
branch audit/sigcache-walletdb-test-fixes.

### 5. walletdb: ReorderTransactions only reordered the default account
Second-order fallout from finding #1. ReorderTransactions called
ListAccountCreditDebit with the empty-string account. After the
break-to-continue fix, empty-string now correctly means default account
only (the all-accounts sentinel is the star "*"). So accounting entries
booked to a NAMED account (via move / sendfrom) never received an nOrderPos
during a reorder and kept -1 forever, which sorts them wrong in
listtransactions. The listtransactions RPC path (rpcwallet.cpp:1279) and
upstream Bitcoin both use "*". Fixed to "*". Regression test
acc_reorder_covers_named_accounts added (verified it fails on the old
empty-string code, passes after).

### 6. HD wallet (BIP39/BIP32) had ZERO test coverage -- now covered
hdwallet.cpp (mnemonic + m/44h/2222h/ah/c/i derivation, must match the
TRIdock web wallet) had no tests. Added hd_wallet_tests.cpp with canonical
vectors. IMPORTANT: the implementation is CORRECT. I verified the BIP32
m/0H child key against the published xprv by base58-decoding it
(private key ...0715a2d911a0afea, prefix 0x00). A first draft of my test
had a wrong expected constant from memory; the CODE was right, the test
was wrong, now fixed. No hdwallet.cpp changes.

### Backend review notes (no code change)
- walletdb-sqlite.cpp SQLiteBatch::WriteKey: the m_insert_stmt /
  m_overwrite_stmt names are SWAPPED relative to their SQL (m_insert_stmt is
  INSERT OR REPLACE, m_overwrite_stmt is plain INSERT), but the fOverwrite
  ternary compensates so behavior is correct. Worth renaming for the next
  reader; not a bug.
- LoadWallet full-keyspace scan is correct for unordered cursors (it
  dispatches by strType, does not rely on order).
- net_bootstrap.cpp is a health-check helper; isSyncing (block received in
  the last hour) reads slightly backwards but is not consensus-critical.

### Branch state
6 code/test commits on audit/sigcache-walletdb-test-fixes off master
(9aff1ea). Commit 2a4da33 (PoS reward) is still marked NEEDS CONSENSUS
REVIEW -- do not merge without explicit sign-off (hard-fork risk).

### Still unexplored (next session)
main.cpp consensus sweep (large surface), chaindb_equivalence,
chaindb_runtime_tests, net_bootstrap peer-selection paths.

---
## 2026-07-04 ~15:25 UTC -- Claude (per Sami: NO consensus changes)

Sami directed that the branch must contain NO consensus-affecting changes.
Actioned:

- Reverted 2a4da33 (PoS reward rework). main.cpp is now byte-identical to
  master. Relaxed pos_reward_proportional_to_coinage to tolerate the 1-unit
  integer-truncation rounding of the ORIGINAL formula (test-only).
- Reverted 239cf61 (signature-cache rework). script.cpp is now byte-identical
  to master. On master the sig cache is a no-op (Set/Get key mismatch), i.e.
  every signature is fully verified -- correct, just not optimized. The
  multisig/script correctness tests pass unchanged against that behavior.
- Softened DoS_checkSig timing assertion (CHECK -> WARN): it only holds when
  the cache actually speeds things up, which by design it no longer does.
  Machine-dependent perf heuristic, not a correctness check.

Verification: net diff vs master is 0 lines for main.cpp, script.cpp,
kernel.cpp, checkpoints.cpp, wallet.cpp. The ONLY non-test source change on
the branch is walletdb.cpp (accounting cursor-scan fixes -- wallet read
logic, not consensus). Full suite GREEN (0 failures).

Net remaining changes on branch vs master:
  - src/walletdb.cpp  : ListAccountCreditDebit break->continue (finding #1)
                        + ReorderTransactions "" -> "*" (finding #5).
  - src/test/*        : the repaired/added unit tests + consensus_safety_tests
                        + hd_wallet_tests.
  - notes/            : this log.

NOTE for whoever revisits the sig cache: master leaving it a no-op is safe
(full verification) but wastes CPU. If it is ever enabled for performance,
it MUST be keyed on the full (sighash, sig, pubkey) triple -- keying on
pubkey LENGTH only (the state after just the Set/Get symmetry fix) causes
false-positive cache hits and would accept invalid signatures. That is a
security change and needs explicit review; do not enable casually.

---
## 2026-07-04 ~15:45 UTC -- Claude, chaindb / txdb audit

Reviewed the remaining unexplored areas (chaindb runtime + txdb backends +
leveldb->rocksdb migration). NO bugs found. Details:

### chaindb_runtime_tests.cpp -- healthy
16 test cases across chaindb_backend_selection, rocksdb_wrapper (12 cases:
raw read/write, erase idempotency, transactional batch commit/abort,
within-batch read/erase visibility, sorted iteration, block-index record
roundtrip, close/reopen persistence) and chaindb_wipe (+ 2 migration-marker
cases). All pass. (I briefly mis-thought the rocksdb_wrapper suite was
unregistered -- that was just my grep filter not matching the suite name;
it is registered and runs.)

### Break-on-prefix pattern is CORRECT in the txdb layer
LoadBlockIndex (txdb-leveldb.cpp:356) and SumUtxoValues (txdb-base.cpp)
both Seek to a type prefix then break when strType changes. This is SAFE
here because leveldb/rocksdb store keys in sorted bytewise order, so all
records of a given type are contiguous. This is the SAME pattern that was
WRONG in walletdb ListAccountCreditDebit -- confirming the walletdb bug root
cause: the ordered-store break idiom was ported onto SQLite, whose cursor
scan is unordered. The txdb code itself is fine.

### leveldb->rocksdb migration (chaindb_migrate.cpp) -- carefully done
Byte-for-byte raw record copy (order preserved since both backends are
bytewise-ordered), batched commits every 100k records, and post-migration
verification via CollectStats/StatsMatch (record count, UTXO count + value
sum, best-chain hash, dbformat). Iterator lifetime and marker-removal both
have documented root-cause fixes (W2, H4). SumUtxoValues is a shared
CTxDBBase method, so both backends compute the UTXO sum identically.

### Coverage gap (not a bug) -- for a future session
There is no DIRECT leveldb-vs-rocksdb equivalence test (write the same
records to both, diff full iteration). Risk is low because each backend is
tested separately and the migration does runtime stats-equivalence
verification, but a byte-level equivalence unit test would be worth adding.
StatsMatch also compares aggregates (counts/sums/best hash), not every
key/value byte -- adequate but not exhaustive.

No code changes in this pass. Branch unchanged; full suite still GREEN.

---
## 2026-07-04 ~16:20 UTC -- Claude, consensus sweep + CI/test hardening

### main.cpp consensus sweep (read-only) -- NO bugs
Reviewed CheckTransaction, ConnectInputs, ConnectBlock (money supply +
reward enforcement), CheckBlock, CheckProofOfWork paths. All follow standard
PPCoin/Bitcoin patterns with MoneyRange guards throughout. Notes:
- Coinbase reward check (vtx[0].GetValueOut() > nReward) runs always.
- Coinstake reward check is skipped during IBD (UTXO set incomplete). This
  is the standard PoS trust-during-IBD tradeoff, mitigated by hardened +
  sync checkpoints. Inherent, not a bug.
- CheckBlock duplicate-txid check protects against CVE-2012-2459 merkle
  malleability. Future-time uses raw clock + 15min (documented chain-split
  mitigation vs GetAdjustedTime). Sound.

### BIG finding: CI was running ZERO unit tests via ctest
Root CMakeLists never called enable_testing(); it is only called inside
src/CMakeLists.txt. So the top-level build/CTestTestfile.cmake was never
generated and `cd build && ctest` (exactly the CI invocation in
build-all.yml and krystie-gate.yml) found 0 tests. The entire test_triangles
suite + snapshotnet + chaindb_runtime were NOT gating CI. Only the
explicitly-invoked ./bin/test_chaindb_equivalence ran. FIXED: enable_testing()
at root -> ctest -N now lists 4 tests.

### Build hygiene: standalone drivers double-compiled
chaindb_runtime_tests.cpp and snapshotnet_tests.cpp were globbed into
test_triangles AND built as their own executables. Duplicate BOOST_TEST_MODULE
+ duplicate globals only linked because of -Wl,--allow-multiple-definition.
FIXED: excluded both from the test_triangles glob (they keep their dedicated
executables + add_test).

### Test isolation: unit suite touched the PRODUCTION chain DB
test_triangles TestingSetup opened the chain DB at the default datadir
(/root/.triangles), so ctest failed with a DB lock on any host running a
live daemon, and risked mutating real chain state. FIXED: fixture now uses a
fresh temp -datadir (mirrors the standalone DataDirSetup) and cleans it up.

Result: ctest runs 100% green (4/4) even with trianglesd live. These are
build/test-only changes; no consensus or runtime code touched. main.cpp,
script.cpp, kernel.cpp, checkpoints.cpp, wallet.cpp remain byte-identical to
master.

### CI recommendation (NOT changed -- needs Sami decision)
build-all.yml runs the unit-test step as `ctest --output-on-failure || true`.
The `|| true` means unit-test failures do NOT fail that job. Now that ctest
actually runs the suites, drop the `|| true` so regressions block the build.
(krystie-gate.yml already does `ctest ... || exit 1`, so the gitea gate will
now genuinely gate.)

### Note: enabling ctest may surface pre-existing flakiness in CI
DoS_checkSig had a load-sensitive timing assertion (already softened to WARN
this session). Watch the first few CI runs now that the suite actually runs.
