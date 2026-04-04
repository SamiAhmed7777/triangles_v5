# Version Bump Script

Updates the version number across all files in the repo from a single command.

## Usage

**Set a specific version:**
```bash
bash scripts/bump-version.sh 5.7.0
```

**Or edit `src/clientversion.h` first, then sync everything else:**
```bash
bash scripts/bump-version.sh
```

## What it updates

- `src/clientversion.h` (source of truth)
- `src/version.h`
- `triangles-qt.pro`
- `Dockerfile`
- All packaging manifests (Docker, Snap, Scoop, WinGet, RPM, Flatpak, Debian, AppImage)

## What still needs manual review after running

- `packaging/appstream/...metainfo.xml` — add a new `<release>` entry
- `README.md` — update header version if desired
- Any documentation with download URLs
