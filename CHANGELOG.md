# Changelog

All notable changes to Triangles (TRI) are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [6.2.3] - 2026-08-01

### Changed
- **Local snapshot loading no longer requires a compiled-in SHA match.**
  Previously, loading `utxo-snapshot.bin` from the data dir rejected the
  file unless its SHA256 was present in `Checkpoints::mapSnapshotHashes`
  (which only knows about one or two canonical tips at compile time).
  Local file loads are operator-trusted — the operator already has
  filesystem access — so the SHA gate was friction without a security
  benefit. The gate still exists for P2P-delivered snapshots via
  `SnapshotNet` (requireCheckpoint=true there).

### Added
- `-acceptanylocalsnapshot` CLI flag: forces acceptance of a local
  `utxo-snapshot.bin` whose SHA is not in the compiled map, with an
  explicit warning log line. Use only with operator-signed snapshots.

## [6.2.2] - 2026-08-01


### Fixed
- **Snapshot regeneration: full chain index, not just the last 2000.**
  `UTXO_SNAPSHOT_DEFAULT_HEADERS` was 2000, which silently trimmed the
  snapshot to the last 2000 blocks even though the v2+ format is designed
  to carry the full chain index. The too-small snapshot caused
  `GetKernelStakeModifier() : block not indexed` errors after a fresh
  node loaded it — the kernel-stake-modifier walk in `CreateCoinStake`
  needs blocks older than the last 2000 because `nStakeModifierSelectionInterval`
  is multi-day. The block index was effectively unusable for the
  StakeMiner on the recovered node. Default is now 0 (all headers); the
  trim is bypassed when `nHeaders=0`. Callers may still pass an explicit
  positive value for a small diagnostic snapshot.


### Fixed
- **Build portability: v6.1.9 binary crashed with SIGILL on every
  production node.** v6.1.9 was built on GitHub Actions' EPYC 7763
  runner (AVX-512 capable). GCC 11.4 + libstdc++ inlining emitted 741
  `vpbroadcastq` EVEX instructions into the daemon binary even though
  the cmake `AddCompilerFlags.cmake` was setting `-march=x86-64-v2
  -mtune=generic`. The resulting binary crashed on every production
  CPU that lacks AVX-512: KVM-virtualized EPYC (DNS2), Ryzen 5 3600
  (SAMI-PC), and any non-x86_64 node. v6.2.0 adds an explicit
  `-mno-avx512f -mno-avx512*` block to the global compile options so
  the build cannot leak AVX-512 regardless of what the build host
  supports. Carries forward the v6.1.9 staking-selfheal fix unchanged.
  See `references/avx-512-sigill-build-fix.md` for the full diagnosis.

### Changed
- Bump version 6.1.9 → 6.2.0 to reflect the build-system change.

## [6.1.9] - 2026-07-31

### Fixed
- **Staking deadlock on idle networks.** `IsStakingSafe()` refused to
  stake whenever `IsInitialBlockDownload()` was true, and `IBD` flipped
  true whenever the chain tip was older than 24h. After 24h of no blocks,
  every node simultaneously refused to stake and the chain deadlocked.
  The `staking: true` flag in `getstakinginfo` was misleading — it only
  reflected a single search in the brief window after a restart. Narrowed
  the gate to "refuse only when IBD is true AND local height is behind
  the peer/checkpoint estimate" (`f69f087`). A node at the peer median
  now clears the gate and keeps staking through idle periods, so the
  chain self-heals. Genuinely-behind nodes still hold off. Block
  validation, reorg rules, and checkpoint rules are unchanged. The
  `-forcestaking` bootstrap escape hatch still works on nodes caught
  up to the checkpoint.

### Changed
- CLI: `-conf=` (empty value) now falls back to the default config
  path instead of erroring out (`41e3898`).
- CLI: `-conf` / `-datadir` / `-rpcuser` / `-rpcpassword` are honored
  in the documented order, with clearer error messages on bad input
  (`64556dc`).
