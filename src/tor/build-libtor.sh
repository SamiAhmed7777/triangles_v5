#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOR_SRC_DIR="${TOR_SRC_DIR:-$ROOT_DIR/tor-src}"

if [[ ! -d "$TOR_SRC_DIR" ]]; then
  echo "Tor source tree not found at: $TOR_SRC_DIR" >&2
  exit 1
fi

cd "$TOR_SRC_DIR"

if [[ ! -x "./configure" ]]; then
  echo "Running autogen.sh"
  ./autogen.sh
fi

echo "Configuring Tor static library build from: $TOR_SRC_DIR"
./configure \
  --enable-static-tor \
  --disable-module-relay \
  --disable-module-dirauth \
  --disable-asciidoc \
  --disable-manpage \
  --disable-html-manual \
  --disable-unittests \
  --disable-tool-name-check

echo "Building Tor"
make -j"${NPROC:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)}"

echo
echo "Build finished. Inspect these locations for static libraries:"
echo "  $TOR_SRC_DIR/src/core"
echo "  $TOR_SRC_DIR/src/lib"
echo "  $TOR_SRC_DIR/src/trunnel"
echo
echo "Suggested next step for Triangles:"
echo '  make -f src/makefile.unix USE_TOR_EMBEDDED=1'
