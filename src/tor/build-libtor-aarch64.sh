#!/usr/bin/env bash
# Cross-compile libtor.a for aarch64
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOR_SRC_DIR="${TOR_SRC_DIR:-$ROOT_DIR/tor-src}"

if [[ ! -d "$TOR_SRC_DIR" ]]; then
  echo "Tor source tree not found at: $TOR_SRC_DIR" >&2
  exit 1
fi

cd "$TOR_SRC_DIR"

# Use vendored configure (same as the native build)
VENDORED_CONFIGURE="$ROOT_DIR/configure.vendored"
VENDORED_AUX_DIR="$ROOT_DIR/configure-aux"
VENDORED_INPUT_DIR="$ROOT_DIR/configure-input"

if [[ -f "$VENDORED_CONFIGURE" ]]; then
  echo "Using vendored configure from $VENDORED_CONFIGURE"
  cp -f "$VENDORED_CONFIGURE" "./configure"
  chmod +x ./configure
  if [[ -d "$VENDORED_AUX_DIR" ]]; then
    cp -f "$VENDORED_AUX_DIR"/* ./
    chmod +x ./ar-lib ./compile ./config.guess ./config.sub \
             ./depcomp ./install-sh ./missing ./test-driver 2>/dev/null || true
  fi
  if [[ -d "$VENDORED_INPUT_DIR" ]]; then
    cp -rf "$VENDORED_INPUT_DIR"/. ./
    find . -name '*.in' -o -name 'aclocal.m4' | xargs touch 2>/dev/null || true
  fi
fi

echo "=== Configuring Tor for aarch64 cross-compile ==="
CC=aarch64-linux-gnu-gcc \
CXX=aarch64-linux-gnu-g++ \
AR=aarch64-linux-gnu-ar \
RANLIB=aarch64-linux-gnu-ranlib \
STRIP=aarch64-linux-gnu-strip \
./configure \
  --host=aarch64-linux-gnu \
  --disable-asciidoc \
  --disable-manpage \
  --disable-html-manual \
  --disable-system-torrc \
  --disable-systemd \
  --disable-lzma \
  --disable-zstd \
  --disable-nss \
  --enable-pic \
  --enable-static-libevent \
  --with-openssl-dir=/usr \
  --with-libevent-dir=/usr \
  --with-zlib-dir=/usr \
  LIBS="-L/usr/lib/aarch64-linux-gnu" \
  CPPFLAGS="-I/usr/include" \
  LDFLAGS="-L/usr/lib/aarch64-linux-gnu"

echo "=== Building libtor.a for aarch64 ==="
make -j$(nproc) libor.a libtor.a 2>&1 || make -j$(nproc) 2>&1

echo "=== Result ==="
ls -lh "$TOR_SRC_DIR/libtor.a" 2>/dev/null && echo "SUCCESS: libtor.a built for aarch64" || echo "FAILED"
file "$TOR_SRC_DIR/libtor.a" 2>/dev/null
