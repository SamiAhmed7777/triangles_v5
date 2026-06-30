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

# Scrub the rocksdb.pc that install-shared just wrote. RocksDB's
# Makefile unconditionally appends `-isystem third-party/gtest-1.8.1/
# fused-src` to Cflags, which is a RELATIVE path baked in from the build
# directory. Modern CMake (>= 3.27) refuses to consume imported targets
# with non-existent relative paths in INTERFACE_INCLUDE_DIRECTORIES,
# so pkg_check_modules(rocksdb) on a Triangles configure errors out
# with: 'Imported target "PkgConfig::RocksDB" includes non-existent
# path "third-party/gtest-1.8.1/fused-src"'.
#
# Replace the bad flag with the absolute include dir so pkg-config
# consumers see a path that actually exists on disk.
PC_FILE="${INSTALL_PREFIX}/lib/pkgconfig/rocksdb.pc"
if [ -f "${PC_FILE}" ]; then
    sed -i \
        -e "s|-isystem third-party/gtest-1.8.1/fused-src|-I${INSTALL_PREFIX}/include|g" \
        -e "s|-isystem \\\${prefix}/third-party/gtest-1.8.1/fused-src|-I${INSTALL_PREFIX}/include|g" \
        -e 's|-std=c++17 ||g' \
        -e 's|-std=c++17$||g' \
        "${PC_FILE}"
fi

ldconfig

# Sanity: installed library should be on disk and registered with ldconfig.
# ldconfig strips the patch version from its output, so we check both:
#   1. File exists at the versioned path (definitive).
#   2. ldconfig shows a matching major.minor (sanity for runtime linker).
ROCKSDB_MAJOR_MINOR="${ROCKSDB_VERSION%.*}"
if [ ! -f "${INSTALL_PREFIX}/lib/librocksdb.so.${ROCKSDB_VERSION}" ]; then
    echo "!!! librocksdb.so.${ROCKSDB_VERSION} not found at ${INSTALL_PREFIX}/lib/" >&2
    ls -l "${INSTALL_PREFIX}/lib/librocksdb"* 2>&1 || true
    exit 1
fi
if ! ldconfig -p | grep -q "librocksdb.so.${ROCKSDB_MAJOR_MINOR}"; then
    echo "!!! ldconfig did not register librocksdb.so.${ROCKSDB_MAJOR_MINOR}" >&2
    ldconfig -p | grep -i rocksdb >&2 || true
    exit 1
fi

echo ">>> RocksDB ${ROCKSDB_TAG} installed to ${INSTALL_PREFIX}"
echo ">>>  - library:  ${INSTALL_PREFIX}/lib/librocksdb.so.${ROCKSDB_VERSION}"
echo ">>>  - headers:  ${INSTALL_PREFIX}/include/rocksdb/version.h"
ls -l "${INSTALL_PREFIX}/lib/librocksdb.so"* "${INSTALL_PREFIX}/include/rocksdb/version.h"