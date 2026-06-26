#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOR_SRC_DIR="${TOR_SRC_DIR:-$ROOT_DIR/tor-src}"

if [[ ! -d "$TOR_SRC_DIR" ]]; then
  echo "Tor source tree not found at: $TOR_SRC_DIR" >&2
  exit 1
fi

cd "$TOR_SRC_DIR"

# Prefer the vendored configure (../configure.vendored, committed to
# this repo). It was generated with autoconf 2.71 on Linux, which emits
# a known-good bash/dash-compatible script that does NOT contain:
#   - backtick command substitutions (Patch 1)
#   - the `${ac_cv_func_${ac_func}+y}` nested-expansion form (Patches 4/5)
#   - the `printf "%s\n" "ac_cv_func_$ac_func" | $as_tr_sh` form (Patches 2/3)
# Skipping autoreconf on the CI runner eliminates the entire
# MSYS2/autoconf-wrapper/dash/bash interaction that was producing
# `${ac_cv_func_ RtlSecureZeroMemory+y}: bad substitution` at line 2220.
#
# To regenerate (Linux only — needs autoconf 2.71):
#   bash src/tor/regenerate-tor-configure.sh
# To force a fresh autoreconf on the runner instead (legacy behavior):
#   AUTORECONF_FORCE=1 bash src/tor/build-libtor.sh
VENDORED_CONFIGURE="$ROOT_DIR/configure.vendored"
VENDORED_AUX_DIR="$ROOT_DIR/configure-aux"
VENDORED_INPUT_DIR="$ROOT_DIR/configure-input"
if [[ -f "$VENDORED_CONFIGURE" ]] && [[ "${AUTORECONF_FORCE:-0}" != "1" ]]; then
  echo "Using vendored configure from $VENDORED_CONFIGURE"
  cp -f "$VENDORED_CONFIGURE" "./configure"
  chmod +x "./configure"
  # configure looks for auxiliary files (ar-lib, config.guess,
  # config.sub, compile, depcomp, install-sh, missing, test-driver)
  # in the same directory as itself. autoreconf -i normally creates
  # them, but we skipped autoreconf — so vendor them alongside.
  if [[ -d "$VENDORED_AUX_DIR" ]]; then
    cp -f "$VENDORED_AUX_DIR"/* ./
    chmod +x ./ar-lib ./compile ./config.guess ./config.sub \
             ./depcomp ./install-sh ./missing ./test-driver 2>/dev/null || true
    echo "Vendored $(ls "$VENDORED_AUX_DIR" | wc -l) auxiliary files"
  fi
  # configure also reads AC_CONFIG_FILES inputs (Makefile.in,
  # Doxyfile.in, torrc.sample.in, etc.) from the source tree.
  # automake normally generates these from *.am files. Since we
  # skipped autoreconf, vendor the .in files too. We preserve the
  # directory structure (e.g. src/config/torrc.sample.in) because
  # configure looks for them at their original paths.
  # aclocal.m4 is also vendored because the generated Makefile has a
  # rule to regenerate it from acinclude.m4 + m4/*.m4 — which would
  # invoke aclocal on the runner (an automake dependency we don't
  # want to install there). With aclocal.m4 vendored, the rule's
  # dependency check sees an up-to-date file and skips regeneration.
  if [[ -d "$VENDORED_INPUT_DIR" ]]; then
    cp -rf "$VENDORED_INPUT_DIR"/. ./
    # Make the vendored files appear newer than the source files
    # (configure.ac, acinclude.m4, m4/*.m4) so make's dependency
    # tracking doesn't try to regenerate anything.
    find "$VENDORED_INPUT_DIR" -type f -exec touch -r "$VENDORED_INPUT_DIR/aclocal.m4" {} +
    touch -r "$VENDORED_INPUT_DIR/aclocal.m4" aclocal.m4
    echo "Vendored $(find "$VENDORED_INPUT_DIR" -type f | wc -l) configure input files"
  fi
elif [[ "${AUTORECONF_FORCE:-0}" == "1" ]] || [[ ! -x "./configure" ]]; then
  echo "Running autoreconf with -W no-error (autogen.sh -W all,error is too strict for autoconf 2.73+)"
  # Prefer autoconf 2.71 when available. autoreconf 2.73 emits
  # configure patterns that bash on MSYS2/MINGW64 chokes on even
  # after the patches below. autoreconf 2.71 emits clean backtick
  # assignments; it is installed as a side effect of
  # mingw-w64-x86_64-autotools on MSYS2 but autoconf-wrapper still
  # picks 2.73 unless we call the versioned binary directly.
  if command -v autoreconf-2.71 >/dev/null 2>&1; then
    AUTORECONF=autoreconf-2.71
  else
    AUTORECONF=autoreconf
  fi
  "$AUTORECONF" -i -f -W no-error

  # Apply both configure patches via a single perl script. We write
  # the script to /tmp first to avoid the quoting nightmare of nested
  # single quotes inside bash single-quoted strings.
  #
  # CRITICAL perl replacement gotchas (cost me several iterations):
  #   - `$(` in the replacement source is parsed by perl as `$$` (process
  #     ID). Use `\$(` to emit a literal `$(`.
  #   - `\n` in the replacement source is parsed by perl as a newline.
  #     Use `\\n` to emit a literal backslash-n.
  # We avoid these entirely by building replacement strings with
  # sprintf() and %s placeholders, so perl never sees the dollar
  # signs or backslashes that would trigger interpolation.
  cat > /tmp/patch-tor-configure.pl <<'PERL_EOF'
use strict;
use warnings;
local $/;
open(my $fh, "<", "configure") or die "open: $!";
my $s = <$fh>;
close($fh);
my $before = $s;

# Patch 1: convert single-line backtick assignments.
#   `var=`cmd``  ->  `var=$(cmd)`
# Exclude newlines from the content class so we don't greedily match
# multi-line backtick command substitutions (which would break their
# internal paren balance). sprintf here is safe — $1/$2/$3 are backrefs.
$s =~ s/^([ \t]*[A-Za-z_][A-Za-z0-9_]*=)`([^`\n]*)`([ \t]*$)/sprintf('%s$(%s)%s', $1, $2, $3)/egm;

# Patch 2 + 3 (combined): AC_CHECK_FUNCS printf format and as_tr_sh.
# Replace:
#   $(printf "%s\n" "ac_cv_func_$ac_func" ...)  ->
#   $(printf '%s\n' "ac_cv_func_$ac_func" | sed 's/[^a-zA-Z0-9_]/_/g')
# Uses sprintf with chr() to build the replacement text WITHOUT
# triggering perl's $VAR interpolation or \n newline interpretation.
# The only $ in sprintf's format string is via chr(36) = '$', which
# perl doesn't interpret.
my $DOLLAR = chr(36);
my $BSLASH_N = '\\n';  # 2 chars: backslash + n; perl sees this literally
# Use single-dollar $ac_func (not ${ac_func}) so the value is fully
# resolved at assignment time. Otherwise the downstream autoconf
# pattern `${$as_ac_var+y}` becomes `${ac_cv_func_${ac_func}+y}`
# which bash cannot parse (nested ${} inside ${}).
my $p23_repl = sprintf(
  'ac_cv_func_%s%s',
  $DOLLAR, 'ac_func'
);
$s =~ s{\$\(printf "%s\\n" "ac_cv_func_\$ac_func"[^)]*\)}{$p23_repl}g;
$s =~ s{\$\(printf '%s\\n' "ac_cv_func_\$ac_func"[^)]*\)}{$p23_repl}g;

# Patch 4: replace literal ${ac_func} (curly-brace form) in the
# AC_CHECK_FUNCS cache check with single-dollar $ac_func. MSYS2's
# autoconf 2.71 generates code like
#     if eval test \${ac_cv_func_${ac_func}+y}
# which bash can't parse (nested ${} inside ${+y}), emitting
#     ${ac_cv_func_ RtlSecureZeroMemory+y}: bad substitution
# (bash expands the inner ${ac_func} before displaying the error,
# hence the space). Switching to single-dollar form fixes this.
my $p4_repl = sprintf('ac_cv_func_%s%s', $DOLLAR, 'ac_func');
$s =~ s/ac_cv_func_\$\{ac_func\}/$p4_repl/g;

# Patch 5: rewrite the bash-incompatible cache-check pattern
#     if eval test x${ac_cv_func_${ac_func}+y} = xyes
# to the bash-compatible form using indirect expansion:
#     if eval "[ -n \"\${$as_ac_var+x}\" ]"
# bash 4.4 on MSYS2 cannot parse ${VAR1${VAR2}+y} OR ${VAR1$VAR2+y}
# at script-load time, regardless of eval. The replacement uses
# ${$as_ac_var+x} where bash's `!` indirect prefix looks up the
# variable whose name is the VALUE of $as_ac_var. With eval, the
# inner $as_ac_var is expanded to e.g. ac_cv_func_vsnprintf, then
# ${ac_cv_func_vsnprintf+x} is the standard parameter-expansion
# test (returns 'x' if set, empty otherwise).
my $p5_repl = q{if eval "[ -n \"\${$as_ac_var+x}\" ]"};
# The \{ in the pattern is correct (perl still treats it as literal {) but
# Perl 5.36+ emits an "Unescaped left brace" warning. Disable warnings
# locally around just this s/// to keep CI logs clean.
{
  local $SIG{__WARN__} = sub { warn @_ unless $_[0] =~ /Unescaped left brace/ };
  $s =~ s/if eval test x\${ac_cv_func_(.+?)\+y\} = xyes/$p5_repl/g;
}

if ($s ne $before) {
  open(my $out, ">", "configure") or die "write: $!";
  print $out $s;
  close($out);
}
PERL_EOF
  perl /tmp/patch-tor-configure.pl \
    && echo "Patched configure (backtick + printf format + as_tr_sh)" \
    || echo "perl patch failed (continuing)"
fi

echo "Configuring Tor static library build from: $TOR_SRC_DIR"
# Even after the patches above, the configure script's shebang is
# `#!/bin/sh` and MSYS2's /bin/sh is dash. Force bash so any
# remaining edge cases parse the same way on every platform.
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