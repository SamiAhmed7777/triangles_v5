#!/usr/bin/env bash
# sign-release.sh
#
# Sign Triangles release artifacts (the binaries/.debs/.dmgs/.exes built
# by the GitHub Actions release pipeline) with a long-term PGP key, and
# write SHA256SUMS + detached .asc signatures alongside each artifact.
#
# Usage:
#   scripts/sign-release.sh /path/to/release-dir
#   scripts/sign-release.sh /path/to/release-dir --key 0xDEADBEEF
#   scripts/sign-release.sh --verify /path/to/release-dir
#
# Inputs (in the release directory):
#   - *.tar.gz, *.deb, *.dmg, *.exe, *.zip, *.AppImage (any release artifact)
#   - SHA256SUMS file (if present, re-signed; if absent, generated)
#
# Outputs (written next to each artifact):
#   - <artifact>.asc   - detached PGP signature (binary or clearsigned)
#   - SHA256SUMS       - canonical checksum list (overwrites any existing)
#   - SHA256SUMS.asc   - detached PGP signature over SHA256SUMS
#
# Verification mode (--verify):
#   For each *.asc, runs `gpg --verify` against the artifact.
#   Then runs `sha256sum -c SHA256SUMS` if present.
#   Exits 0 if all artifacts verify; non-zero on any failure.
#
# Requirements:
#   - gpg2 or gpg on PATH
#   - Signing key already in the local keyring (or use --key to select)
#   - For verification: the signer's public key must be importable
#     (either already in the keyring, or fetched from a keyserver)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEFAULT_KEY="${TRIANGLES_RELEASE_KEY:-sami@cryptographic-triangles.org}"

usage() {
    sed -n '2,30p' "$0"
    exit "${1:-1}"
}

# ── Parse args ─────────────────────────────────────────────────────────────
MODE="sign"
RELEASE_DIR=""
SIGN_KEY="$DEFAULT_KEY"

while [ $# -gt 0 ]; do
    case "$1" in
        --verify)
            MODE="verify"
            shift
            ;;
        --key)
            SIGN_KEY="$2"
            shift 2
            ;;
        -h|--help)
            usage 0
            ;;
        *)
            RELEASE_DIR="$1"
            shift
            ;;
    esac
done

if [ -z "$RELEASE_DIR" ]; then
    echo "ERROR: release directory required" >&2
    usage 2
fi

if [ ! -d "$RELEASE_DIR" ]; then
    echo "ERROR: not a directory: $RELEASE_DIR" >&2
    exit 2
fi

cd "$RELEASE_DIR"

