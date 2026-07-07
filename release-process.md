# Triangles Release Process

> Canonical release pipeline for `SamiAhmed7777/triangles_v5`. This document
> is the source of truth for *how* a release is cut. The implementation lives
> in `scripts/verify-reproducible-build.sh` and `scripts/sign-release.sh`.

## Goals

1. **Reproducible** — any two builders with the same source tree, same
   toolchain, and same flags produce byte-identical binaries.
2. **Signed** — every release artifact has a detached PGP signature that
   verifiers can check against a known public key.
3. **Verifiable end-to-end** — a third party can confirm a release is
   legitimate using only `gpg` and `sha256sum`, both installed by default
   on every Linux distribution.

## Pipeline overview

```
   source tag (e.g. v6.1.4)
            │
            ▼
   ┌─────────────────────┐
   │  CI builds all 4     │  .github/workflows/build-all.yml
   │  targets on each     │  (ubuntu / windows / macos)
   │  platform            │
   └──────────┬───────────┘
              │  produces: daemon.tar.gz, qt.tar.gz, .deb, .dmg, .exe, ...
              ▼
   ┌─────────────────────┐
   │  Local maintainer     │  scripts/sign-release.sh <release-dir>
   │  signs artifacts      │  (uses release signing key in local keyring)
   └──────────┬───────────┘
              │  produces: SHA256SUMS, *.asc detached signatures
              ▼
   ┌─────────────────────┐
   │  Push to GitHub       │  .github/workflows/distribute.yml
   │  release + Docker    │  (uploads artifacts, builds Docker image,
   │  + Homebrew tap +     │   updates Homebrew formula, submits
   │  WinGet + Snap        │   WinGet + Snap PRs)
   └──────────┬───────────┘
              │
              ▼
   ┌─────────────────────┐
   │  Verifier             │  scripts/sign-release.sh --verify <dir>
   │  independently        │  + gpg --import <release-pubkey>
   │  confirms             │
   └─────────────────────┘
```

## Reproducibility — how it works today

The Triangles build is already reproducible for Release builds with the
following properties:

| Property | Implementation |
|---|---|
| `BUILD_DESC` | Git describe output, written to `build.h` at build time |
| `BUILD_DATE` | **Commit timestamp** (NOT wall-clock), from `git log -n 1 --format=%ci` |
| `__DATE__`/`__TIME__` fallback | Dead code in practice — `build.h` always defines `BUILD_DATE` |
| Build paths in binaries | Currently absolute; should add `-ffile-prefix-map` for full reproducibility |

### Verifying reproducibility

Run on a clean checkout:

```bash
scripts/verify-reproducible-build.sh
```

This builds `trianglesd` twice into two separate build directories and
compares SHA256 hashes. Exits 0 on success.

Options:
- `BUILD_TYPE=Debug scripts/verify-reproducible-build.sh`
- `TARGET=triangles-qt scripts/verify-reproducible-build.sh`
- `BUILD_DIR_A=/tmp/A BUILD_DIR_B=/tmp/B scripts/verify-reproducible-build.sh`

## Signing — how it works

### Generate (or import) a release signing key

