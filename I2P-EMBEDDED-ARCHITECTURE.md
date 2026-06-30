# I2P Embedded Architecture (Level 3)

**Date:** 2026-06-27  
**Status:** ✅ IMPLEMENTED & WORKING

---

## What This Is

Triangles now runs **two embedded anonymity networks simultaneously**:

1. **Tor** — Every node is a .onion hidden service (existing, unchanged)
2. **I2P** — Every node is a .b32.i2p destination (new)

Both routers run **in-process** as static libraries. No external dependencies, no separate daemons to install.

### What I2P Adds Over Tor-Only

| Property | Tor | I2P |
|----------|-----|-----|
| Routing | Onion (3-hop circuits) | Garlic (variable-hop tunnels) |
| Directory | Centralized authorities | Distributed floodfills |
| Service discovery | Hidden service descriptors | Network database (KadDHT) |
| Designed for | Exit to clearnet | Peer-to-peer services |
| Peer correlation resistance | Moderate | Strong (ephemeral tunnels) |

I2P was designed from the ground up for **peer-to-peer anonymous services** — exactly what a cryptocurrency P2P network needs. Tor's hidden services work, but Tor is optimized for anonymous web browsing (exit traffic). I2P's garlic routing, distributed network database, and short-lived tunnels make it inherently better suited for P2P mesh communication.

---

## Architecture

### Dual-Network Routing

```
                    ┌─────────────────────────────────┐
                    │       trianglesd (process)       │
                    │                                  │
                    │  ┌─────────┐    ┌─────────┐     │
                    │  │ libtor  │    │ libi2pd │     │
                    │  │ (Tor)   │    │ (I2P)   │     │
                    │  └────┬────┘    └────┬────┘     │
                    │       │              │          │
  .onion peers ─────┼───────┘              │          │
                    │     SOCKS 19099      │          │
                    │                      │          │
  .b32.i2p peers ───┼──────────────────────┘          │
                    │     SOCKS 19100                 │
                    └─────────────────────────────────┘
```

### Traffic Flow

| Destination | Route | Proxy |
|-------------|-------|-------|
| `*.onion` | Tor SOCKS5 → Tor circuit → hidden service | 127.0.0.1:19099 |
| `*.b32.i2p` | I2P SOCKS5 → I2P tunnel → destination | 127.0.0.1:19100 |
| Clearnet (IPv4/IPv6) | **BLOCKED** | — |

The routing decision happens in `ConnectSocketByName()` (netbase.cpp):
- `.b32.i2p` suffix → I2P SOCKS proxy (NET_I2P)
- Everything else → Tor name proxy (SetNameProxy)

---

## Implementation

### Files Added

```
src/i2p/
├── i2pd-src/              # PurpleI2P/i2pd git submodule
├── i2p_embedded.h         # CI2PEmbedded class declaration
├── i2p_embedded.cpp       # Embedded router start/stop logic
├── i2pseed.h              # Hardcoded .b32.i2p seed nodes
└── build-libi2pd.sh       # Static library build script
```

### Files Modified

| File | Change |
|------|--------|
| `CMakeLists.txt` | `USE_I2P_EMBEDDED` option + config summary |
| `src/CMakeLists.txt` | I2P source, includes, library linking |
| `src/init.cpp` | I2P startup (after Tor), shutdown, CLI flags |
| `src/net.cpp` | Allow `.b32.i2p` in `ConnectNode()` and seed parser |
| `src/netbase.cpp` | I2P SOCKS routing, fixed `.b32.i2p` address parsing |

### CI2PEmbedded Class

Singleton pattern (mirrors `CTorEmbedded`):

```cpp
class CI2PEmbedded {
    bool Start(int socksPort, int samPort, int serverPort);
    void Stop();
    bool IsRunning() const;
    std::string GetSocksProxy() const;    // "127.0.0.1:19100"
    std::string GetI2PAddress() const;     // .b32.i2p destination
};
```

### Startup Sequence (init.cpp)

```
1. StartEmbeddedTor()          → Tor SOCKS on 19099
2. TOR-NATIVE MODE             → all traffic forced through Tor
3. StartEmbeddedI2P()          → i2pd SOCKS on 19100
4. I2P-NATIVE MODE             → .b32.i2p routed through i2pd
5. Dual-network anonymity      → Tor + I2P co-equal
```

