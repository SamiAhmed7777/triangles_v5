#!/usr/bin/env bash
# scripts/ci/package-windows-daemon.sh
#
# Windows MSYS2 packaging step for the triangles daemon + CLI.
# Called from .github/workflows/build-all.yml build-windows-daemon step.
#
# Why a script file instead of inline YAML:
# The GitHub Actions msys2 shell wrapper has shown inconsistent handling of
# multi-line inline run: blocks under `set -e -o pipefail` (silent exits with
# code 1). A committed script file bypasses the YAML → shell translation
# quirks and gives us a known-good artifact that we can also run locally in
# MSYS2 for debugging.
#
# Usage: bash scripts/ci/package-windows-daemon.sh <dist-dir> <bin> [<bin> ...]
# Example: bash scripts/ci/package-windows-daemon.sh daemon-dist trianglesd triangles-cli

set -euo pipefail

DIST="${1:-daemon-dist}"
shift
BINS=("$@")

if [ "${#BINS[@]}" -eq 0 ]; then
    echo "Usage: $0 <dist-dir> <bin> [<bin> ...]" >&2
    echo "  e.g. $0 daemon-dist trianglesd triangles-cli" >&2
    exit 2
fi

echo ">>> Package step: bins=${BINS[*]} dist=${DIST}"

# Make the dist directory
mkdir -p "${DIST}/tor"

# Copy each binary to dist/
for bin in "${BINS[@]}"; do
    src="build/bin/${bin}.exe"
    if [ ! -f "${src}" ]; then
        echo "ERROR: ${src} not found" >&2
        exit 3
    fi
    cp "${src}" "${DIST}/"
    echo "  copied ${src} -> ${DIST}/"
done

# Copy linked DLLs (union of all binaries' dependencies, deduped)
echo ">>> Collecting DLLs from ldd output..."
ALL_DLLS="$(mktemp)"
trap 'rm -f "${ALL_DLLS}"' EXIT

for bin in "${BINS[@]}"; do
    src="build/bin/${bin}.exe"
    ldd "${src}" 2>/dev/null \
        | grep '/mingw64' \
        | awk '{print $3}' \
        >> "${ALL_DLLS}" || true
done

if [ ! -s "${ALL_DLLS}" ]; then
    echo "WARNING: no /mingw64 DLLs found in ldd output for ${BINS[*]}" >&2
else
    echo ">>> Copying $(sort -u "${ALL_DLLS}" | wc -l) unique DLLs..."
    sort -u "${ALL_DLLS}" | while IFS= read -r dll; do
        if [ -n "${dll}" ] && [ -f "${dll}" ]; then
            cp "${dll}" "${DIST}/" || echo "WARN: failed to copy ${dll}" >&2
        fi
    done
fi

echo ">>> Package complete: $(ls -1 "${DIST}" | wc -l) files in ${DIST}/"
ls -la "${DIST}/"
exit 0
