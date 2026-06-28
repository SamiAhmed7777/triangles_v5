#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
I2PD_SRC_DIR="${I2PD_SRC_DIR:-$ROOT_DIR/i2pd-src}"

if [[ ! -d "$I2PD_SRC_DIR" ]]; then
  echo "i2pd source tree not found at: $I2PD_SRC_DIR" >&2
  exit 1
fi

cd "$I2PD_SRC_DIR"

echo "Building libi2pd static libraries from: $I2PD_SRC_DIR"

NPROC_VAL="${NPROC:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)}"

# Detect the correct OpenSSL formula path on macOS. The i2pd
# Makefile.homebrew hardcodes openssl@3.5 but Homebrew may install
# openssl@3 instead. Command-line make variables override Makefile
# assignments, so passing SSLROOT=<detected> fixes the include path.
EXTRA_MAKE_ARGS=()
if [[ "$(uname -s)" == "Darwin" ]]; then
  if [[ -d "/opt/homebrew/opt/openssl@3" ]]; then
    SSLROOT="/opt/homebrew/opt/openssl@3"
  elif [[ -d "/usr/local/opt/openssl@3" ]]; then
    SSLROOT="/usr/local/opt/openssl@3"
  fi
  if [[ -n "${SSLROOT:-}" ]]; then
    echo "Detected OpenSSL at: $SSLROOT (overriding Makefile.homebrew)"
    EXTRA_MAKE_ARGS+=("SSLROOT=${SSLROOT}")
  fi
fi

# i2pd uses a hand-written Makefile system. We build only the static library
# targets (libi2pd.a, libi2pdclient.a, libi2pdlang.a), NOT the standalone
# i2pd daemon binary, which pulls in HTTPServer/I2PControl deps we don't need
# and can OOM on memory-constrained build machines.
make -j"$NPROC_VAL" USE_STATIC=no "${EXTRA_MAKE_ARGS[@]}" libi2pd.a libi2pdclient.a libi2pdlang.a

echo
echo "Build finished. Static libraries:"
ls -lh libi2pd*.a
echo
echo "Suggested next step for Triangles:"
echo "  cmake -DUSE_I2P_EMBEDDED=ON -DI2P_SOURCE_ROOT=src/i2p/i2pd-src .."
