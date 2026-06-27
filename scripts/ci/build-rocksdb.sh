#!/usr/bin/env bash
#
# build-rocksdb.sh — Build and install a pinned RocksDB version for CI.
#
# Ubuntu 22.04's librocksdb-dev is 6.11.4 (the same version that bit
# DNS2 — see PR #10). Triangles requires RocksDB >= 7.4.0 for the XXH3
# per-block checksum used in modern smsgDB SST files; src/smessage.cpp's
# SecMsgDB::Open has a runtime quarantine fallback, but the build-time
# check in CMakeLists.txt refuses to configure against < 7.4.
#
# This script clones RocksDB at a pinned tag, builds only the shared
# library (fast), installs to /usr/local, and refreshes ldconfig.
# Triangles' CMake find_library probes /usr/local before /usr/lib so
# the just-built copy is picked up first.
#
# Pinned version matches DNS2's system librocksdb (8.9.1) so test
# coverage matches production.
#
# Usage:  sudo ./scripts/ci/build-rocksdb.sh
set -euo pipefail

ROCKSDB_VERSION="${ROCKSDB_VERSION:-8.9.1}"
ROCKSDB_TAG="v${ROCKSDB_VERSION}"
INSTALL_PREFIX="${INSTALL_PREFIX:-/usr/local}"
JOBS="${JOBS:-$(nproc)}"

WORKDIR="$(mktemp -d)"
trap 'rm -rf "$WORKDIR"' EXIT

echo ">>> Building RocksDB ${ROCKSDB_TAG} (${JOBS} jobs) into ${INSTALL_PREFIX}"

git clone --depth 1 --branch "${ROCKSDB_TAG}" \
    https://github.com/facebook/rocksdb.git "${WORKDIR}/rocksdb"

cd "${WORKDIR}/rocksdb"

# Shared library only — Triangles links dynamically. Statically linking
# rocksdb.a would also work but balloons the daemon binary by ~50 MB.
make -j"${JOBS}" shared_lib PORTABLE=1 USE_RTTI=1 \
    EXTRA_CXXFLAGS="-Wno-error=deprecated-declarations"

make install-shared PREFIX="${INSTALL_PREFIX}"

ldconfig

# Sanity: installed library should be loadable and export the version
# macros we look for at configure time.
if ! ldconfig -p | grep -q "librocksdb.so.${ROCKSDB_VERSION}"; then
    echo "!!! ldconfig did not pick up librocksdb.so.${ROCKSDB_VERSION}" >&2
    exit 1
fi

echo ">>> RocksDB ${ROCKSDB_TAG} installed to ${INSTALL_PREFIX}"
echo ">>>  - library:  ${INSTALL_PREFIX}/lib/librocksdb.so.${ROCKSDB_VERSION}"
echo ">>>  - headers:  ${INSTALL_PREFIX}/include/rocksdb/version.h"
ls -l "${INSTALL_PREFIX}/lib/librocksdb.so"* "${INSTALL_PREFIX}/include/rocksdb/version.h"