# ── Sign mode ──────────────────────────────────────────────────────────────
if [ "$MODE" = "sign" ]; then
    command -v gpg >/dev/null || { echo "ERROR: gpg not found" >&2; exit 3; }

    # Verify the signing key actually exists in the keyring (don't want to
    # silently create a new key with the same email).
    if ! gpg --list-secret-keys "$SIGN_KEY" >/dev/null 2>&1; then
        echo "ERROR: signing key '$SIGN_KEY' not found in local keyring" >&2
        echo "       import it first: gpg --import <keyfile>" >&2
        exit 3
    fi

    echo "Signing artifacts in $RELEASE_DIR with key $SIGN_KEY..."

    # Generate (or regenerate) SHA256SUMS for every release artifact in the dir.
    # Recognized extensions: .tar.gz, .deb, .dmg, .exe, .zip, .AppImage, .dmg.blockmap
    # Excludes: .asc files, SHA256SUMS itself, README/notes text files.
    ARTIFACTS=()
    while IFS= read -r -d '' f; do
        case "$f" in
            *.asc|SHA256SUMS|SHA256SUMS.asc|*.txt|*.md) continue ;;
        esac
        ARTIFACTS+=("$f")
    done < <(find . -maxdepth 1 -type f -print0 | sort -z)

    if [ ${#ARTIFACTS[@]} -eq 0 ]; then
        echo "ERROR: no release artifacts found in $RELEASE_DIR" >&2
        echo "       expected: .tar.gz, .deb, .dmg, .exe, .zip, .AppImage" >&2
        exit 4
    fi

    echo "  Found ${#ARTIFACTS[@]} artifact(s):"
    for a in "${ARTIFACTS[@]}"; do echo "    - $a"; done
    echo ""

    # Regenerate SHA256SUMS from scratch (deterministic sort).
    : > SHA256SUMS
    for a in "${ARTIFACTS[@]}"; do
        sha256sum "$a" >> SHA256SUMS
    done
    echo "✓ Wrote SHA256SUMS"

    # Detached signature over each artifact.
    for a in "${ARTIFACTS[@]}"; do
        rm -f "${a}.asc"
        if gpg --batch --yes \
               --local-user "$SIGN_KEY" \
               --armor --detach-sign \
               --output "${a}.asc" \
               "$a" 2>/dev/null; then
            echo "✓ Signed ${a}"
        else
            echo "✗ Failed to sign ${a}" >&2
            exit 5
        fi
    done

    # Detached signature over SHA256SUMS (this is what verifiers actually check
    # first; individual .asc files are belt-and-suspenders).
    rm -f SHA256SUMS.asc
    if gpg --batch --yes \
           --local-user "$SIGN_KEY" \
           --armor --detach-sign \
           --output SHA256SUMS.asc \
           SHA256SUMS 2>/dev/null; then
        echo "✓ Signed SHA256SUMS"
    else
        echo "✗ Failed to sign SHA256SUMS" >&2
        exit 5
    fi

    echo ""
    echo "Done. To verify from this directory:"
    echo "  gpg --verify SHA256SUMS.asc SHA256SUMS"
    echo "  sha256sum -c SHA256SUMS"
    echo ""
    echo "Or run: $0 --verify $RELEASE_DIR"
    exit 0
fi

# ── Verify mode ───────────────────────────────────────────────────────────
if [ "$MODE" = "verify" ]; then
    command -v gpg >/dev/null || { echo "ERROR: gpg not found" >&2; exit 3; }

    FAILED=0

    echo "Verifying signatures in $RELEASE_DIR..."
    echo ""

    # Verify SHA256SUMS.asc if present (this is the master signature).
    if [ -f SHA256SUMS ] && [ -f SHA256SUMS.asc ]; then
        if gpg --verify SHA256SUMS.asc SHA256SUMS 2>/dev/null; then
            echo "✓ SHA256SUMS signature: VALID ($(gpg --list-packets < SHA256SUMS.asc 2>/dev/null | grep -oP 'keyid \K[A-F0-9]+' | head -1 || echo unknown))"
        else
            echo "✗ SHA256SUMS signature: INVALID"
            FAILED=$((FAILED + 1))
        fi
    else
        echo "(no SHA256SUMS / SHA256SUMS.asc; skipping master signature)"
    fi

    # Verify each artifact's individual signature.
    while IFS= read -r -d '' asc; do
        artifact="${asc%.asc}"
        if [ ! -f "$artifact" ]; then
            echo "✗ $asc: artifact missing ($artifact)"
            FAILED=$((FAILED + 1))
            continue
        fi
        if gpg --verify "$asc" "$artifact" 2>/dev/null; then
            echo "✓ $artifact signature: VALID"
        else
            echo "✗ $artifact signature: INVALID"
            FAILED=$((FAILED + 1))
        fi
    done < <(find . -maxdepth 1 -name "*.asc" -not -name "SHA256SUMS.asc" -print0 | sort -z)

    # Verify checksums.
    if [ -f SHA256SUMS ]; then
        echo ""
        echo "Verifying checksums..."
        if sha256sum -c SHA256SUMS 2>&1 | tail -n +3; then
            :  # sha256sum -c outputs per-file status; aggregate below
        fi
        # Count any "FAILED" lines from sha256sum -c output.
        CHECKSUM_FAILS="$(sha256sum -c SHA256SUMS 2>&1 | grep -c ': FAILED' || true)"
        if [ "$CHECKSUM_FAILS" -gt 0 ]; then
            echo "✗ $CHECKSUM_FAILS checksum(s) FAILED"
            FAILED=$((FAILED + CHECKSUM_FAILS))
        else
            echo "✓ All checksums match SHA256SUMS"
        fi
    fi

    echo ""
    if [ "$FAILED" -eq 0 ]; then
        echo "✓ ALL VERIFICATIONS PASSED"
        exit 0
    else
        echo "✗ $FAILED VERIFICATION(S) FAILED"
        exit 1
    fi
fi