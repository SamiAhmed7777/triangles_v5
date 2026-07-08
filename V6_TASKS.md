# TRI v6 Development Task Queue

*Autonomous development pipeline — Krystie cycles through these continuously.*

## Legend
- **P0** = Critical (chain broken / users blocked)
- **P1** = Important (v6 milestone)
- **P2** = Nice-to-have (polish / optimization)
- **Status**: TODO | IN-PROGRESS | DONE | BLOCKED

---

## P0 — Immediate (Unblock Chain & Users)

### T001: Fix DNS2 RPC thread crash
- **Status**: TODO
- **Depends**: none
- **Description**: ThreadRPCServer exits on bad auth attempts from external IPs. Need to not kill the RPC thread on individual auth failures.
- **Files**: `src/rpc.cpp` or `src/bitcoinrpc.cpp`
- **Acceptance**: RPC stays up even with bad auth attempts; curl JSON-RPC works reliably
- **Model**: Claude Code or MiniMax M2.7

### T002: Fix DNS2 wallet 0 confirmed balance
- **Status**: TODO
- **Depends**: T001 (need reliable RPC)
- **Description**: Wallet restored from April 20 backup. Shows 11.24 TRI unconfirmed. Need to verify rescan completes and coins mature (520 confirmations) for staking.
- **Files**: wallet.dat, `src/wallet.cpp`
- **Acceptance**: Wallet shows confirmed balance after rescan + confirmations
- **Model**: Krystie (manual investigation, not subagent)

### T003: Fix seeds.txt parsing (only returns 1 address)
- **Status**: TODO
- **Depends**: none
- **Description**: HTTPS fetch of seeds.cryptographic-triangles.org/seeds.txt only returns 1 address. Possible comment parsing bug in net.cpp seed fetch logic.
- **Files**: `src/net.cpp`, `/var/www/seeds/seeds.txt`
- **Acceptance**: All 7 onion addresses returned on fetch
- **Model**: ZAI GLM-5.1

### T004: Fix Sami's PC wallet block 570 stall
- **Status**: IN-PROGRESS
- **Depends**: Windows binary build (DONE — built on sami-pc)
- **Description**: Windows Qt wallet stuck at block 570. GUI bootstrap fix committed (d0fb2dc). New binary built at E:\repos\triangles_v5\build-mingw\bin\triangles-qt.exe. Needs testing.
- **Acceptance**: Windows wallet syncs past block 570 with bootstrap
- **Model**: Krystie (manual deployment)

---

## P1 — v6 Core Milestones

### T010: Complete RocksDB runtime testing
- **Status**: TODO
- **Depends**: T001
- **Description**: RocksDB backend compiles clean but never tested with actual blockchain data. Need to: start daemon with `-rocksdb`, let it index chain, verify block lookups work, compare performance vs LevelDB.
- **Files**: `src/txdb.h`, `src/txdb.cpp`, `src/utxosnapshot.cpp`
- **Acceptance**: Daemon runs with `-rocksdb` flag, processes blocks, RPC queries return correct data
- **Model**: MiniMax M2.7

### T011: Wire UTXO snapshot P2P distribution (SnapshotNet)
- **Status**: TODO
- **Depends**: T010
- **Description**: `snapshotnet.cpp` exists but is placeholder. Need to implement: peer advertisement of snapshot availability, chunk transfer protocol, hash verification, integration with bootstrap flow.
- **Files**: `src/snapshotnet.cpp`, `src/net.cpp`, `src/utxosnapshot.cpp`
- **Acceptance**: New node can get UTXO snapshot from peers via P2P (not just HTTPS)
- **Model**: Claude Code + MiniMax M2.7 (architecture + implementation)

### T012: Implement automated checkpoint generation (DESIGN DONE)
- **Status**: TODO
- **Depends**: none
- **Description**: Checkpoints exist through block 2,207,000 but are manually maintained. Need automated checkpoint generation: every N blocks, compute checkpoint hash, push to code or external manifest.
- **Files**: `src/checkpoints.cpp`, `src/checkpoints.h`
- **Acceptance**: New checkpoints generated automatically, committed or published
- **Model**: Claude Code

### T013: GPG signing for bootstrap artifacts
- **Status**: TODO
- **Depends**: none
- **Description**: GPG key created (6913E13610F698183429CE20C2DC60618C85A159). Need to: sign every bootstrap/snapshot artifact on generation, verify signature on download, publish public key.
- **Files**: `/usr/local/bin/auto-update.sh`, `src/bootstrap.cpp`
- **Acceptance**: `gpg --verify` works on downloaded artifacts
- **Model**: ZAI GLM-5.1

