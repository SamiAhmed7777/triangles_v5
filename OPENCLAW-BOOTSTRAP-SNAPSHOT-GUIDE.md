# OpenClaw Bootstrap Snapshot Guide

## Purpose

This document tells OpenClaw exactly how to update the existing Triangles bootstrap server so new wallets download a ready-to-use snapshot instead of downloading `blk0001.dat` and rebuilding the index locally.

This guide matches the current wallet code in:

- `src/bootstrap.cpp`
- `src/bootstrap.h`
- `src/checkpoints.cpp`
- `src/version.h`

## What The Wallet Actually Does

When a fresh wallet bootstraps, it:

1. Downloads `http://bootstrap.cryptographic-triangles.org/bootstrap.tar.gz`
2. Extracts it into the data directory
3. Requires `blk0001.dat` to exist after extraction
4. Looks for `txleveldb/` and `snapshot.manifest`
5. Keeps `txleveldb/` only if `snapshot.manifest` passes verification
6. Deletes `txleveldb/` if verification fails, then rebuilds from `blk0001.dat`
7. Always deletes `database/` from the extracted snapshot

The verification rules are strict:

- `format` must be `1`
- `network` must be `main` on mainnet
- `dbversion` must be `70509`
- `height` and `hash` must exactly match a hardcoded checkpoint

If any of those checks fail, the wallet throws away the shipped `txleveldb/`.

## Current Hardcoded Mainnet Checkpoint

As of the current codebase, the latest hardcoded mainnet checkpoint is:

- Height: `2186940`
- Hash: `bd952e8d4a612e336d840ad924a7e09395e36bcd9d929b302e47e60b5c3098c0`

OpenClaw must not generate a manifest with an arbitrary tip hash. The manifest only survives if it matches a hardcoded checkpoint from `src/checkpoints.cpp`.

## Important Limitation

If the live chain tip is past the latest hardcoded checkpoint, OpenClaw has two valid options:

1. Publish a snapshot taken exactly at the latest hardcoded checkpoint
2. Publish `blk0001.dat` only, without `txleveldb/`, and let clients rebuild locally

OpenClaw must not publish a `snapshot.manifest` for a height/hash that is not compiled into the wallet.

## Files OpenClaw Should Publish

The preferred `bootstrap.tar.gz` should contain:

- `blk0001.dat`
- `txleveldb/`
- `snapshot.manifest`
- optionally `peers.dat`

It must not contain:

- `wallet.dat`
- `database/`
- `.lock`
- pid files
- logs
- Tor state

Legacy fallback files should still exist on the web root:

- `blk0001.dat`
- `filelist.txt`

## Requirements For The Source Node

Before building a snapshot, the source node should be:

- fully synced
- cleanly shut down before copying files
- built from the same code/version expected by clients
- using the same LevelDB schema as the client (`DATABASE_VERSION=70509`)

Recommended node config for the source snapshot node:

```ini
txindex=1
addressindex=1
daemon=1
server=1
```

`addressindex=1` is recommended so clients that enable address index can benefit from faster indexed wallet rescans and address RPCs immediately.

## OpenClaw Workflow

### Step 1: Decide Whether A Prebuilt Index Is Allowed

OpenClaw must first decide whether it can ship `txleveldb/`.

Rules:

- If the snapshot node is exactly at checkpoint `2186940`, shipping `txleveldb/` is allowed
- If the snapshot node is above `2186940` and the code has not been updated with a newer checkpoint, do not ship `txleveldb/`
- In that case, publish a blocks-only bootstrap instead

### Step 2: Stop The Source Node Cleanly

Never copy a live LevelDB directory.

```bash
trianglesd stop
sleep 10
pgrep -af trianglesd || true
```

OpenClaw should confirm the daemon is fully stopped before copying `txleveldb/`.

### Step 3: Create A Staging Directory

```bash
rm -rf /tmp/triangles-bootstrap-stage
mkdir -p /tmp/triangles-bootstrap-stage
```

### Step 4: Copy Snapshot Files

For a verified snapshot:

```bash
cp ~/.triangles/blk0001.dat /tmp/triangles-bootstrap-stage/
cp -a ~/.triangles/txleveldb /tmp/triangles-bootstrap-stage/
test -f ~/.triangles/peers.dat && cp ~/.triangles/peers.dat /tmp/triangles-bootstrap-stage/
```

Do not copy:

```bash
rm -rf /tmp/triangles-bootstrap-stage/database
rm -f /tmp/triangles-bootstrap-stage/wallet.dat
rm -f /tmp/triangles-bootstrap-stage/.lock
rm -f /tmp/triangles-bootstrap-stage/*.pid
rm -f /tmp/triangles-bootstrap-stage/debug.log
```

### Step 5: Write `snapshot.manifest`

If OpenClaw is publishing a verified prebuilt index, write:

