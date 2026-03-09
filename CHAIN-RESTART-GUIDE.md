# Triangles (TRI) Chain Restart - Complete Operations Guide

## For Krystie: This document contains everything needed to help Sami restart the Triangles cryptocurrency chain from Telegram. Use Pegasus for server operations on the VPSes.

---

## Overview

Triangles is a Bitcoin-derived Proof-of-Stake cryptocurrency. The chain froze at approximately **block 2,000,000**. The goal is to restart the chain from that point, preserving all existing wallets and balances.

**Domain**: `cryptographic-triangles.org`
**Block Explorer**: `blocks.cryptographic-triangles.org`
**Source Code**: `e:\repos\triangles` (on Sami's Windows PC)

---

## Chain Technical Specs

| Parameter | Value |
|---|---|
| Algorithm | Hash9 (13-step cascade: Blake→BMW→Groestl→Skein→JH→Keccak→Luffa→CubeHash→SHAvite→SIMD→Echo→Hamsi→Fugue) |
| Consensus | PoW blocks 0-9000, **PoS only from block 9001+** |
| Block Time | 2 minutes (120 sec target) |
| PoS Reward | 33% annual (coin-age based) |
| Max Supply | 120,000 TRI |
| Min Stake Age | 1 hour |
| Max Stake Age | 12 hours |
| P2P Port | 24112 (mainnet), 24111 (testnet) |
| RPC Port | 19112 (mainnet), 19111 (testnet) |
| Protocol Version | 70205 |
| Data Dir (Windows) | `%APPDATA%\triangles\` |
| Data Dir (Linux) | `~/.triangles/` |
| Last Hardcoded Checkpoint | Block 17650 |
| V5 Hard Fork Height | Block 17651 |
| Chain Froze At | ~Block 2,000,000 |
| Genesis Hash | `0x7e7a6e4dd5fe895106fca912dfbacaeaf2a89e76c6a588df8ff96e0e18b96021` |

---

## What Needs to Happen (5 Phases)

### Phase 1: Code Changes (on Sami's PC in IDE)

These are source code edits in `e:\repos\triangles\src\`:

**1.1 — Update DNS Seeds**
File: `src/net_bootstrap.h` (lines 19-24)
Change from `triangles.network` to `cryptographic-triangles.org`:
```
seed1.cryptographic-triangles.org
seed2.cryptographic-triangles.org
seed3.cryptographic-triangles.org
backup-seed.cryptographic-triangles.org
```

**1.2 — Add Hardcoded Seed Node IPs**
File: `src/net.cpp` (line 1362)
Currently empty: `unsigned int pnSeed[] = {};`
Must add the VPS IP addresses in network byte order (little-endian).
IP conversion formula: for IP `a.b.c.d` → `(d << 24) | (c << 16) | (b << 8) | a`
Example: `192.168.1.100` = `0x6401A8C0`

Also update the duplicate array in `src/net_bootstrap.h` (lines 29-33).

**1.3 — Fix Linux Build System**
File: `src/makefile.unix`
The makefile references old Tor v2 object files that were removed in v5. The OBJS list must be updated to match the current source files (the Qt .pro file has the correct list). Key files to add: `net_bootstrap.o`, `anonymize.o`, `onion_v3.o`. Remove all old embedded Tor v2 references.

**1.4 — Add Checkpoint at Block 2,000,000**
File: `src/checkpoints.cpp` (lines 25-39)
Once we have the block hash at height 2,000,000, add it as a checkpoint:
```cpp
(2000000, uint256("0x<hash_at_block_2000000>"))
```
This helps new nodes sync faster and validates chain integrity.

**1.5 — Build Binaries**
- Linux `trianglesd` (headless daemon for VPS seed nodes)
- Windows `triangles-qt.exe` (Qt GUI wallet for users)

Build deps (Linux): `build-essential libssl-dev libboost-all-dev libdb++-dev libleveldb-dev libevent-dev`
Build system: `qmake` or corrected `makefile.unix`

---

### Phase 2: Infrastructure Setup (VPS + DNS)

**2.1 — DNS Records** (at `cryptographic-triangles.org` registrar/DNS)

| Record | Type | Value |
|---|---|---|
| `seed1.cryptographic-triangles.org` | A | VPS1 IP |
| `seed2.cryptographic-triangles.org` | A | VPS2 IP |
| `seed3.cryptographic-triangles.org` | A | VPS3 IP |
| `backup-seed.cryptographic-triangles.org` | A | VPS1 IP |
| `blocks.cryptographic-triangles.org` | A | Explorer VPS IP |

**2.2 — Deploy Blockchain Data to VPSes**

Copy from Sami's Windows `%APPDATA%\triangles\` to each VPS `~/.triangles/`:
- `blk0001.dat`, `blk0002.dat`, etc. (blockchain data)
- `txleveldb/` directory (transaction index)
- **DO NOT** copy `peers.dat` (stale peer data)
- **ONLY** copy `wallet.dat` to the PRIMARY staking node

**2.3 — Firewall (on each VPS)**
```bash
sudo ufw allow 24112/tcp    # P2P
# RPC - localhost only (or allow explorer IP)
sudo ufw allow from 127.0.0.1 to any port 19112 proto tcp
```

**2.4 — triangles.conf (on each VPS)**

Primary staking node (`~/.triangles/triangles.conf`):
```ini
listen=1
server=1
daemon=1
rpcuser=trianglesrpc
rpcpassword=<STRONG_RANDOM_PASSWORD>
rpcport=19112
rpcallowip=127.0.0.1
staking=1
addnode=seed2.cryptographic-triangles.org
addnode=seed3.cryptographic-triangles.org
logtimestamps=1
```

Secondary seed nodes — same but with `staking=0` and different `addnode` entries pointing to other seeds.

---

### Phase 3: Chain Restart (the critical part)

**Order of operations:**

1. **Start secondary seed nodes first** (they have blockchain data but no wallet, `staking=0`)
   ```bash
   ./trianglesd -daemon
   ./trianglesd getblockcount   # verify blockchain loaded
   ```

2. **Start primary staking node** (has wallet.dat with coins)
   ```bash
   ./trianglesd -daemon
   ./trianglesd getinfo         # verify it's running
   ```

3. **Verify peer connectivity**
   ```bash
   ./trianglesd getconnectioncount   # must be >= 1
   ./trianglesd getpeerinfo          # shows connected peers
   ```

4. **Unlock wallet for staking** (if encrypted)
   ```bash
   ./trianglesd walletpassphrase "passphrase" 999999999 true
   # The 'true' = staking only (can't spend, only stake)
   ```

5. **Verify staking is active**
   ```bash
   ./trianglesd getstakinginfo
   # Look for: "enabled": true, "staking": true, "weight": > 0
   ```

6. **Monitor for first new block**
   ```bash
   watch -n 10 './trianglesd getblockcount'
   # Or: tail -f ~/.triangles/debug.log | grep "proof-of-stake"
   ```

**Important staking notes:**
- Staking thread needs at least 1 peer connected to attempt staking
- With 2 nodes, staking works (the peer count check at `miner.cpp:554` passes when `nBestHeight >= GetNumBlocksOfPeers()`)
- Coins from old chain already exceed min stake age (1 hour) — they'll stake immediately
- V5 fork resets PoS difficulty to minimum — first blocks should come within minutes
- `IsInitialBlockDownload()` returns false once blockchain is fully loaded and no new blocks arrive for >10 seconds

**If staking isn't working, check:**
- Wallet unlocked? (`walletpassphrase`)
- Peers connected? (`getconnectioncount`)
- Chain synced? (`getblockcount` matches expected height)
- Coins mature enough? (1 hour minimum age)

---

### Phase 4: Block Explorer (Iquidus)

**Install on a VPS (can share with a seed node):**

```bash
# Dependencies
sudo apt install -y nodejs npm mongodb

# Clone and install
git clone https://github.com/iquidus/explorer.git iquidus-explorer
cd iquidus-explorer && npm install

# Configure settings.json with:
# - wallet.host: 127.0.0.1, wallet.port: 19112
# - wallet.user/pass matching triangles.conf RPC credentials
# - genesis_block: "7e7a6e4dd5fe895106fca912dfbacaeaf2a89e76c6a588df8ff96e0e18b96021"
# - coin: "Triangles", symbol: "TRI"

# Set up MongoDB
mongo --eval 'db = db.getSiblingDB("explorerdb"); db.createUser({user:"iquidus",pwd:"dbpassword",roles:["readWrite"]})'

# Start explorer
npm start &

# Sync blockchain into explorer (run via cron every minute)
node scripts/sync.js index update
```

**Caddy reverse proxy** for `blocks.cryptographic-triangles.org`:
```
blocks.cryptographic-triangles.org {
    reverse_proxy localhost:3001
}
```

---

### Phase 5: User Wallet Restoration

**Users don't need to do anything special.** Their wallets are preserved automatically:

1. Wallet keys live in `wallet.dat` — completely independent of blockchain
2. Users download the new binary (with updated seed addresses)
3. Binary connects to seed nodes, syncs the full blockchain
4. `ScanForWalletTransactions()` (in `init.cpp:876`) automatically finds all their transactions
5. Balances appear correctly

**Tips for users:**
- Delete old `peers.dat` for faster initial connection
- Or add `addnode=seed1.cryptographic-triangles.org` to their `triangles.conf`
- Existing encrypted wallets stay encrypted — same passphrase works

---

## Key Source Files Reference

| File | Purpose |
|---|---|
| `src/net_bootstrap.h` | DNS seeds, hardcoded IP seeds |
| `src/net.cpp` | Main peer discovery, `pnSeed[]` array (line 1362) |
| `src/onionseed.h` | Tor v3 onion seeds (currently empty) |
| `src/main.cpp` | Consensus rules, block rewards, `IsInitialBlockDownload()` |
| `src/main.h` | Constants: fork heights, max money, maturity |
| `src/miner.cpp` | Staking thread logic, peer count gates |
| `src/checkpoints.cpp` | Hardcoded checkpoints, sync checkpoint |
| `src/kernel.cpp` | PoS kernel hash computation |
| `src/wallet.h/cpp` | Wallet operations, staking interface |
| `src/init.cpp` | Node startup, wallet loading, blockchain scan |
| `src/version.h` | Protocol versions |
| `src/makefile.unix` | Linux headless daemon build (needs fixing) |
| `triangles-qt.pro` | Qt GUI build (Windows/Linux) |

---

## RPC Commands Cheat Sheet

| Command | Purpose |
|---|---|
| `getinfo` | Node status overview |
| `getblockcount` | Current block height |
| `getblockhash <height>` | Get hash of block at height |
| `getblock <hash>` | Full block details |
| `getstakinginfo` | Staking status and weight |
| `getconnectioncount` | Number of connected peers |
| `getpeerinfo` | Detailed peer information |
| `getbalance` | Wallet balance |
| `walletpassphrase "<pass>" <timeout> true` | Unlock for staking only |
| `walletlock` | Lock wallet |
| `sendtoaddress "<addr>" <amount>` | Send coins |
| `getnewaddress` | Generate new receiving address |
| `dumpprivkey "<addr>"` | Export private key |
| `importprivkey "<key>"` | Import private key |
| `backupwallet "<path>"` | Backup wallet.dat |

---

## Troubleshooting

**No peers connecting:**
- Check firewall: `sudo ufw status` — port 24112 must be open
- Check DNS: `dig seed1.cryptographic-triangles.org` — must resolve
- Fallback: use `addnode=<IP>` in triangles.conf with direct VPS IPs

**Staking not starting:**
- `getstakinginfo` → check `enabled`, `staking`, `weight` fields
- Weight = 0 means no mature coins (wait 1 hour after wallet loads)
- Need at least 1 peer connected

**Node stuck in initial block download:**
- Normal if best block is >24 hours old and still loading
- Wait for blockchain to fully load from blk*.dat files
- Check `debug.log` for progress

**Explorer not syncing:**
- Check RPC credentials match between `settings.json` and `triangles.conf`
- Verify `rpcallowip` permits the explorer's connection
- Test: `curl -u user:pass http://127.0.0.1:19112 -d '{"method":"getblockcount"}'`

---

## Forking at Block 2,000,000

The chain froze at approximately block 2,000,000. To fork/restart from there:

1. The existing blockchain data up to block ~2M is preserved as-is
2. All wallet balances and UTXOs at block 2M are the canonical state
3. New PoS blocks (2,000,001+) will be produced by the restarted staking
4. Any blocks that existed after 2M on a dead/stale fork are naturally orphaned — nodes with the new binary will follow the chain with the most cumulative stake weight
5. If corrupt blocks exist near 2M, use `invalidateblock <hash>` RPC to manually reject them and let the chain reorganize

**Adding the block 2M checkpoint (after restart):**
Once we confirm the hash at block 2,000,000, we add it to `src/checkpoints.cpp` so all new nodes validate against it. This prevents any alternative chain from being accepted.
