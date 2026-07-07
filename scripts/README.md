# Scripts

Operational scripts for the Triangles project. See also `release-process.md`
at the repo root for the canonical release pipeline documentation.

## Build verification

- **`verify-reproducible-build.sh`** — builds the daemon (or another target)
  twice from the same source tree and verifies the SHA256 hashes match.
  Catches accidental introduction of non-determinism (e.g. `__DATE__`/`__TIME__`
  regressions, dirty git state, PIE base-address drift).

## Release signing

- **`sign-release.sh`** — generates `SHA256SUMS`, writes detached PGP
  signatures (`.asc`) over each release artifact and over `SHA256SUMS`.
  Supports `--verify` for independent third-party verification.
  Uses `TRIANGLES_RELEASE_KEY` env var (defaults to
  `sami@cryptographic-triangles.org`).

## Existing infrastructure

- **`bump-version.sh`** — sync version numbers across all manifests from
  `src/clientversion.h`.
- **`sign-snapshot.sh`** — sign a UTXO snapshot file with the wallet's
  signing address (not a PGP key; this is a chain-level signature, not a
  release signature).
- **`validate_onion_seeds.py`** — validate every `.onion` address in
  `triangles.conf` against the v3 hidden-service checksum.
- **`ibd-smoke-test.sh`** — fresh-datadir IBD smoke test for catching the
  classic "stalls early / loops around 570" failure mode.
- **`ci/build-rocksdb.sh`** — build and install a pinned RocksDB version
  for CI.
- **`ci/package-linux-daemon.sh`** — Linux packaging step (.deb).
- **`ci/package-windows-daemon.sh`** — Windows packaging step.
- **`tri/`** — operator-facing CLI for node administration.