```bash
cat > /tmp/triangles-bootstrap-stage/snapshot.manifest << 'EOF'
format=1
network=main
height=2186940
hash=bd952e8d4a612e336d840ad924a7e09395e36bcd9d929b302e47e60b5c3098c0
dbversion=70509
EOF
```

Rules:

- `hash` must not include `0x`
- `network` must be `main`
- `dbversion` must be `70509`
- If OpenClaw is publishing blocks-only bootstrap, it should omit `snapshot.manifest` entirely

### Step 6: Build The Tarball

```bash
cd /tmp/triangles-bootstrap-stage
tar czf /tmp/bootstrap.tar.gz .
```

### Step 7: Publish To The Existing Bootstrap Server

This guide assumes the existing nginx root is:

- `/var/www/triangles-bootstrap`

Publish the preferred tarball and the legacy fallback files:

```bash
sudo mkdir -p /var/www/triangles-bootstrap
sudo mv /tmp/bootstrap.tar.gz /var/www/triangles-bootstrap/bootstrap.tar.gz
sudo cp ~/.triangles/blk0001.dat /var/www/triangles-bootstrap/blk0001.dat
printf "blk0001.dat\n" | sudo tee /var/www/triangles-bootstrap/filelist.txt > /dev/null
sudo chown -R www-data:www-data /var/www/triangles-bootstrap
```

If OpenClaw is publishing a blocks-only bootstrap, the commands are the same except the tarball should contain only `blk0001.dat` and optional `peers.dat`.

## Validation Checklist

Before marking the update complete, OpenClaw should verify:

### Tarball contents

```bash
tar tzf /var/www/triangles-bootstrap/bootstrap.tar.gz | sort
```

Expected for verified snapshot:

- `./blk0001.dat`
- `./txleveldb/...`
- `./snapshot.manifest`

Expected not to exist:

- `wallet.dat`
- `database/`

### HTTP responses

```bash
curl -I http://localhost/bootstrap.tar.gz
curl -I http://localhost/blk0001.dat
curl http://localhost/filelist.txt
```

Expected:

- HTTP `200`
- `filelist.txt` contains `blk0001.dat`

### Manifest sanity

```bash
tar xOf /var/www/triangles-bootstrap/bootstrap.tar.gz ./snapshot.manifest
```

Expected:

- `format=1`
- `network=main`
- `height=2186940`
- `hash=bd952e8d4a612e336d840ad924a7e09395e36bcd9d929b302e47e60b5c3098c0`
- `dbversion=70509`

## Fresh-Client Test

OpenClaw should test the artifact on a clean machine or clean data directory:

```bash
mv ~/.triangles ~/.triangles.backup.$(date +%s)
mkdir -p ~/.triangles
trianglesd -bootstrap
```

Then inspect startup logs.

Successful verified snapshot behavior should include:

- snapshot downloaded
- `snapshot.manifest found`
- `manifest verified - keeping pre-built index`
- no message about removing extracted `txleveldb/`

Failure behavior will include:

- manifest parse or verification failure
- `removing extracted txleveldb/`
- slow rebuild from `blk0001.dat`

## Safe Publish Procedure

OpenClaw should use this order:

1. Build snapshot in `/tmp`
2. Validate tarball contents
3. Replace `/var/www/triangles-bootstrap/bootstrap.tar.gz`
4. Replace `/var/www/triangles-bootstrap/blk0001.dat`
5. Replace `/var/www/triangles-bootstrap/filelist.txt`
6. Confirm HTTP `200`

This avoids serving a half-written tarball.

## Example Bot Prompt

Use this exact tasking for OpenClaw:

```text
Update the existing Triangles bootstrap server on bootstrap.cryptographic-triangles.org.

Rules:
- Build the snapshot from a cleanly stopped source node
- If the source node is exactly at hardcoded checkpoint 2186940 / bd952e8d4a612e336d840ad924a7e09395e36bcd9d929b302e47e60b5c3098c0, publish a verified snapshot containing blk0001.dat, txleveldb/, and snapshot.manifest
- If the source node is above the latest hardcoded checkpoint, publish a blocks-only bootstrap and do not ship txleveldb/
- Do not ship wallet.dat, database/, .lock, pid files, logs, or Tor state
- Publish bootstrap.tar.gz, blk0001.dat, and filelist.txt to /var/www/triangles-bootstrap
- Verify curl HTTP 200 for bootstrap.tar.gz and blk0001.dat
- Report the tarball contents and whether the snapshot is verified or blocks-only
```

## Recommended Next Improvement

This workflow will stay constrained until the next checkpoint is updated in `src/checkpoints.cpp`.

If you want OpenClaw to keep shipping prebuilt `txleveldb/` snapshots as the chain advances, the software needs periodic checkpoint updates. Without that, the verified snapshot path will stop at the latest compiled checkpoint and clients will fall back to rebuilds.
