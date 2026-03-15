#!/bin/bash
# Build AppImage for Cryptographic Triangles Qt Wallet
# Run on a Linux x64 system with appimagetool installed
set -e

VERSION="5.1.4"
APPDIR="Triangles-x86_64.AppDir"
RELEASE_URL="https://github.com/SamiAhmed7777/triangles_v5/releases/download/v${VERSION}"

echo "Building AppImage for Cryptographic Triangles v${VERSION}..."

# Clean and create AppDir structure
rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/bin"
mkdir -p "$APPDIR/usr/share/applications"
mkdir -p "$APPDIR/usr/share/icons/hicolor/256x256/apps"

# Download binary
echo "Downloading triangles-qt..."
curl -L -o "$APPDIR/usr/bin/triangles-qt" "${RELEASE_URL}/triangles-qt-linux"
chmod +x "$APPDIR/usr/bin/triangles-qt"

# Create desktop entry
cat > "$APPDIR/usr/share/applications/triangles-qt.desktop" << 'DESKTOP'
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

# Copy desktop file and icon to AppDir root (required by AppImage)
cp "$APPDIR/usr/share/applications/triangles-qt.desktop" "$APPDIR/"

# Use project icon if available, otherwise create placeholder
if [ -f "../../src/qt/res/icons/triangles.png" ]; then
    cp "../../src/qt/res/icons/triangles.png" "$APPDIR/usr/share/icons/hicolor/256x256/apps/"
    cp "../../src/qt/res/icons/triangles.png" "$APPDIR/triangles.png"
else
    echo "WARNING: No icon found, AppImage will have no icon"
fi

# Create AppRun
cat > "$APPDIR/AppRun" << 'APPRUN'
#!/bin/bash
HERE="$(dirname "$(readlink -f "$0")")"
exec "$HERE/usr/bin/triangles-qt" "$@"
APPRUN
chmod +x "$APPDIR/AppRun"

# Build the AppImage
if command -v appimagetool &> /dev/null; then
    ARCH=x86_64 appimagetool "$APPDIR" "Cryptographic_Triangles-v${VERSION}-x86_64.AppImage"
    echo "AppImage built: Cryptographic_Triangles-v${VERSION}-x86_64.AppImage"
else
    echo "appimagetool not found. Install it from: https://github.com/AppImage/AppImageKit/releases"
    echo "Then run: ARCH=x86_64 appimagetool $APPDIR Cryptographic_Triangles-v${VERSION}-x86_64.AppImage"
fi
