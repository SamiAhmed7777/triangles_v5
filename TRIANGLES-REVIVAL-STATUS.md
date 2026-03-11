# Triangles (TRI) Blockchain Revival - Project Status

**Date:** March 11, 2026
**Prepared by:** Sami
**For:** Krystie (@Krystie7777bot)

---

## Executive Summary

The Triangles blockchain has been successfully revived. The chain froze on December 8, 2022 at block 2,186,951 due to all nodes going offline. As of March 11, 2026, two seed nodes are running, connected, and actively staking new blocks. All wallet balances from the original chain have been preserved.

---

## What Was Done

### 1. Code Modernization (v5.0.0.0 "Black Pharao" Release)

The Triangles codebase was updated to compile and run on modern systems:

- **OpenSSL 3.x compatibility** - Updated all deprecated SHA256/RIPEMD160 calls
- **Boost 1.83+ support** - Fixed compatibility with modern Boost libraries
- **Tor v2 removal** - Removed obsolete Tor v2 onion service code (deprecated Oct 2021)
- **Tor v3 integration** - Added new Onion v3 hidden service support with Ed25519 keys
- **V5 hard fork at block 17,651** - Disabled checkpoint master key, removed Tor v2
- **Hardcoded checkpoint at block 2,186,940** - Protects the chain history
- **MAX_MONEY set to 222,222 TRI** - Updated supply cap
- **Bind fix** - Nodes now listen on all interfaces (0.0.0.0) for external connections

### 2. Blockchain Data Recovery

- Extracted all 2,186,967 blocks from the original `blk0001.dat` file
- Built a custom sorting tool to order blocks by height (they were stored out of order)
- Produced `sorted_blk0001.dat` (927.8 MB) containing the complete verified chain
- Validated the full chain: genesis block through block 2,186,940 (checkpoint)

### 3. Seed Node Deployment

Two seed nodes are now running on geographically distributed VPS servers:

| Property | DNS2 (Staking Node) | DNS3 (Relay Node) |
|----------|-------------------|-------------------|
| **Hosting** | DigitalOcean (OpenClaw) | Contabo (IONOS) |
| **Public IP** | 194.233.88.206 | 74.208.167.19 |
| **OS** | Ubuntu 24.04 LTS | AlmaLinux 9.7 |
| **RAM** | 11 GB | 1.6 GB |
| **Onion Address** | gxvrhv3qitnc6kobrhsrse46bmcfitnybapor3or3oczzuxn6hfzxyid.onion | futmtrvh6j34t7s6yjdxfia6iwuyfzwh4k5eqfof5kfhoqk3xmi3qoqd.onion |
| **Role** | Staking + Seed | Seed/Relay |
| **Wallet** | Has wallet.dat with coins | Fresh wallet (no coins) |

### 4. Chain Status (Live as of March 11, 2026)

```
Block Height:    2,186,984 (and counting - 44 new blocks minted!)
Money Supply:    222,072.39 TRI
Wallet Balance:  2,567.72 TRI (on DNS2)
Staking:         ACTIVE on DNS2
Connections:     1 (DNS2 <-> DNS3 peer link)
PoS Difficulty:  0.00001381
```

### 5. Features Verified Working

- **Proof-of-Stake**: Staking is active and producing new blocks (~2 min intervals)
- **Secure Messaging**: Encrypted peer-to-peer messaging tested and confirmed working
  - First messages sent on the revived network:
    - "THE TRIANGLES ARE FREE."
    - "LONG LIVE THE RESISTANCE. WE ARE THE RESISTANCE."
- **Tor v3 Hidden Services**: Both nodes accessible via .onion addresses
- **Wallet Preservation**: Original wallet.dat loaded successfully, all balances intact

---

## Technical Architecture

### Network Ports
- **P2P Port:** 24112 (node-to-node communication)
- **RPC Port:** 19112 (local management commands)
- **Protocol Version:** 70205