**One-time setup** (the maintainer's machine):

```bash
# Generate a fresh Ed25519 signing subkey under your existing PGP master.
# Ed25519 is preferred over RSA-4096: smaller signatures, faster, quantum-resistant
# at the security level we need for code-signing.
gpg --quick-generate-key 'Sami Ahmed <sami@cryptographic-triangles.org>' ed25519 sign never

# Print the public key block to publish on the website / GitHub.
gpg --armor --export 'sami@cryptographic-triangles.org' > release-pubkey.asc

# Export your secret key BACKUP. Store this on airgapped / offline media.
# Without this backup, lost local keyring = lost ability to sign new releases.
gpg --export-secret-keys 'sami@cryptographic-triangles.org' > release-seckey-BACKUP.asc
chmod 600 release-seckey-BACKUP.asc
```

**Import an existing key** (e.g. on a new maintainer machine):

```bash
gpg --import release-seckey-BACKUP.asc
```

### Sign a release directory

After CI has produced the artifacts in a known directory:

```bash
scripts/sign-release.sh /path/to/release-dir
```

This will:
1. Generate `SHA256SUMS` for every release artifact (.tar.gz, .deb, .dmg,
   .exe, .zip, .AppImage)
2. Write a detached PGP signature (`<artifact>.asc`) for each artifact
3. Write a detached PGP signature over `SHA256SUMS` itself
4. Refuse to run if the signing key isn't in the local keyring (safety)

### Verify a release

A third party (user, exchange, package maintainer) verifies with:

```bash
# 1. Import the public key (one-time).
gpg --import release-pubkey.asc

# 2. Verify everything in the release directory.
scripts/sign-release.sh --verify /path/to/release-dir
```

This checks:
- `SHA256SUMS.asc` against `SHA256SUMS` (the master signature)
- Each `<artifact>.asc` against its `<artifact>` (belt-and-suspenders)
- Each artifact's SHA256 against `SHA256SUMS` (integrity)

## Why both per-artifact signatures AND a SHA256SUMS signature?

- **SHA256SUMS + signature**: small, fast to verify, single point of trust.
  If the SHA256SUMS.asc checks out and a file's SHA256 matches an entry,
  you're done — you trust that entry.
- **Per-artifact signatures**: defense against a hypothetical attack where
  someone modifies `SHA256SUMS` but not the artifacts (or vice versa).
  Two independent signature chains.

For most verifiers, checking `SHA256SUMS.asc` + `sha256sum -c SHA256SUMS`
is sufficient. The per-artifact .asc files are insurance.

## CI integration

`.github/workflows/build-all.yml` already produces the artifacts. The
remaining work (separate PR) is to add a "sign" job that runs
`scripts/sign-release.sh` against the assembled release directory using a
key stored as a GitHub Actions secret.

**Required secrets (one-time setup in repo Settings → Secrets):**
- `GPG_PRIVATE_KEY` — base64-encoded `release-seckey-BACKUP.asc`
  (see [GitHub docs on encrypted secrets](https://docs.github.com/en/actions/security-guides/encrypted-secrets))
- `GPG_PASSPHRASE` — passphrase for the signing key (if any)
- `GITHUB_TOKEN` — already provided by Actions

**Suggested job sketch** (in `.github/workflows/build-all.yml` after all
build jobs complete):

```yaml
sign:
    name: Sign release artifacts
    needs: [build-linux-daemon, build-linux-qt, build-windows-daemon, build-windows-qt, build-macos]
    runs-on: ubuntu-22.04
    if: startsWith(github.ref, 'refs/tags/v')
    steps:
      - uses: actions/checkout@v4

      - name: Import signing key
        run: |
          echo "${{ secrets.GPG_PRIVATE_KEY }}" | base64 -d | gpg --import

      - name: Sign artifacts
        run: scripts/sign-release.sh release-artifacts/
        env:
          GPG_PASSPHRASE: ${{ secrets.GPG_PASSPHRASE }}
```

## Public key distribution

The release public key MUST be published in **at least three independent
places** so a keyserver takedown or DNS hijack cannot prevent verification:

1. **This repository** — `release-pubkey.asc` at the repo root, committed
   on every release tag.
2. **The website** — `https://cryptographic-triangles.org/release-pubkey.asc`
3. **Public keyservers** — submit to `keys.openpgp.org`, `keyserver.ubuntu.com`,
   `pgp.mit.edu`. Each is independently operated.

Distribution list refreshed with every key rotation (rare; treat as
multi-year commitment).

## Failure modes & recovery

| Scenario | Recovery |
|---|---|
| Signing key compromised | Revoke via pre-published revocation certificate. Re-cut release. Document incident. |
| Signing key lost (no backup) | Cannot sign new releases. Existing artifacts still verify against the published public key. Treat as catastrophic; re-mint a new key and treat the chain as fork-vulnerable until community updates. |
| Public key not yet distributed | User gets `gpg: Can't check signature: No public key`. Provide clear "first verify the key fingerprint out-of-band" instructions on the website. |
| CI secret leaked | Rotate the signing key immediately; treat all artifacts signed with the old key as suspect. |
| `SHA256SUMS` signed but artifacts don't match | `sha256sum -c` fails. Either an artifact was corrupted in transit, or someone tampered. Re-download from GitHub and re-verify. |

## Checklist for cutting a release

- [ ] Source tree is clean (no uncommitted changes)
- [ ] `scripts/verify-reproducible-build.sh` passes (builds are reproducible)
- [ ] All CI jobs on the release tag are green
- [ ] Release artifacts are in a single directory (`release-artifacts/`)
- [ ] `scripts/sign-release.sh release-artifacts/` runs without error
- [ ] `scripts/sign-release.sh --verify release-artifacts/` passes
- [ ] `release-pubkey.asc` is current and committed to the repo
- [ ] GitHub release created with all artifacts + SHA256SUMS + SHA256SUMS.asc
- [ ] `distribute.yml` workflow ran (Docker Hub, Homebrew, WinGet, Snap)
- [ ] Announcement posted (Twitter/Mastodon, Discord/Telegram, mailing list if any)

## Future work

- **Reproducibility hardening**: add `-ffile-prefix-map` to compile flags so
  absolute source paths don't leak into the binary (would also fix the
  simd.c:265 UBSan build-id drift).
- **Gitian-style deterministic builds**: containerized build environment
  pinned to a specific GCC/binutils version, so multiple independent
  verifiers can rebuild from source and get identical hashes.
- **Transparency log**: publish each release artifact hash to a Sigstore /
  sigsum / Certificate Transparency-style log so any tampering is publicly
  auditable.
- **Key rotation policy**: document how/when the signing key gets rotated
  (probably never, but state the policy).