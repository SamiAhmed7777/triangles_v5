#!/bin/bash
# bump-version.sh - Sync all version references from src/clientversion.h
#
# Usage:
#   ./scripts/bump-version.sh              # Read version from clientversion.h, update everything
#   ./scripts/bump-version.sh 5.7.0        # Set version to 5.7.0 in clientversion.h AND everywhere else
#
# The single source of truth is src/clientversion.h

set -e

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CLIENTVERSION="$REPO_ROOT/src/clientversion.h"

if [ ! -f "$CLIENTVERSION" ]; then
    echo "ERROR: Cannot find $CLIENTVERSION"
    exit 1
fi

# If a version argument is provided, update clientversion.h first
if [ -n "$1" ]; then
    IFS='.' read -r MAJOR MINOR REV <<< "$1"
    REV="${REV:-0}"
    BUILD=0
    sed -i "s/#define CLIENT_VERSION_MAJOR.*/#define CLIENT_VERSION_MAJOR       $MAJOR/" "$CLIENTVERSION"
    sed -i "s/#define CLIENT_VERSION_MINOR.*/#define CLIENT_VERSION_MINOR       $MINOR/" "$CLIENTVERSION"
    sed -i "s/#define CLIENT_VERSION_REVISION.*/#define CLIENT_VERSION_REVISION    $REV/" "$CLIENTVERSION"
    sed -i "s/#define CLIENT_VERSION_BUILD.*/#define CLIENT_VERSION_BUILD       $BUILD/" "$CLIENTVERSION"
    echo "Updated clientversion.h to $MAJOR.$MINOR.$REV.$BUILD"
fi

# Read version from clientversion.h (the source of truth)
MAJOR=$(grep '#define CLIENT_VERSION_MAJOR' "$CLIENTVERSION" | awk '{print $3}')
MINOR=$(grep '#define CLIENT_VERSION_MINOR' "$CLIENTVERSION" | awk '{print $3}')
REV=$(grep '#define CLIENT_VERSION_REVISION' "$CLIENTVERSION" | awk '{print $3}')
BUILD=$(grep '#define CLIENT_VERSION_BUILD' "$CLIENTVERSION" | awk '{print $3}')

VERSION="$MAJOR.$MINOR.$REV"
VERSION_FULL="$MAJOR.$MINOR.$REV.$BUILD"

echo "Syncing all files to version $VERSION (full: $VERSION_FULL)"
echo "==========================================================="

update_file() {
    local file="$1"
    local pattern="$2"
    local replacement="$3"
    if [ -f "$file" ]; then
        sed -i "$pattern" "$file"
        echo "  Updated: $file"
    fi
}

# --- Source files ---

# src/version.h - DISPLAY_VERSION macros
update_file "$REPO_ROOT/src/version.h" \
    "s/#define DISPLAY_VERSION_MAJOR.*/#define DISPLAY_VERSION_MAJOR       $MAJOR/" ""
update_file "$REPO_ROOT/src/version.h" \
    "s/#define DISPLAY_VERSION_MINOR.*/#define DISPLAY_VERSION_MINOR       $MINOR/" ""
update_file "$REPO_ROOT/src/version.h" \
    "s/#define DISPLAY_VERSION_REVISION.*/#define DISPLAY_VERSION_REVISION    $REV/" ""
update_file "$REPO_ROOT/src/version.h" \
    "s/#define DISPLAY_VERSION_BUILD.*/#define DISPLAY_VERSION_BUILD       $BUILD/" ""

# triangles-qt.pro
update_file "$REPO_ROOT/triangles-qt.pro" \
    "s/^VERSION = .*/VERSION = $VERSION_FULL/" ""

# --- Docker ---

update_file "$REPO_ROOT/Dockerfile" \
    "s/LABEL version=\"[^\"]*\"/LABEL version=\"$VERSION\"/" ""

update_file "$REPO_ROOT/packaging/docker/Dockerfile" \
    "s/LABEL version=\"[^\"]*\"/LABEL version=\"$VERSION\"/" ""
update_file "$REPO_ROOT/packaging/docker/Dockerfile" \
    "s/ARG VERSION=.*/ARG VERSION=$VERSION/" ""

update_file "$REPO_ROOT/packaging/docker/docker-compose.yml" \
    "s|cryptographic-triangles/trianglesd:[0-9.]*|cryptographic-triangles/trianglesd:$VERSION|" ""

# --- Snap ---

