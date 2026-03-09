# Triangles (TRI) Chain Restart - Briefing for Krystie

**Project**: Reviving the Triangles cryptocurrency blockchain
**For**: Krystie (@Krystie7777bot) via Telegram / Pegasus
**From**: Sami
**Date**: March 9, 2026
**Source Code**: `e:\repos\triangles` on Sami's Windows PC

---

## What Is Triangles?

Triangles (TRI) is a Bitcoin-derived Proof-of-Stake cryptocurrency. It uses the Hash9 algorithm (13-step hash cascade) and has a 2-minute block target time. Max supply is 222,222 TRI.

The chain **froze on December 8, 2022** at block **2,186,951** when there were no longer enough active nodes staking. All wallets, balances, and transaction history are preserved in the blockchain data. The goal is to restart staking from that exact point so the chain continues where it left off.

---

## Current Status (What's Done)

### Code Changes (all completed)
- **DNS seeds** updated to `cryptographic-triangles.org` (seed1, seed2, seed3, backup-seed)
- **Hardcoded seed IP**: 194.233.88.206 (DNS2-OpenClaw) baked into the binary
- **V5 hard fork** at block 17,651: disabled checkpoint master key, removed old Tor v2
- **Checkpoint added** at block **2,186,940** with hash `0xbd952e8d4a612e336d840ad924a7e09395e36bcd9d929b302e47e60b5c3098c0`
  - This checkpoint makes syncing dramatically faster (skips signature verification for all 2.18M historical blocks)
- **Linux build system** (`src/makefile.unix`) fixed for v5
- **OpenSSL 3.x** and **modern Boost** compatibility fixed
- **Protocol version**: 70205 (old nodes on 70002-70203 will be disconnected)

### Windows Binary (completed)
- `triangles-qt.exe` built on Mar 9, 2026 with all updates
- Located at `e:\repos\triangles\release\triangles-qt.exe`
- All required DLLs are in the release directory

