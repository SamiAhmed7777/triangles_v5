#!/usr/bin/env bash
# Regenerate the vendored tor configure from the pinned tor submodule.
#
# Run this when:
#   - The tor submodule is updated (src/tor/tor-src) to a new commit
#   - We need to test if a newer autoconf emits a parseable script
#
# Requirements:
#   - autoconf 2.71 (other versions emit patterns bash 4.4 / dash
#     cannot parse — see ../src/tor/build-libtor.sh history for details)
#   - The tor submodule must be initialized
#
# Output: ./configure.vendored in this directory
#
# Usage: bash src/tor/regenerate-tor-configure.sh

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOR_SRC_DIR="$ROOT_DIR/tor-src"

if [[ ! -d "$TOR_SRC_DIR" ]]; then
  echo "Tor source tree not found at: $TOR_SRC_DIR" >&2
  echo "Run: git submodule update --init --recursive" >&2
  exit 1
fi

# Find autoconf 2.71
AUTOCONF_BIN=""
for candidate in /tmp/autoconf271/bin/autoconf /usr/local/bin/autoconf-2.71 \
                  /opt/autoconf/2.71/bin/autoconf; do
  if [[ -x "$candidate" ]]; then
    AUTOCONF_BIN="$candidate"
    break
  fi
done

if [[ -z "$AUTOCONF_BIN" ]]; then
  if command -v autoreconf-2.71 >/dev/null 2>&1; then
    AUTOCONF_BIN="$(command -v autoreconf-2.71)"
  elif command -v autoconf-2.71 >/dev/null 2>&1; then
    AUTOCONF_BIN="$(command -v autoconf-2.71)"
  fi
fi

if [[ -z "$AUTOCONF_BIN" ]]; then
  echo "autoconf 2.71 not found. Install with:" >&2
  echo "  cd /tmp && wget -q https://ftp.gnu.org/gnu/autoconf/autoconf-2.71.tar.xz && \\" >&2
  echo "    tar xf autoconf-2.71.tar.xz && cd autoconf-2.71 && \\" >&2
  echo "    ./configure --prefix=/tmp/autoconf271 && make -j\$(nproc) install" >&2
  exit 1
fi

# Make autoreconf use 2.71. The versioned binary might be named
# autoreconf2.71 or autoreconf-2.71 depending on how it was built.
AUTORECONF_BIN="$(dirname "$AUTOCONF_BIN")/autoreconf$(echo "$AUTOCONF_BIN" | sed 's/.*autoconf/autoreconf/')"
if [[ ! -x "$AUTORECONF_BIN" ]]; then
  AUTORECONF_BIN="$(dirname "$AUTOCONF_BIN")/autoreconf"
fi

echo "Using AUTOCONF_BIN=$AUTOCONF_BIN"
echo "Using AUTORECONF_BIN=$AUTORECONF_BIN"

cd "$TOR_SRC_DIR"

# Wipe any existing generated files to ensure a clean regenerate.
rm -f configure configure.ac~
"$AUTORECONF_BIN" -i -f -W no-error

if [[ ! -x ./configure ]]; then
  echo "autoreconf did not produce ./configure" >&2
  exit 1
fi

# Sanity-check the output: warn if it contains patterns that bash 4.4
# or dash cannot parse. If you see warnings, do NOT commit the result.
WARN=0
if grep -qE '^\s*\S+=\`' configure; then
  echo "WARNING: configure still contains backtick command substitutions" >&2
  WARN=1
fi
if grep -qE '\$\{ac_cv_func_\$\{ac_func\}\+y\}' configure; then
  echo "WARNING: configure still contains nested \${ac_cv_func_\${ac_func}+y}" >&2
  WARN=1
fi
if grep -qE '\$\{ac_cv_func_\$ac_func\+y\}' configure; then
  echo "WARNING: configure still contains single-dollar \${ac_cv_func_\$ac_func+y}" >&2
  WARN=1
fi

# Parse-check in the shells CI cares about
echo "Parse-checking configure in: bash $(bash --version | head -1 | awk '{print $4}'), dash, /tmp/bash44 (if present)..."
for sh in bash dash; do
  if command -v "$sh" >/dev/null 2>&1; then
    if ! "$sh" -n ./configure 2>/dev/null; then
      echo "ERROR: configure fails to parse in $sh" >&2
      "$sh" -n ./configure 2>&1 | head -5 >&2
      exit 1
    else
      echo "  $sh: OK"
    fi
  fi
done
if [[ -x /tmp/bash44/bin/bash ]]; then
  if ! /tmp/bash44/bin/bash -n ./configure 2>/dev/null; then
    echo "ERROR: configure fails to parse in bash 4.4 (MSYS2 version)" >&2
    exit 1
  else
    echo "  bash 4.4: OK"
  fi
fi

OUT="$ROOT_DIR/configure.vendored"
cp -f ./configure "$OUT"
chmod +x "$OUT"
echo
echo "Wrote $OUT ($(wc -c < "$OUT") bytes)"
if [[ $WARN -ne 0 ]]; then
  echo
  echo "DO NOT COMMIT this file — it has parse hazards. See warnings above." >&2
  exit 2
fi
echo "Safe to commit."