update_file "$REPO_ROOT/snap/snapcraft.yaml" \
    "s/^version: '[^']*'/version: '$VERSION'/" ""
# Update download URLs in snapcraft.yaml
if [ -f "$REPO_ROOT/snap/snapcraft.yaml" ]; then
    sed -i "s|/download/v[0-9.]*\/|/download/v$VERSION/|g" "$REPO_ROOT/snap/snapcraft.yaml"
    sed -i "s/Cryptographic-Triangles-v[0-9.]*-linux/Cryptographic-Triangles-v$VERSION-linux/g" "$REPO_ROOT/snap/snapcraft.yaml"
fi

# --- Scoop ---

if [ -f "$REPO_ROOT/packaging/scoop/triangles.json" ]; then
    sed -i "s/\"version\": \"[^\"]*\"/\"version\": \"$VERSION\"/" "$REPO_ROOT/packaging/scoop/triangles.json"
    sed -i "s|/download/v[0-9.]*/|/download/v$VERSION/|g" "$REPO_ROOT/packaging/scoop/triangles.json"
    sed -i "s/Cryptographic-Triangles-[0-9.]*-win/Cryptographic-Triangles-$VERSION-win/g" "$REPO_ROOT/packaging/scoop/triangles.json"
    echo "  Updated: packaging/scoop/triangles.json"
fi

# --- WinGet ---

if [ -f "$REPO_ROOT/packaging/winget/CryptographicTriangles.TrianglesQt.yaml" ]; then
    sed -i "s/PackageVersion: .*/PackageVersion: $VERSION/" "$REPO_ROOT/packaging/winget/CryptographicTriangles.TrianglesQt.yaml"
    sed -i "s|/download/v[0-9.]*/|/download/v$VERSION/|g" "$REPO_ROOT/packaging/winget/CryptographicTriangles.TrianglesQt.yaml"
    sed -i "s/Cryptographic-Triangles-[0-9.]*-win/Cryptographic-Triangles-$VERSION-win/g" "$REPO_ROOT/packaging/winget/CryptographicTriangles.TrianglesQt.yaml"
    echo "  Updated: packaging/winget/CryptographicTriangles.TrianglesQt.yaml"
fi

# --- RPM ---

update_file "$REPO_ROOT/packaging/rpm/triangles.spec" \
    "s/^Version:        .*/Version:        $VERSION/" ""

if [ -f "$REPO_ROOT/packaging/rpm/build-rpm.sh" ]; then
    sed -i "s/^VERSION=\"[^\"]*\"/VERSION=\"$VERSION\"/" "$REPO_ROOT/packaging/rpm/build-rpm.sh"
    echo "  Updated: packaging/rpm/build-rpm.sh"
fi

# --- Flatpak ---

if [ -f "$REPO_ROOT/packaging/flatpak/org.cryptographic_triangles.TrianglesQt.yml" ]; then
    sed -i "s|/download/v[0-9.]*/|/download/v$VERSION/|g" "$REPO_ROOT/packaging/flatpak/org.cryptographic_triangles.TrianglesQt.yml"
    sed -i "s/Cryptographic-Triangles-v[0-9.]*-linux/Cryptographic-Triangles-v$VERSION-linux/g" "$REPO_ROOT/packaging/flatpak/org.cryptographic_triangles.TrianglesQt.yml"
    echo "  Updated: packaging/flatpak/org.cryptographic_triangles.TrianglesQt.yml"
fi

# --- Debian ---

if [ -f "$REPO_ROOT/packaging/debian/build-deb.sh" ]; then
    sed -i "s/^VERSION=\"[^\"]*\"/VERSION=\"$VERSION\"/" "$REPO_ROOT/packaging/debian/build-deb.sh"
    echo "  Updated: packaging/debian/build-deb.sh"
fi

# --- AppImage ---

if [ -f "$REPO_ROOT/packaging/appimage/build-appimage.sh" ]; then
    sed -i "s/^VERSION=\"[^\"]*\"/VERSION=\"$VERSION\"/" "$REPO_ROOT/packaging/appimage/build-appimage.sh"
    echo "  Updated: packaging/appimage/build-appimage.sh"
fi

echo ""
echo "Done! All files synced to v$VERSION"
echo ""
echo "Files NOT auto-updated (require manual review):"
echo "  - packaging/appstream/...metainfo.xml  (add new <release> entry)"
echo "  - README.md                            (update header version)"
echo "  - Documentation .md files              (update download URLs if needed)"