### Blockchain Data
- `blk0001.dat` (~984 MB) contains the full chain: 2,186,951 blocks
- Available at `E:\Coins\TRIagain\blk0001.dat` on Sami's PC
- A LevelDB index rebuild is currently in progress (importing blocks into `E:\Coins\TRI\txleveldb\`)

---

## What Needs to Happen Next

### Phase 1: Build Linux Binary (trianglesd)

The headless daemon needs to be compiled on a Linux VPS. The source code must be transferred from Sami's PC.

**Build dependencies** (Ubuntu/Debian):
```bash
sudo apt update
sudo apt install -y build-essential libssl-dev libboost-all-dev libdb++-dev libleveldb-dev libevent-dev git
```

**Build steps**:
```bash
# Get source code from Sami (via git push, scp, or zip upload)
cd triangles/src
mkdir -p obj
make -f makefile.unix -j$(nproc)
# Output: ./trianglesd
```

If the build fails on `anonymize.o` or `onion_v3.o`, make sure the `tor/` subdirectory and its files exist:
- `tor/anonymize.cpp`
- `tor/onion_v3.cpp`
- `tor/onion_v3.h`
- `tor/tor_crypto_compat.h`

---

### Phase 2: DNS Records

Set up A records at the `cryptographic-triangles.org` registrar:

| Record | Type | Value |
|---|---|---|
| `seed1.cryptographic-triangles.org` | A | *Primary staking VPS IP* |
| `seed2.cryptographic-triangles.org` | A | *DNS2 VPS IP (194.233.88.206?)* |
| `seed3.cryptographic-triangles.org` | A | *DNS3 VPS IP* |
| `backup-seed.cryptographic-triangles.org` | A | *Same as seed1* |
| `blocks.cryptographic-triangles.org` | A | *Explorer VPS IP (later)* |

---

### Phase 3: Deploy Seed Nodes (DNS2 + DNS3)

Each seed node needs:

#### 3a. Blockchain Data
Copy from Sami's PC to each VPS at `~/.triangles/`:
- `blk0001.dat` (the full ~984 MB blockchain)
- **DO NOT** copy `peers.dat` (stale peer data)
- **DO NOT** copy `wallet.dat` to seed nodes (only to the staking node)

The binary will rebuild the LevelDB index (`txleveldb/`) automatically on first run when it finds a `blk0001.dat` but no index. Alternatively, use `-loadblock=<path>` to import.

#### 3b. Firewall
```bash
sudo ufw allow 24112/tcp    # P2P port
sudo ufw allow from 127.0.0.1 to any port 19112 proto tcp  # RPC localhost only
sudo ufw enable
```

#### 3c. Configuration File
Create `~/.triangles/triangles.conf`:

**For DNS2 (seed node, no staking)**:
```ini
listen=1
server=1
daemon=1
rpcuser=trianglesrpc
rpcpassword=CHANGE_THIS_TO_SOMETHING_STRONG
rpcport=19112
rpcallowip=127.0.0.1
staking=0
addnode=seed1.cryptographic-triangles.org
addnode=seed3.cryptographic-triangles.org
logtimestamps=1
maxconnections=50
```

**For DNS3 (seed node, no staking)**:
```ini
listen=1
server=1
daemon=1
rpcuser=trianglesrpc
rpcpassword=CHANGE_THIS_TO_SOMETHING_DIFFERENT
rpcport=19112
rpcallowip=127.0.0.1
staking=0
addnode=seed1.cryptographic-triangles.org
addnode=seed2.cryptographic-triangles.org
logtimestamps=1
maxconnections=50
```

#### 3d. Start Seed Nodes
```bash
# Start the daemon
./trianglesd -daemon

# Verify it's running
./trianglesd getinfo

# Check block count (should reach 2,186,951 after sync)
./trianglesd getblockcount

# Check peer connections
./trianglesd getconnectioncount
./trianglesd getpeerinfo
```

---

### Phase 4: Start the Primary Staking Node

This is the node that will produce the first new block and restart the chain. It needs `wallet.dat` with coins.

**Which node**: Can be DNS2, DNS3, or a separate VPS. Whichever has `wallet.dat`.

**Configuration** (same as above but with `staking=1`):
```ini
listen=1
server=1
daemon=1
rpcuser=trianglesrpc
rpcpassword=STRONG_PASSWORD_HERE
rpcport=19112
rpcallowip=127.0.0.1
staking=1
addnode=seed2.cryptographic-triangles.org
addnode=seed3.cryptographic-triangles.org
logtimestamps=1
```

**Start order**:
1. Start DNS2 and DNS3 seed nodes **first** (let them sync)
2. Start the staking node **after** seeds are running
3. Unlock the wallet for staking:
   ```bash
   ./trianglesd walletpassphrase "YOUR_PASSPHRASE" 999999999 true
   # The 'true' = staking only (wallet can stake but not spend)
   ```
4. Verify staking is active:
   ```bash
   ./trianglesd getstakinginfo
   # Look for: "enabled": true, "staking": true, "weight": > 0
   ```
5. Monitor for the first new block:
   ```bash
   watch -n 10 './trianglesd getblockcount'
   # Or: tail -f ~/.triangles/debug.log | grep "proof-of-stake"
   ```

**Important notes about staking**:
- Staking needs at least **1 connected peer** to start attempting
- Coins from the old chain already exceed the 1-hour minimum stake age
- The V5 fork reset PoS difficulty to minimum, so the first block should come within **minutes**
- `IsInitialBlockDownload()` must return false (chain must be fully loaded, no new blocks for >10 seconds)

---

## Chain Technical Specs Quick Reference

| Parameter | Value |
|---|---|
| Algorithm | Hash9 (13-step cascade) |
| Consensus | PoW blocks 0-9000, **PoS only from 9001+** |
| Block Time | 2 minutes (120 sec target) |
| PoS Reward | 33% annual (coin-age based) |
| Max Supply | 222,222 TRI |
| Min Stake Age | 1 hour |
| P2P Port | **24112** |
| RPC Port | **19112** |
| Protocol Version | 70205 |
| Data Dir (Linux) | `~/.triangles/` |
| Chain Tip | Block **2,186,951** (Dec 8, 2022) |
| Checkpoint | Block **2,186,940** |
| Genesis Hash | `0x7e7a6e4dd5fe895106fca912dfbacaeaf2a89e76c6a588df8ff96e0e18b96021` |

---

## RPC Commands Cheat Sheet

| Command | What It Does |
|---|---|
| `getinfo` | Node status overview |
| `getblockcount` | Current block height |
| `getblockhash <height>` | Get hash of block at height |
| `getstakinginfo` | Staking status and weight |
| `getconnectioncount` | Number of connected peers |
| `getpeerinfo` | Detailed peer info |
| `getbalance` | Wallet balance |
| `walletpassphrase "<pass>" <timeout> true` | Unlock for staking |
| `walletlock` | Lock wallet |
| `stop` | Gracefully shut down the node |

---

## Troubleshooting

**No peers connecting:**
- Check firewall: `sudo ufw status` -- port 24112 must be open
- Check DNS: `dig seed1.cryptographic-triangles.org` -- must resolve
- Fallback: use `addnode=<IP>` in conf with direct VPS IPs
- Check: `./trianglesd getpeerinfo`

**Staking not starting:**
- `getstakinginfo` -- check `enabled`, `staking`, `weight` fields
- Weight = 0 means no mature coins (wait 1 hour after wallet loads)
- Need at least 1 peer connected
- Chain must be fully synced (not in initial block download)

**Node won't start:**
- Check `~/.triangles/debug.log` for errors
- Make sure port 24112 isn't already in use: `netstat -tlnp | grep 24112`
- Delete `peers.dat` if it contains stale data
- Try starting with `-debug` for verbose logging

**Node stuck at genesis (height=0):**
- The `blk0001.dat` needs to be imported. Either:
  - Place it in `~/.triangles/` and restart (the node reads it during `LoadBlockIndex`)
  - Use `./trianglesd -loadblock=/path/to/blk0001.dat` to import explicitly
- Note: `-loadblock` exits after import. Restart normally after.

---

## Wallet Preservation (for end users later)

Users don't need to do anything special. Their wallets work automatically:
1. `wallet.dat` contains keys -- completely independent of blockchain
2. Users download the new binary with updated seed addresses
3. Binary connects to seed nodes, syncs the blockchain
4. `ScanForWalletTransactions()` automatically finds all their transactions
5. Balances appear correctly

**Tips for users:**
- Delete old `peers.dat` for faster connection
- Add `addnode=seed1.cryptographic-triangles.org` to `triangles.conf`
- Encrypted wallets stay encrypted -- same passphrase works

---

## File Transfer Checklist

**From Sami's PC to each VPS:**

| File | Size | Destination | Notes |
|---|---|---|---|
| Source code (zip/git) | ~50 MB | Any temp dir | For building `trianglesd` |
| `blk0001.dat` | ~984 MB | `~/.triangles/` | Full blockchain data |
| `wallet.dat` | ~3.2 MB | `~/.triangles/` | **STAKING NODE ONLY** |
| `trianglesd` | N/A | Build on VPS | Compiled from source |

**DO NOT transfer**: `peers.dat`, `txleveldb/` (node rebuilds it), any `.lock` files

---

## Contact & Resources

- **Domain**: `cryptographic-triangles.org`
- **Source**: `e:\repos\triangles` (Sami's PC)
- **Full ops guide**: `e:\repos\triangles\CHAIN-RESTART-GUIDE.md`
- **Block explorer** (future): `blocks.cryptographic-triangles.org` (Iquidus)
