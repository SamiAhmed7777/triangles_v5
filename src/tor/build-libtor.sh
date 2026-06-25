#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOR_SRC_DIR="${TOR_SRC_DIR:-$ROOT_DIR/tor-src}"

if [[ ! -d "$TOR_SRC_DIR" ]]; then
  echo "Tor source tree not found at: $TOR_SRC_DIR" >&2
  exit 1
fi

cd "$TOR_SRC_DIR"

if [[ ! -x "./configure" ]] || [[ "${AUTORECONF_FORCE:-0}" == "1" ]]; then
  echo "Running autoreconf with -W no-error (autogen.sh -W all,error is too strict for autoconf 2.73+)"
  autoreconf -i -f -W no-error

  # autoconf 2.73 generates ./configure with backtick command
  # substitutions inside variable assignments such as
  #   as_ac_var=`printf '%s\n' "ac_cv_func_$ac_func" | sed "$as_sed_sh"`
  # bash on MSYS2/MINGW64 parses this with a syntax error at the
  # nested backticks with quoted $-vars. autoconf 2.71 doesn't
  # generate this pattern, but pinning the MSYS2 autoconf version
  # is fragile (meta-package pulls whatever's current). Convert
  # all `var=\`cmd\`` assignments to `var=$(cmd)` form, which is
  # POSIX-ly equivalent and nests cleanly.
  if grep -qE '^[ \t]*[A-Za-z_][A-Za-z0-9_]*=`[^`]*`[ \t]*$' configure; then
    echo "Patching generated configure: \`...\` -> \$(...) in assignments"
    # Match leading whitespace, identifier, =, single backtick,
    # any chars except backtick, single backtick, optional whitespace.
    perl -i -pe 's/^([ \t]*[A-Za-z_][A-Za-z0-9_]*=)`([^`]*)`([ \t]*)$/$1\$($2)$3/' configure \
      || echo "perl not available, falling back to sed"
  fi
fi

echo "Configuring Tor static library build from: $TOR_SRC_DIR"
# Even after the perl patch above, the configure script's shebang
# is `#!/bin/sh` and MSYS2's /bin/sh is dash. Force bash so any
# remaining edge cases (nested quoting, $RANDOM usage, etc.) parse
# the same way on every platform.
export CONFIG_SHELL="${CONFIG_SHELL:-$(command -v bash)}"
"$CONFIG_SHELL" ./configure \
  --enable-static-tor \
  --disable-module-relay \
  --disable-module-dirauth \
  --disable-asciidoc \
  --disable-manpage \
  --disable-html-manual \
  --disable-unittests \
  --disable-tool-name-check \
  --with-libevent-dir="${LIBEVENT_DIR:-/mingw64}" \
  --with-openssl-dir="${OPENSSL_DIR:-/mingw64}" \
  --with-zlib-dir="${ZLIB_DIR:-/mingw64}"

echo "Building Tor (libtor.a only — skip the helper tools like tor-resolve"
echo "and tor-print-ed-signing-cert that pull in extra static OpenSSL and"
echo "are not needed by Triangles)"
make -j"${NPROC:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)}" libtor.a

echo
echo "Build finished. Inspect these locations for static libraries:"
echo "  $TOR_SRC_DIR"
echo "  $TOR_SRC_DIR/src/lib"
echo
echo "Suggested next step for Triangles:"
echo '  make -f src/makefile.unix USE_TOR_EMBEDDED=1 TOR_SOURCE_ROOT=src/tor/tor-src'
