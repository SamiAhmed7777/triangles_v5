#!/usr/bin/env bash
# scripts/ci/package-linux-daemon.sh
#
# Linux packaging step for the triangles daemon + CLI .deb.
# Called from .github/workflows/build-all.yml build-linux-daemon step.
#
# Builds a self-contained .deb with trianglesd, triangles-cli, bundled libs,
# Tor, systemd service, and CLI launchers. Designed to be reproducible and
# debuggable outside the CI environment.
#
# Usage: bash scripts/ci/package-linux-daemon.sh <version>

set -euo pipefail

VERSION="${1:-0.0.0}"
PKG="cryptographic-triangles-daemon_${VERSION}_amd64"
TOR_VERSION="${TOR_VERSION:-15.0.9}"

echo ">>> Building .deb for triangles ${VERSION}"

# Stage directories
rm -rf "${PKG}"
mkdir -p "${PKG}/DEBIAN"
mkdir -p "${PKG}/usr/lib/cryptographic-triangles/lib"
mkdir -p "${PKG}/usr/lib/cryptographic-triangles/tor"
mkdir -p "${PKG}/usr/bin"
mkdir -p "${PKG}/etc/systemd/system"

# Download + extract Tor
TOR_TARBALL="tor-expert-bundle-linux-x86_64-${TOR_VERSION}.tar.gz"
if [ ! -f "${TOR_TARBALL}" ]; then
    echo ">>> Downloading Tor ${TOR_VERSION}..."
    # Resilient download: archive.torproject.org occasionally times out from
    # CI egress (observed 2026-07-03: macOS job exit code 6 after exactly 30s
    # of curl hang). --retry 3 + --retry-connrefused covers transient network
    # drops; --fail-with-body surfaces HTTP errors loudly.
    curl -fSL --connect-timeout 15 --max-time 120 \
        --retry 3 --retry-delay 5 --retry-connrefused --retry-all-errors \
        "https://archive.torproject.org/tor-package-archive/torbrowser/${TOR_VERSION}/${TOR_TARBALL}" \
        -o "${TOR_TARBALL}"
fi
mkdir -p tor-extract
tar -xzf "${TOR_TARBALL}" -C tor-extract

# Copy binaries
cp "build/bin/trianglesd"    "${PKG}/usr/lib/cryptographic-triangles/"
cp "build/bin/triangles-cli" "${PKG}/usr/lib/cryptographic-triangles/"

# Copy Tor
cp "tor-extract/tor/tor" "${PKG}/usr/lib/cryptographic-triangles/tor/"
chmod +x "${PKG}/usr/lib/cryptographic-triangles/tor/tor"
if [ -d "tor-extract/data" ]; then
    cp -r "tor-extract/data" "${PKG}/usr/lib/cryptographic-triangles/tor/data"
fi

# Bundle shared library dependencies (skip glibc/kernel — always present)
echo ">>> Bundling shared library dependencies..."
ALL_LIBS="$(mktemp)"
trap 'rm -f "${ALL_LIBS}"' EXIT

for bin in trianglesd triangles-cli; do
    ldd "build/bin/${bin}" 2>/dev/null \
        | grep '=> /' \
        | awk '{print $3}' \
        >> "${ALL_LIBS}" || true
done

if [ -s "${ALL_LIBS}" ]; then
    sort -u "${ALL_LIBS}" | while IFS= read -r lib; do
        if [ -z "${lib}" ]; then continue; fi
        case "${lib}" in
            /lib/x86_64-linux-gnu/libc.so*|/lib/x86_64-linux-gnu/libm.so*|/lib/x86_64-linux-gnu/libpthread.so*|/lib/x86_64-linux-gnu/libdl.so*|/lib/x86_64-linux-gnu/librt.so*|/lib/x86_64-linux-gnu/ld-linux*|/lib64/ld-linux*)
                ;;  # Skip glibc core
            *)
                cp -L "${lib}" "${PKG}/usr/lib/cryptographic-triangles/lib/" 2>/dev/null || true
                ;;
        esac
    done
fi

echo ">>> Bundled libs:"
ls -la "${PKG}/usr/lib/cryptographic-triangles/lib/" | tail -n +2 | wc -l

# Launchers (set LD_LIBRARY_PATH for bundled libs)
cat > "${PKG}/usr/bin/trianglesd" << 'LAUNCHER'
#!/bin/bash
INSTALL_DIR=/usr/lib/cryptographic-triangles
export LD_LIBRARY_PATH="${INSTALL_DIR}/lib:${LD_LIBRARY_PATH}"
exec "${INSTALL_DIR}/trianglesd" "$@"
LAUNCHER
chmod +x "${PKG}/usr/bin/trianglesd"

cat > "${PKG}/usr/bin/triangles-cli" << 'LAUNCHER'
#!/bin/bash
INSTALL_DIR=/usr/lib/cryptographic-triangles
export LD_LIBRARY_PATH="${INSTALL_DIR}/lib:${LD_LIBRARY_PATH}"
exec "${INSTALL_DIR}/triangles-cli" "$@"
LAUNCHER
chmod +x "${PKG}/usr/bin/triangles-cli"

# systemd unit
cat > "${PKG}/etc/systemd/system/trianglesd.service" << 'SVC'
[Unit]
Description=Cryptographic Triangles Daemon
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
Environment=LD_LIBRARY_PATH=/usr/lib/cryptographic-triangles/lib
ExecStart=/usr/lib/cryptographic-triangles/trianglesd
Restart=on-failure
RestartSec=10

[Install]
WantedBy=multi-user.target
SVC

# DEBIAN/control
cat > "${PKG}/DEBIAN/control" << CTRL
Package: cryptographic-triangles-daemon
Version: ${VERSION}
Architecture: amd64
Maintainer: Cryptographic Triangles <dev@cryptographic-triangles.org>
Description: Cryptographic Triangles daemon + CLI with integrated Tor
 Fully self-contained headless node + JSON-RPC client with all libraries,
 Tor, and systemd service. No external dependencies required.
Section: finance
Priority: optional
CTRL

# DEBIAN/postinst
cat > "${PKG}/DEBIAN/postinst" << 'POST'
#!/bin/bash
systemctl daemon-reload
echo ""
echo "Cryptographic Triangles daemon + CLI installed."
echo "  Start daemon: sudo systemctl start trianglesd"
echo "  On boot:      sudo systemctl enable trianglesd"
echo "  Use CLI:      triangles-cli getinfo"
echo ""
POST
chmod +x "${PKG}/DEBIAN/postinst"

# Build the .deb
dpkg-deb --build "${PKG}"
echo ">>> Built: ${PKG}.deb"
ls -la "${PKG}.deb"
exit 0