- Build: reproducible build + signed release pipeline (PR #26 chain).

## [6.1.8] - 2026-07-17

### Changed
- Bootstrap: RPC-driven trusted snapshot publisher rotation (PR #26).
  Operators can rotate the snapshot publisher via RPC instead of
  hard-coding it in the binary.
- Consensus: removed local-finality, fixed `getheaders` fork recovery
  (`935d1d5`).
- Consensus: fail-closed reorg guard when the startup checkpoint
  pointer is null (`6116cff`).
- IBD: allow `getblocks`/`getheaders` on OneShot peers during IBD
  (`c68a8cb`).
- Build: bump revision 7 → 8.

### ⚠️ Known issue
- v6.1.8 introduced a staking deadlock on idle networks via the
  `IsStakingSafe()` gate. Operators on v6.1.8 should set
  `staking=1` and `forcestaking=1` in `triangles.conf` and restart
  to unstick the chain. v6.1.9 fixes the root cause.

## [6.1.7] - 2026-07-08

### Changed
- Overview page UI: the Total balance label is now rendered with
  `font-weight: 900` (full bold) instead of Qt's default bold (75,
  medium-bold). On builds where the font has a true heavy variant,
  the Total now visually pops as the headline number against the
  Spendable / Stake / Unconfirmed rows.
- Transactions amount column **Confirming tier color** is now
  `#4A8C5E` (mid green) instead of `#C5EBC9` (pale mint). The pale
  mint was too close to the bright `#7CDB8A` Confirmed green on
  the dark background and read as the same color. Mid green sits
  clearly between grey (Unconfirmed) and bright green (Confirmed)
  so the three tiers are visually distinct.
- Transactions amount column **now reads confirmation depth
  directly** (new `DepthRole` on `TransactionTableModel`) instead
  of going through the `TransactionStatus` enum. The rule fires on
  every block increment, not just on enum state transitions.
  Affects both `transactiontablemodel.cpp` (Transactions tab) and
  `overviewpage.cpp` (Overview recent-5 list).

## [6.1.6] - 2026-07-08

### Changed
- Overview page UI: conditional color on the **Total** balance label.
  Renders money-green (`#7CDB8A`) when the total is greater than zero
  and brand-red (`#e32105`) when the wallet is empty. Previously a
  static green stylesheet rule failed to cascade on some Qt builds,
  leaving Total always red.
- Transactions list (and Overview recent-5 list) **amount column** now
  uses a 3-tier color rule keyed off the existing `TransactionStatus`
  state machine, so the amount color agrees with the status icon:
  - 0 confirms (`Unconfirmed`) → grey (`#61280E`)
  - 1–3 confirms (`Confirming`) → pale mint (`#C5EBC9`)
  - 4+ confirms (`Confirmed`) → money-green (`#7CDB8A`)
  - Conflicted → grey
  - Negative amounts (spent) stay red across all tiers.
- Internal: added `COLOR_CONFIRMING` constant in `guiconstants.h`;
  rewired both amount paint sites
  (`overviewpage.cpp::TxViewDelegate::paint` and
  `transactiontablemodel.cpp::ForegroundRole`) to share the rule.

### Fixed
- `overviewpage.cpp` now includes `transactionrecord.h` so the
  `TransactionStatus::Confirming` enum value is in scope (was
  previously only forward-declared via `transactiontablemodel.h`).

## [6.1.5] - 2026-07-08

### Added
- New `tweet@sami-ahmed.net` uid on the maintainer signing key, with
  `hello@sami-ahmed.net` verified on the GitHub account — release tags now
  show as "Verified" on github.com.
- `CHANGELOG.md` at the repo root (this file).

### Changed
- Overview page UI: pending (`labelUnconfirmed`) and immature (`labelImmature`)
  balance labels now render in **olive green** (`#A8B847`) instead of the
  same light green as confirmed balances. The distinction reads as
  "incoming but not yet confirmed" instead of "incoming and final".
- `doc/release-process.md`: corrected signing-key identity to match the
  actual key in use (RSA-4096 `Krystie Triangles Release <krystie-triangles-release@dns2.sami.tailnet>`,
  not the Ed25519 `sami@cryptographic-triangles.org` the doc previously claimed).

### Fixed
- Wallet close-hang on Windows: detached `std::thread` instances backing the
  embedded Tor and I2P controllers now join cleanly on shutdown, removing
  the ~30s exit delay. (`#20`)
- Consensus: live proof-of-stake checks run during stale-tip IBD instead of
  being suppressed, fixing a divergence path where a node could accept a
  stale chain tip while local PoS validity checks were off. (`#18`)
- CI: `simd.c:265` UBSan build-id drift resolved; reproducible-build
  warnings now ignore untracked files. (`#17`)

### Security
- Audit follow-ups merged: kernel coverage, keystore coverage, sigcache
  fixes, wallet-DB test fixes. (`#14`, `#15`)

## [6.1.4] - 2026-07-04

### Fixed
- CI: Tor bundle download resilience.
- `NeedsBootstrap` flag now correctly persists across `rocksdb/` restarts.

## [6.1.3] - 2026-07-01

### Changed
- Chain-DB migration hardening.
- BIP39 passphrase support.
- HD-wallet indicator in the UI.
- Test isolation improvements.

## [6.1.2] - 2026-06-30 [YANKED]

Hotfix for v3 snapshot seek-offset corruption. Superseded by 6.1.3.
Do not use.

## [6.1.1] - 2026-06-22

### Fixed
- Minor wallet bugs.

## [6.1.0] - 2026-06-15

### Added
- Initial 6.x release line. C++20 modernization, embedded Tor/I2P support.

[6.1.7]: https://github.com/SamiAhmed7777/triangles_v5/compare/v6.1.6...v6.1.7
[6.1.6]: https://github.com/SamiAhmed7777/triangles_v5/compare/v6.1.5...v6.1.6
[6.1.5]: https://github.com/SamiAhmed7777/triangles_v5/compare/v6.1.4...v6.1.5
[6.1.4]: https://github.com/SamiAhmed7777/triangles_v5/compare/v6.1.3...v6.1.4
[6.1.3]: https://github.com/SamiAhmed7777/triangles_v5/compare/v6.1.2...v6.1.3
[6.1.2]: https://github.com/SamiAhmed7777/triangles_v5/compare/v6.1.1...v6.1.2
[6.1.1]: https://github.com/SamiAhmed7777/triangles_v5/compare/v6.1.0...v6.1.1
[6.1.0]: https://github.com/SamiAhmed7777/triangles_v5/releases/tag/v6.1.0