### Consensus Rules
- **Blocks 0-9000:** Proof-of-Work (Hash9 algorithm - 13-step hash cascade)
- **Blocks 9001+:** Proof-of-Stake only (33% annual, coin-age based)
- **Block Time:** ~2 minutes
- **Max Supply:** 222,222 TRI

### Key Files
- **Binary:** `trianglesd` (Linux daemon, ~3.5-3.7 MB stripped)
- **Config:** `~/.triangles/triangles.conf`
- **Blockchain:** `~/.triangles/txleveldb/` (LevelDB)
- **Wallet:** `~/.triangles/wallet.dat` (BDB 5.3)

### Build Dependencies (Linux)
- GCC/G++ 11+
- Boost 1.74+ (system, filesystem, program_options, thread, chrono)
- OpenSSL 3.x
- Berkeley DB 5.3 (C++ bindings)
- LevelDB (included in source tree)
- libevent
- zlib
- miniupnpc (optional)

---

## DNS Configuration Needed

For the domain `cryptographic-triangles.org`, the following DNS records should be set up:

```
seed1.cryptographic-triangles.org  A  194.233.88.206   (DNS2)
seed2.cryptographic-triangles.org  A  74.208.167.19    (DNS3)
seed3.cryptographic-triangles.org  A  <future node IP>
```

These DNS seeds are hardcoded in the wallet binary so new nodes can find peers automatically.

---

## How Wallets Are Preserved

**Important:** All existing wallet.dat files from the original Triangles network are fully compatible. Here's how it works:

1. The `wallet.dat` file contains private keys in a BDB database - it is completely independent of the blockchain
2. When a user starts the new v5.0.0.0 binary with their old wallet.dat, it:
   - Loads all private keys from the wallet
   - Syncs the blockchain from seed nodes
   - Scans all blocks for transactions matching those keys
   - Reconstructs the correct balance automatically
3. No migration, export, or special action is needed - just use the old wallet.dat with the new binary

---

## What's Next

### Immediate Priorities
1. **Build Windows Qt wallet** - So users can connect from desktop with the familiar GUI
2. **Set up DNS records** - Point seed1/seed2.cryptographic-triangles.org to the VPS IPs
3. **Add more seed nodes** - For network resilience (at least 3-5 recommended)

### Medium-Term
4. **Block explorer** - Deploy at blocks.cryptographic-triangles.org
5. **Website update** - Publish new wallet downloads and chain status
6. **Community distribution** - Distribute updated wallet binaries to TRI holders

### Already Implemented (in codebase, needs next binary release)
- Onion v3 seed addresses hardcoded
- Seeder network message handlers wired up
- Both DNS2 and DNS3 IPs in hardcoded seed list
- Tor v3 bootstrap seeder list populated

---

## Node Management Quick Reference

### SSH Access (via Tailscale)
```bash
# DNS2
ssh -o ProxyCommand="tailscale nc dns2-openclaw %p" root@dns2-openclaw

# DNS3
ssh -o ProxyCommand="tailscale nc dns3-sami %p" root@dns3-sami
```

### Common RPC Commands
```bash
trianglesd getinfo              # Node status, balance, block height
trianglesd getstakinginfo       # Staking status
trianglesd getpeerinfo          # Connected peers
trianglesd smsginbox all        # Read encrypted messages
trianglesd smsgsend <from> <to> <msg>  # Send encrypted message
trianglesd stop                 # Graceful shutdown
```

### Config Location
- DNS2: `/root/.triangles/triangles.conf`
- DNS3: `/root/.triangles/triangles.conf`

### Binary Location
- Both: `/usr/local/bin/trianglesd`
- Source: `/opt/triangles-build/src/`

---

## Summary

The Triangles blockchain is alive again. Two seed nodes are running and connected, staking is producing new blocks, encrypted messaging works, and Tor v3 hidden services are active. All original wallet balances are preserved and accessible. The chain picked up exactly where it left off in December 2022, as if it had never stopped.
