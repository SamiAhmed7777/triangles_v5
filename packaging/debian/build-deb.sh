#!/bin/bash
# Build .deb package for Cryptographic Triangles
# Run from the packaging/debian directory
set -e

VERSION="5.7.6"
PKGDIR="triangles_${VERSION}-1_amd64"
RELEASE_URL="https://github.com/SamiAhmed7777/triangles_v5/releases/download/v${VERSION}"

echo "Building .deb package for Triangles v${VERSION}..."

# Create directory structure
rm -rf "$PKGDIR"
mkdir -p "$PKGDIR/DEBIAN"
mkdir -p "$PKGDIR/usr/bin"
mkdir -p "$PKGDIR/usr/local/bin"
mkdir -p "$PKGDIR/usr/share/applications"

# Copy control and postinst
cp DEBIAN/control "$PKGDIR/DEBIAN/"
cp DEBIAN/postinst "$PKGDIR/DEBIAN/"
chmod 755 "$PKGDIR/DEBIAN/postinst"

# Download binaries
echo "Downloading binaries..."
curl -L -o "$PKGDIR/usr/bin/triangles-qt" "${RELEASE_URL}/Cryptographic-Triangles-v${VERSION}-linux-x64-qt"
curl -L -o "$PKGDIR/usr/bin/trianglesd" "${RELEASE_URL}/Cryptographic-Triangles-v${VERSION}-linux-x64-daemon"
chmod 755 "$PKGDIR/usr/bin/triangles-qt" "$PKGDIR/usr/bin/trianglesd"

# Copy bootstrap installer
cp usr/local/bin/triangles-bootstrap-install "$PKGDIR/usr/local/bin/"
chmod 755 "$PKGDIR/usr/local/bin/triangles-bootstrap-install"

# Create desktop entry
cat > "$PKGDIR/usr/share/applications/triangles-qt.desktop" << 'DESKTOP'
[Desktop Entry]
Name=Cryptographic Triangles
GenericName=TRI Wallet
Comment=Cryptographic Triangles cryptocurrency wallet
Exec=triangles-qt %u
Terminal=false
Type=Application
Icon=triangles
MimeType=x-scheme-handler/triangles;
Categories=Finance;Network;P2P;Qt;
StartupNotify=true
DESKTOP

# Build the .deb
dpkg-deb --build "$PKGDIR"
echo "Package built: ${PKGDIR}.deb"
echo "Install with: sudo dpkg -i ${PKGDIR}.deb && sudo apt-get install -f"
