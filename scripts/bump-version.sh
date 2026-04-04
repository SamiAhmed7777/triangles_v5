#!/bin/bash
# bump-version.sh — Update the version number across the entire repo.
#
# Usage:
#   scripts/bump-version.sh 5.7.0      # Set version to 5.7.0
#   scripts/bump-version.sh             # Read from src/clientversion.h and sync everything else

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$REPO_ROOT"

# ── Parse version ──────────────────────────────────────────────

if [ -n "${1:-}" ]; then
    VERSION="$1"
else
    # Read from clientversion.h (source of truth)
    MAJOR=$(grep '#define CLIENT_VERSION_MAJOR' src/clientversion.h | awk '{print $3}')
    MINOR=$(grep '#define CLIENT_VERSION_MINOR' src/clientversion.h | awk '{print $3}')
    REV=$(grep '#define CLIENT_VERSION_REVISION' src/clientversion.h | awk '{print $3}')
    VERSION="${MAJOR}.${MINOR}.${REV}"
fi

# Split into components
IFS='.' read -r MAJOR MINOR REV <<< "$VERSION"
BUILD=0

if [ -z "$MAJOR" ] || [ -z "$MINOR" ] || [ -z "$REV" ]; then
    echo "Error: Invalid version '$VERSION'. Expected format: X.Y.Z"
    exit 1
fi

echo "Bumping to v${VERSION} (${MAJOR}.${MINOR}.${REV}.${BUILD})"
echo ""

# ── Helper ─────────────────────────────────────────────────────

updated=()

update_file() {
    local file="$1"
    local pattern="$2"
    local replacement="$3"

    if [ ! -f "$file" ]; then
        echo "  SKIP  $file (not found)"
        return
    fi

    if grep -qE "$pattern" "$file"; then
        sed -i -E "s|${pattern}|${replacement}|g" "$file"
        echo "  OK    $file"
        updated+=("$file")
    else
        echo "  SKIP  $file (pattern not found)"
    fi
}

# ── src/clientversion.h (source of truth) ──────────────────────

echo "Core:"
cat > src/clientversion.h << EOF
#ifndef CLIENTVERSION_H
#define CLIENTVERSION_H

//
// client versioning
//

// These need to be macros, as version.cpp's and triangles-qt.rc's voodoo requires it
#define CLIENT_VERSION_MAJOR       ${MAJOR}
#define CLIENT_VERSION_MINOR       ${MINOR}
#define CLIENT_VERSION_REVISION    ${REV}
#define CLIENT_VERSION_BUILD       ${BUILD}

// Converts the parameter X to a string after macro replacement on X has been performed.
// Don't merge these into one macro!
#define STRINGIZE(X) DO_STRINGIZE(X)
#define DO_STRINGIZE(X) #X

#endif // CLIENTVERSION_H
EOF
echo "  OK    src/clientversion.h"
updated+=("src/clientversion.h")

# ── triangles-qt.pro ───────────────────────────────────────────

update_file "triangles-qt.pro" \
    "^VERSION = [0-9]+\.[0-9]+\.[0-9]+\.[0-9]+" \
    "VERSION = ${MAJOR}.${MINOR}.${REV}.${BUILD}"

# ── Root Dockerfile ────────────────────────────────────────────

update_file "Dockerfile" \
    'LABEL version="[0-9]+\.[0-9]+\.[0-9]+"' \
    "LABEL version=\"${VERSION}\""

# ── Packaging manifests ────────────────────────────────────────

echo ""
echo "Packaging:"

# Docker
update_file "packaging/docker/Dockerfile" \
    'LABEL version="[0-9]+\.[0-9]+\.[0-9]+"' \
    "LABEL version=\"${VERSION}\""

update_file "packaging/docker/docker-compose.yml" \
    'image: triangles:[0-9]+\.[0-9]+\.[0-9]+' \
    "image: triangles:${VERSION}"

# AUR
update_file "packaging/aur/PKGBUILD" \
    "^pkgver=[0-9]+\.[0-9]+\.[0-9]+" \
    "pkgver=${VERSION}"

# Chocolatey
update_file "packaging/chocolatey/triangles.nuspec" \
    "<version>[0-9]+\.[0-9]+\.[0-9]+</version>" \
    "<version>${VERSION}</version>"

# Debian
update_file "packaging/debian/DEBIAN/control" \
    "^Version: [0-9]+\.[0-9]+\.[0-9]+-[0-9]+" \
    "Version: ${VERSION}-1"

# RPM
update_file "packaging/rpm/triangles.spec" \
    "^Version:[[:space:]]+[0-9]+\.[0-9]+\.[0-9]+" \
    "Version:        ${VERSION}"

# WinGet
update_file "packaging/winget/CryptographicTriangles.TrianglesQt.yaml" \
    "^PackageVersion: [0-9]+\.[0-9]+\.[0-9]+" \
    "PackageVersion: ${VERSION}"

# Homebrew
update_file "packaging/homebrew/triangles.rb" \
    'version "[0-9]+\.[0-9]+\.[0-9]+"' \
    "version \"${VERSION}\""

# Nix
update_file "packaging/nix/default.nix" \
    'version = "[0-9]+\.[0-9]+\.[0-9]+"' \
    "version = \"${VERSION}\""

# Flatpak — only update the app version tag, not runtime-version
if [ -f "packaging/flatpak/org.cryptographic_triangles.TrianglesQt.yml" ]; then
    # The flatpak manifest doesn't have a simple version field to sed,
    # so we update any tag: line that has our version pattern
    echo "  NOTE  packaging/flatpak/ — check manually for version references"
fi

# AppImage build script
update_file "packaging/appimage/build-appimage.sh" \
    'VERSION="[0-9]+\.[0-9]+\.[0-9]+"' \
    "VERSION=\"${VERSION}\""

update_file "packaging/appimage/build-appimage.sh" \
    "VERSION=[0-9]+\.[0-9]+\.[0-9]+" \
    "VERSION=${VERSION}"

# ── Summary ────────────────────────────────────────────────────

echo ""
echo "Updated ${#updated[@]} files to v${VERSION}"
echo ""
echo "Still needs manual review:"
echo "  - packaging/appstream/...metainfo.xml — add a new <release> entry"
echo "  - README.md — update header version if desired"
echo "  - Any documentation with download URLs"
echo ""
echo "Next steps:"
echo "  git add -A && git commit -m 'Bump version to v${VERSION}'"
echo "  git tag -a v${VERSION} -m 'v${VERSION}'"
echo "  git push origin master && git push origin v${VERSION}"