### T014: Contabo seed Docker image hardening
- **Status**: TODO
- **Depends**: none
- **Description**: Seeds are running but image is fragile. Need: proper Dockerfile with version pinning, health checks, auto-restart, log shipping, and persistent volumes.
- **Files**: `/tmp/Dockerfile` on Contabo, `/tri/seed-{1..4}/`
- **Acceptance**: Seeds survive host reboot, auto-restart on crash, health check endpoint
- **Model**: ZAI GLM-5.1

### T015: Network health dashboard
- **Status**: TODO
- **Depends**: T001, T003
- **Description**: Operator-facing dashboard showing: block height per node, peer count, staking weight, chain sync status, seed health. Could be a simple web page served from DNS2.
- **Files**: New — `src/rpcblockchain.cpp` (health endpoint), frontend
- **Acceptance**: Live page showing all 7 nodes' status updated every 30s
- **Model**: MiniMax M2.7 (design) + Claude Code (implementation)

### T016: Hetzner ARM64 persistent setup
- **Status**: TODO
- **Depends**: none
- **Description**: Hetzner node is running but manually configured. Need: systemd service, auto-start on boot, bootstrap automation, monitoring.
- **Files**: systemd unit file on Hetzner
- **Acceptance**: Node survives reboot, auto-syncs, reports health
- **Model**: Krystie (manual, it's infra not code)

---

## P2 — Polish & Optimization

### T024: PoS reward exact-proportionality rework — REJECTED
- **Status**: REJECTED
- **Depends**: none
- **Description**: Audit review of 2a4da33 (PoS reward rework, reverted by 05b5606) and 239cf61 (sigcache fix, reverted by 36d5f29) on 2026-07-07 concluded the PoS reward rework must stay reverted. Reasons: (1) consensus split risk — round-half-up pays 1 unit more than truncation for ~half of all inputs, so a block claiming that unit is valid to upgraded nodes and rejected by un-upgraded nodes; (2) motivation gone — the only driver was a unit-test assertion of exact proportionality (a78a420 already relaxed it to ±1 truncation), which is aesthetic, not correctness; (3) the new formula is worse than advertised — pre-truncating coin-age to whole-COIN units *before* multiplying drops fractional coin-age that the old formula credited, and `nWholeCoinAge * RATE * 2` is int64_t and can overflow. If exact proportionality is ever truly wanted, it must ship as a height-gated hard fork (both formulas in code, switch at activation height, coordinated node upgrade). Not worth it for cosmetic rounding. The sigcache fix from the same review (239cf61) was approved and re-landed in PR #21 / branch `fix/sigcache-false-positives` as a 6.1.6 candidate.
- **Files**: `src/main.cpp` (GetProofOfStakeReward)
- **Acceptance**: none — task is to leave the code as-is and not reopen

### T020: Remove unused Gemini/Google references from codebase
- **Status**: TODO
- **Depends**: none
- **Description**: Clean up any dead code, unused imports, stale comments referencing old architectures.
- **Model**: ZAI GLM-5.1

### T021: Comprehensive test suite
- **Status**: TODO
- **Depends**: T010
- **Description**: Expand test coverage for: UTXO snapshot load/dump, RocksDB backend, bootstrap download, seed fetch, checkpoint verification.
- **Files**: `src/test/`
- **Acceptance**: `test_triangles` passes with < 5 pre-existing failures
- **Model**: ZAI GLM-5.1 + MiniMax M2.7

### T022: CI/CD pipeline for releases
- **Status**: TODO
- **Depends**: none
- **Description**: GitHub Actions workflow: on tag push, build Linux x86_64 + ARM64 + Windows, create release with all binaries + checksums.
- **Files**: `.github/workflows/build-all.yml`
- **Acceptance**: Tag push produces release with 3 platform binaries
- **Model**: ZAI GLM-5.1

### T023: TRIdock + tri-wallet-web consolidation
- **Status**: TODO
- **Depends**: none
- **Description**: TRIdock and tri-wallet-web appear to be near-duplicates. Evaluate and either consolidate or clearly separate concerns.
- **Model**: MiniMax M2.7 (analysis)

---

## Completed

### ✅ Windows GUI bootstrap fix (d0fb2dc)
- Removed `#ifndef QT_GUI` guard so auto-bootstrap runs in GUI wallet
- Added `uiInterface.InitMessage()` for progress display

### ✅ Windows native build on sami-pc
- Built `triangles-qt.exe` (26MB) and `trianglesd.exe` via MSYS2/MinGW64
- All dependencies found natively

### ✅ RocksDB integration complete (ac9c6fb)
- CActiveTxDB wrapper, dual-backend support, compiles clean

### ✅ All nodes updated to v5.9.7.0
- DNS2, DNS3, Hetzner, Contabo seeds all running latest

### ✅ Bootstrap infrastructure live
- HTTPS at bootstrap.cryptographic-triangles.org
- Tor hidden service serving nginx on port 8085
- Seeds.txt with 7 onion nodes