If I2P fails to start, the daemon continues in Tor-only mode (non-fatal).

### How i2pd Integrates

i2pd provides a C++ API (`libi2pd/api.h`) for in-process embedding:

```cpp
i2p::api::InitI2P(argc, argv, "triangles-i2pd");
i2p::api::StartI2P(logStream);
i2p::client::context.Start();  // SAM, SOCKS, tunnels
```

The auto-generated `i2pd.conf` enables:
- SOCKS proxy on 19100 (for outbound .b32.i2p)
- SAM bridge on 7656 (for future SAM v3 protocol)
- Server tunnel in `tunnels.conf` (I2P hidden service)

The `tunnels.conf` is written before `Start()`:
```ini
[triangles-p2p]
type = server
host = 127.0.0.1
port = <P2P_PORT>
keys = triangles-p2p-keys.dat
inbound.length = 3
outbound.length = 3
```

This creates a persistent `.b32.i2p` destination that survives restarts.

---

## Build Instructions

### Prerequisites

Same as existing Tor build + Boost (already required).

### Build with I2P

```bash
# 1. Initialize the i2pd submodule
git submodule update --init --recursive src/i2p/i2pd-src

# 2. Build i2pd static libraries
cd src/i2p && bash build-libi2pd.sh

# 3. Configure and build Triangles
mkdir build && cd build
cmake -G Ninja -DUSE_I2P_EMBEDDED=ON ..
ninja trianglesd
```

### Build without I2P (Tor-only, existing behavior)

```bash
cmake -G Ninja ..          # USE_I2P_EMBEDDED defaults to OFF
ninja trianglesd
```

---

## CLI Flags

| Flag | Default | Description |
|------|---------|-------------|
| `-i2p` | `1` | Enable embedded I2P router |
| `-i2psocks=<port>` | `19100` | I2P SOCKS proxy port |
| `-i2psam=<port>` | `7656` | I2P SAM bridge port |
| `-i2phsport=<port>` | P2P port | I2P server tunnel forward port |

---

## Testing Verification

### Expected Startup Output

```
Embedded I2P: starting i2pd router...
Embedded I2P: server tunnel configured on port 24112
...
Clients: New private keys file .../triangles-p2p-keys.dat for <b32>.b32.i2p created
Clients: 1 I2P server tunnels created
Embedded I2P: SOCKS proxy at 127.0.0.1:19100, SAM at 127.0.0.1:7656
...
I2P-NATIVE MODE: I2P router running
  SOCKS proxy at 127.0.0.1:19100 for .b32.i2p connections
  Dual-network anonymity: Tor (.onion) + I2P (.b32.i2p)
```

---

## Seed Node Deployment

To deploy an I2P seed node:

1. Build with `-DUSE_I2P_EMBEDDED=ON`
2. Start the daemon — it auto-generates a `.b32.i2p` destination
3. Read the address from the log: `grep "b32.i2p" debug.log`
4. Add the address to `src/i2p/i2pseed.h`
5. Add the address to `seeds.cryptographic-triangles.org/i2p-seeds.txt`

The destination keys persist in `<datadir>/i2p_data/triangles-p2p-keys.dat`.

---

## Comparison to Other Projects

| Project | Tor | I2P | Embedded | Dual-Network |
|---------|-----|-----|----------|-------------|
| **Triangles** | ✅ Embedded | ✅ Embedded | Both in-process | ✅ |
| Bitcoin Core | Optional | Optional (SAM) | No | No |
| Monero | Optional | No | No | No |
| Kovri (Monero I2P) | N/A | Planned | Planned | No |

Triangles is the only cryptocurrency with **both** Tor and I2P embedded as in-process routers.

---

## Future Work

- **I2P seed nodes:** Deploy stable .b32.i2p seeds (parallel to onion seeds)
- **SAM v3 direct:** Use SAM bridge for native I2P streaming (bypass SOCKS overhead)
- **I2P address in RPC:** Expose `.b32.i2p` address via `getnetworkinfo`
- **Cross-network bridging:** Allow Tor nodes to discover I2P peers and vice versa
