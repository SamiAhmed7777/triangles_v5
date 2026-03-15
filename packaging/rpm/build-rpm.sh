#!/bin/bash
# Build RPM package for Cryptographic Triangles
# Run on a Fedora/RHEL/openSUSE system with rpmbuild installed
# Install build tools: sudo dnf install rpm-build rpmdevtools
set -e

VERSION="5.1.4"
RELEASE_URL="https://github.com/SamiAhmed7777/triangles_v5/releases/download/v${VERSION}"

echo "Building RPM for Triangles v${VERSION}..."

# Set up rpmbuild directory structure
rpmdev-setuptree

# Download sources into SOURCES
echo "Downloading binaries..."
curl -L -o ~/rpmbuild/SOURCES/triangles-qt-linux "${RELEASE_URL}/triangles-qt-linux"
curl -L -o ~/rpmbuild/SOURCES/trianglesd-linux "${RELEASE_URL}/trianglesd-linux"
cp triangles-qt.desktop ~/rpmbuild/SOURCES/

# Copy spec file
cp triangles.spec ~/rpmbuild/SPECS/

# Build the RPM (binary only, no source compilation needed)
rpmbuild -bb ~/rpmbuild/SPECS/triangles.spec

echo "RPM built in ~/rpmbuild/RPMS/x86_64/"
echo "Install with: sudo dnf install ~/rpmbuild/RPMS/x86_64/triangles-${VERSION}-1.*.x86_64.rpm"
