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
TOR_SHA256="${TOR_SHA256:-7ea13e14cddafb36c6347a9c4f4e639f6010364c16acfd519157c29e226277f2}"

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
printf '%s  %s\n' "${TOR_SHA256}" "${TOR_TARBALL}" | sha256sum --check --strict -
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
User=triangles
Group=triangles
UMask=0077
Environment=HOME=/var/lib/triangles
Environment=LD_LIBRARY_PATH=/usr/lib/cryptographic-triangles/lib
StateDirectory=triangles
StateDirectoryMode=0700
WorkingDirectory=/var/lib/triangles
ExecStart=/usr/lib/cryptographic-triangles/trianglesd -datadir=/var/lib/triangles -conf=/etc/triangles/triangles.conf -printtoconsole
Restart=on-failure
RestartSec=10
NoNewPrivileges=true
PrivateDevices=true
PrivateTmp=true
ProtectClock=true
ProtectControlGroups=true
ProtectHome=true
ProtectHostname=true
ProtectKernelModules=true
ProtectKernelTunables=true
ProtectSystem=strict
ReadWritePaths=/var/lib/triangles
CapabilityBoundingSet=
LockPersonality=true
MemoryDenyWriteExecute=true
RestrictAddressFamilies=AF_UNIX AF_INET AF_INET6
RestrictRealtime=true
RestrictSUIDSGID=true
SystemCallArchitectures=native

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
Depends: adduser
CTRL

# DEBIAN/postinst
cat > "${PKG}/DEBIAN/postinst" << 'POST'
#!/bin/bash
set -e

if ! getent group triangles >/dev/null; then
    addgroup --system triangles
fi
if ! id triangles >/dev/null 2>&1; then
    adduser --system --ingroup triangles --home /var/lib/triangles \
        --no-create-home --disabled-login triangles
fi

install -d -m 0700 -o triangles -g triangles /var/lib/triangles
install -d -m 0750 -o root -g triangles /etc/triangles

if [ ! -e /etc/triangles/triangles.conf ]; then
    RPC_PASSWORD="$(dd if=/dev/urandom bs=32 count=1 2>/dev/null | od -An -tx1 | tr -d ' \n')"
    CONFIG_TMP="$(mktemp)"
    trap 'rm -f "${CONFIG_TMP}"' EXIT
    cat > "${CONFIG_TMP}" << CONF
server=1
rpcuser=trianglesrpc
rpcpassword=${RPC_PASSWORD}
rpcbind=127.0.0.1
rpcallowip=127.0.0.1
rest=0
upnp=0
CONF
    install -m 0640 -o root -g triangles "${CONFIG_TMP}" /etc/triangles/triangles.conf
fi

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
