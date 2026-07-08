# Changelog

All notable changes to Triangles (TRI) are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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

[6.1.5]: https://github.com/SamiAhmed7777/triangles_v5/compare/v6.1.4...v6.1.5
[6.1.4]: https://github.com/SamiAhmed7777/triangles_v5/compare/v6.1.3...v6.1.4
[6.1.3]: https://github.com/SamiAhmed7777/triangles_v5/compare/v6.1.2...v6.1.3
[6.1.2]: https://github.com/SamiAhmed7777/triangles_v5/compare/v6.1.1...v6.1.2
[6.1.1]: https://github.com/SamiAhmed7777/triangles_v5/compare/v6.1.0...v6.1.1
[6.1.0]: https://github.com/SamiAhmed7777/triangles_v5/releases/tag/v6.1.0
