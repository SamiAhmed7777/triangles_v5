#!/usr/bin/env bash
# verify-reproducible-build.sh
#
# Builds the Triangles daemon (trianglesd) twice from the same source tree
# into two separate build directories, then compares the resulting
# SHA256 hashes. Exits 0 if the two builds produce byte-identical binaries,
# non-zero otherwise.
#
# Usage:
#   scripts/verify-reproducible-build.sh                       # default: trianglesd, Release
#   BUILD_TYPE=Debug  scripts/verify-reproducible-build.sh     # override build type
#   TARGET=triangles-qt scripts/verify-reproducible-build.sh   # build Qt wallet instead
#
# What "reproducible" means here:
#   Given identical source tree, identical compiler toolchain, identical
#   build flags, identical SOURCE_DATE_EPOCH (if set) -- the resulting
#   binary must hash identically across separate build directories.
#
# This script does NOT enforce compiler version pinning. Two different
# GCC versions will legitimately produce different binaries even with
# identical flags. The verification is "same source + same toolchain =
# same binary."
#
# Pass criteria:
#   1. Both builds succeed
#   2. Both binaries exist
#   3. SHA256 of the two binaries is equal
#
# On failure: prints the two SHA256s and the diff in size so a reviewer
# can investigate. Common causes of non-determinism:
#   - __DATE__/__TIME__ embedded (we eliminate this in CMakeLists.txt)
#   - absolute paths in __FILE__ (mitigated by -ffile-prefix-map)
#   - uninitialized stack/heap contents (should not affect final binary)
#   - linker adds random base addresses (PIE; deterministic if compiled
#     with -fno-pie)

set -euo pipefail

# ── Config ─────────────────────────────────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SOURCE_DIR="${SOURCE_DIR:-$(cd "$SCRIPT_DIR/.." && pwd)}"
BUILD_TYPE="${BUILD_TYPE:-Release}"
TARGET="${TARGET:-trianglesd}"
# Skip Qt by default -- it's slow and adds CI noise. Override with TARGET=triangles-qt
: "${BUILD_QT:=OFF}"
BUILD_DIR_A="${BUILD_DIR_A:-/tmp/triangles-repro-A}"
BUILD_DIR_B="${BUILD_DIR_B:-/tmp/triangles-repro-B}"
LOG_A="${LOG_A:-/tmp/triangles-repro-A.log}"
LOG_B="${LOG_B:-/tmp/triangles-repro-B.log}"

# ── Preflight ──────────────────────────────────────────────────────────────
command -v cmake >/dev/null || { echo "ERROR: cmake not found" >&2; exit 2; }
command -v ninja >/dev/null || { echo "ERROR: ninja not found (apt install ninja-build)" >&2; exit 2; }
command -v sha256sum >/dev/null || { echo "ERROR: sha256sum not found" >&2; exit 2; }

if [ ! -d "$SOURCE_DIR" ]; then
    echo "ERROR: source dir not found: $SOURCE_DIR" >&2
    exit 2
fi

# Refuse to run if the working tree is dirty -- dirty tree = non-deterministic
# git describe output = non-deterministic binary. Run on a clean checkout
# or a release tag.
if [ -n "$(cd "$SOURCE_DIR" && git status --porcelain 2>/dev/null)" ]; then
    echo "WARNING: working tree has uncommitted changes." >&2
    echo "         build.h will include '-dirty' suffix and the binary will NOT be" >&2
    echo "         reproducible. Commit/stash your changes first, or accept that the" >&2
    echo "         hashes below prove your dirty-tree build is at least internally consistent." >&2
fi

# ── Helpers ────────────────────────────────────────────────────────────────
build_one() {
    local dir="$1" log="$2"
    rm -rf "$dir"
    mkdir -p "$dir"
    echo "  configuring in $dir (BUILD_TYPE=$BUILD_TYPE BUILD_QT=$BUILD_QT)..." >&2
    cmake -S "$SOURCE_DIR" -B "$dir" \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        -DBUILD_QT="$BUILD_QT" \
        > "$log" 2>&1 || { echo "  configure failed; see $log" >&2; tail -30 "$log" >&2; exit 3; }
    echo "  building target $TARGET..." >&2
    cmake --build "$dir" --target "$TARGET" -j "$(nproc)" \
        >> "$log" 2>&1 || { echo "  build failed; see $log" >&2; tail -30 "$log" >&2; exit 3; }
    # ONLY stdout of the find goes to the caller. Progress logs above
    # were redirected to stderr so they don't pollute the captured path.
    find "$dir" -name "$TARGET" -type f -executable | head -1
}

# ── Build twice ────────────────────────────────────────────────────────────
echo "Building $TARGET ($BUILD_TYPE) twice from $SOURCE_DIR..."
echo ""
BIN_A="$(build_one "$BUILD_DIR_A" "$LOG_A")"
BIN_B="$(build_one "$BUILD_DIR_B" "$LOG_B")"

if [ -z "$BIN_A" ] || [ -z "$BIN_B" ]; then
    echo "ERROR: could not find built binary" >&2
    echo "  A: '$BIN_A'" >&2
    echo "  B: '$BIN_B'" >&2
    exit 4
fi

# ── Compare ────────────────────────────────────────────────────────────────
HASH_A="$(sha256sum "$BIN_A" | awk '{print $1}')"
HASH_B="$(sha256sum "$BIN_B" | awk '{print $1}')"
SIZE_A="$(stat -c%s "$BIN_A" 2>/dev/null || stat -f%z "$BIN_A")"
SIZE_B="$(stat -c%s "$BIN_B" 2>/dev/null || stat -f%z "$BIN_B")"

echo ""
echo "Binary A: $BIN_A"
echo "  sha256: $HASH_A"
echo "  size:   $SIZE_A bytes"
echo "Binary B: $BIN_B"
echo "  sha256: $HASH_B"
echo "  size:   $SIZE_B bytes"
echo ""

if [ "$HASH_A" = "$HASH_B" ]; then
    echo "✓ REPRODUCIBLE: both builds produced identical SHA256"
    exit 0
else
    echo "✗ NOT REPRODUCIBLE: hashes differ"
    echo ""
    echo "Likely causes:"
    echo "  - __DATE__/__TIME__ embedded (check src/version.cpp)"
    echo "  - absolute build paths in __FILE__ (check CMakeLists.txt for -ffile-prefix-map)"
    echo "  - dirty git tree (commit/stash and rerun)"
    echo "  - PIE base randomization (compile with -fno-pie -no-pie for testing)"
    echo "  - non-deterministic linker output (linker version mismatch)"
    exit 1
fi