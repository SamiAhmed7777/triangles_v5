# Cryptographic Triangles (TRI) - v5.1.3

Triangles is a privacy-focused cryptocurrency featuring Proof-of-Stake consensus, Tor v3 onion routing, and built-in encrypted messaging. Originally launched in July 2014, the chain was revived in March 2026 after being frozen since December 2022.

## Key Features

- **Proof-of-Stake** - Energy-efficient block production with 33% annual staking rewards (coin-age based)
- **Hash9 Algorithm** - Unique 13-step hash cascade (Fugue, Hamsi, Groestl, Blake, BMW, Skein, Keccak, Shavite, JH, Luffa, Cubehash, Echo, SIMD)
- **Encrypted Messaging** - Send and receive encrypted messages directly through the wallet
- **Tor v3 Integration** - Connect and transact over the Tor network with v3 onion hidden services
- **120-second Block Time** - Fast confirmations with 2-minute target spacing

## Specifications

| Property | Value |
|----------|-------|
| Algorithm | Hash9 (PoW blocks 0-9000), PoS from block 9001 |
| Block Time | ~120 seconds |
| Max Supply | 222,222 TRI |
| PoS Reward | 33% annual, coin-age based |
| P2P Port | 24112 |
| RPC Port | 19112 |
| Protocol | 70205 |

## Network Status

The Triangles network is live with seed nodes operating on both clearnet and Tor:

**Clearnet Seeds:**
- `194.233.88.206:24112`
- `74.208.167.19:24112`

**Tor v3 Seeds:**
- `gxvrhv3qitnc6kobrhsrse46bmcfitnybapor3or3oczzuxn6hfzxyid.onion:24112`
- `futmtrvh6j34t7s6yjdxfia6iwuyfzwh4k5eqfof5kfhoqk3xmi3qoqd.onion:24112`

**DNS Seeds:**
- `seed1.cryptographic-triangles.org`
- `seed2.cryptographic-triangles.org`

## Building from Source

### Linux (Ubuntu 24.04 / Debian 12+)

Install dependencies:
```bash
sudo apt-get install -y build-essential libboost-all-dev libssl-dev \
    libdb5.3++-dev libevent-dev zlib1g-dev libminiupnpc-dev
```

Build the daemon:
```bash
cd src/leveldb && make libleveldb.a libmemenv.a && cd ..
make -j$(nproc) -f makefile.unix USE_UPNP=0
strip trianglesd
```

### Linux (AlmaLinux 9 / RHEL 9)

Install dependencies:
```bash
sudo dnf install -y gcc-c++ make boost-devel openssl-devel libevent-devel \
    zlib-devel miniupnpc-devel
```

BDB 5.3 C++ bindings must be built from source on RHEL-based systems (the `libdb-devel` package does not include C++ headers). Download BDB 5.3.28 from Oracle and build with `--enable-cxx`.

Then build as above.

### Windows (MSYS2 MinGW64)

Open an MSYS2 MinGW64 shell and install:
```bash
pacman -S mingw-w64-x86_64-boost mingw-w64-x86_64-openssl \
    mingw-w64-x86_64-db mingw-w64-x86_64-miniupnpc \
    mingw-w64-x86_64-qt5-base mingw-w64-x86_64-qrencode
```

Build the Qt wallet:
```bash
qmake triangles-qt.pro
make -j$(nproc)
```

## Running

### First Run
```bash
mkdir -p ~/.triangles
cat > ~/.triangles/triangles.conf << 'EOF'
port=24112
rpcport=19112
rpcuser=trianglesrpc
rpcpassword=<generate-a-strong-password>
rpcallowip=127.0.0.1
staking=1
txindex=1
listen=1
server=1
daemon=1
addnode=194.233.88.206
addnode=74.208.167.19
externalip=<your-public-ip>
EOF

trianglesd
```

The node will connect to seed nodes and sync the blockchain automatically.

### Existing Wallet Holders

If you have a `wallet.dat` from the original Triangles network:

1. Place your `wallet.dat` in `~/.triangles/` (Linux) or `%APPDATA%\triangles\` (Windows)
2. Start the wallet - it will sync the blockchain and your balance will appear automatically
3. No migration or special action is needed - all keys and balances are preserved

### Staking

To stake, your wallet must be:
- Running with `staking=1` in the config
- Connected to at least one peer
- Containing coins with sufficient coin-age (mature inputs)

Check staking status:
```bash
trianglesd getstakinginfo
```

### Encrypted Messaging

Send and receive encrypted messages between wallet addresses:

```bash
# Enable messaging
trianglesd smsgenable

# Send a message
trianglesd smsgsend <your-address> <recipient-address> "Hello from Triangles!"

# Check inbox
trianglesd smsginbox all

# Send anonymous message
trianglesd smsgsendanon <recipient-address> "Anonymous message"
```

Messages are encrypted end-to-end using AES and distributed through the peer network in time-bucketed batches.

### Tor Support

To connect through Tor, install the Tor daemon and add to your config:
```
# triangles.conf
proxy=127.0.0.1:9050
```

To run your own hidden service, add to `/etc/tor/torrc`:
```
HiddenServiceDir /var/lib/tor/triangles/
HiddenServiceVersion 3
HiddenServicePort 24112 127.0.0.1:24112
```

Then set `externalip=<your-onion-address>` in `triangles.conf`.

## RPC Commands

### General
- `getinfo` - Node status, balance, block height, connections
- `getpeerinfo` - Connected peer details
- `getstakinginfo` - Staking status and weight

### Wallet
- `getbalance` - Current balance
- `listunspent` - Unspent transaction outputs
- `sendtoaddress <addr> <amount>` - Send TRI
- `getnewaddress` - Generate new receiving address

### Messaging
- `smsgenable` / `smsgdisable` - Toggle secure messaging
- `smsgsend <from> <to> <message>` - Send encrypted message
- `smsgsendanon <to> <message>` - Send anonymous message
- `smsginbox [all|unread|clear]` - View received messages
- `smsgoutbox [all|clear]` - View sent messages
- `smsglocalkeys` - List messaging-enabled addresses
- `smsgscanchain` - Scan blockchain for public keys

## Chain History

- **July 16, 2014** - Genesis block
- **Block 0-9000** - Proof-of-Work mining phase (Hash9)
- **Block 9001+** - Proof-of-Stake only
- **Block 17,651** - V5 hard fork (removed Tor v2, disabled checkpoint master key)
- **December 8, 2022** - Chain frozen (all nodes offline)
- **March 11, 2026** - Chain revived with v5.0.0.0, staking resumed

## Project Structure

```
src/
  main.cpp          - Core blockchain logic, block/tx validation, message routing
  miner.cpp         - Staking miner thread
  net.cpp           - P2P networking
  init.cpp          - Daemon initialization
  wallet.cpp        - Wallet management
  smessage.cpp/h    - Encrypted messaging system
  kernel.cpp        - PoS kernel (stake validation)
  checkpoints.cpp   - Hardcoded checkpoints
  net_bootstrap.h   - DNS/IP seed configuration
  onionseed.h       - Tor v3 onion seed addresses
  tor/
    onion_v3.cpp/h  - Tor v3 hidden service management
    tor_crypto_compat.h - Ed25519/SHA3 crypto compatibility
```

## License

Distributed under the MIT/X11 software license. See `COPYING` for details.

## Links

- Website: [cryptographic-triangles.org](https://cryptographic-triangles.org)
- Explorer: [blocks.cryptographic-triangles.org](https://blocks.cryptographic-triangles.org)
