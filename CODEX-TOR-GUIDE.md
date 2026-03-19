# Embedded Tor Integration Guide for Triangles

This guide explains how to compile Tor as a static library (`libtor.a`) and link
it directly into the Triangles wallet binary so that every node automatically
runs a Tor hidden service without needing an external Tor installation.

## Architecture Overview

```
trianglesd / triangles-qt
  ├── tor_embedded.cpp    ← calls tor_run_main() in a background thread
  ├── tor_process.cpp     ← fallback: launches external tor binary (already works)
  ├── onion_v3.cpp        ← V3 onion address generation / SOCKS5 proxy logic
  └── libtor.a            ← static Tor library (built from official source)
```

When compiled with `ENABLE_TOR_EMBEDDED`, the wallet calls `tor_run_main()` from
`tor_api.h` on a dedicated thread. This gives the wallet a SOCKS5 proxy on
`127.0.0.1:19099` and a V3 hidden service on port 24112 (the P2P port).

When compiled **without** the flag, `tor_embedded.cpp` falls back to the external
`tor_process.cpp` which searches for and launches a system `tor` binary.

## Step 1: Add Tor as a Git Submodule

```bash
cd /path/to/triangles
git submodule add https://gitlab.torproject.org/tpo/core/tor.git src/tor/tor-src
cd src/tor/tor-src
git checkout release-0.4.9   # latest stable branch as of 2026
```

This puts the full Tor source at `src/tor/tor-src/`.
Current imported checkout in this repo: `release-0.4.9` at commit `1442ca4`.
There is also a helper build script at `src/tor/build-libtor.sh`.

## Step 2: Build libtor.a (Linux)

Tor uses autotools. Build it as a static library:

```bash
cd src/tor/tor-src

# Install Tor build dependencies
sudo apt install autoconf automake libtool pkg-config \
    libssl-dev libevent-dev zlib1g-dev

# Generate configure script
./autogen.sh

# Configure for static library build (disable unneeded modules)
./configure \
    --enable-static-tor \
    --disable-module-relay \
    --disable-module-dirauth \
    --disable-asciidoc \
    --disable-manpage \
    --disable-html-manual \
    --disable-unittests \
    --disable-tool-name-check \
    --with-openssl-dir=/usr \
    --with-libevent-dir=/usr \
    --with-zlib-dir=/usr \
    --prefix=/usr/local

make -j$(nproc)
```

Or from the repo root:
```bash
./src/tor/build-libtor.sh
```

After building, the static libraries are in `src/tor/tor-src/src/`:
- `src/core/libtor-app.a`
- `src/lib/libtor-*.a` (multiple component libs)
- `src/trunnel/libor-trunnel.a`

The header `src/feature/api/tor_api.h` provides the public C API:
```c
tor_main_configuration_t *tor_main_configuration_new(void);
int tor_main_configuration_set_command_line(tor_main_configuration_t *cfg,
                                             int argc, char *argv[]);
int tor_run_main(const tor_main_configuration_t *);
void tor_main_configuration_free(tor_main_configuration_t *);
```

## Step 3: Build Triangles with Embedded Tor

### Linux (makefile.unix)

```bash
cd src

# Point to Tor's built libraries and headers
make -f makefile.unix \
    USE_TOR_EMBEDDED=1
```

You may need to adjust the `-l` flags in the makefile depending on the exact
library names Tor produces. Check `src/tor/tor-src/src/` after building:

```bash
find tor/tor-src/src -name '*.a' | sort
```

Common libraries to link (order matters):
```
-ltor-app -lor -lor-ctime -lor-evloop -lor-event -lor-compress
-lor-container -lor-crypt-ops -lor-encoding -lor-err -lor-fs
-lor-intmath -lor-lock -lor-log -lor-malloc -lor-math -lor-memarea
-lor-meminfo -lor-net -lor-osinfo -lor-process -lor-sandbox
-lor-smartlist-core -lor-string -lor-term -lor-thread -lor-time
-lor-tls -lor-trace -lor-version -lor-wallclock
-lor-trunnel
```

### Windows (triangles-qt.pro)

Add to `triangles-qt.pro`:
```qmake
qmake "USE_TOR_EMBEDDED=1" \
      "TOR_SOURCE_ROOT=src/tor/tor-src"
```

Both build systems now default to:
- source root: `src/tor/tor-src`
- include path: `src/tor/tor-src/src/feature/api`
- library paths: `src/tor/tor-src/src/core`, `src/tor/tor-src/src/lib`, `src/tor/tor-src/src/trunnel`

Override `TOR_EMBEDDED_LIBS` if the actual Tor static library names differ on your platform/build.

## Step 4: Wire into init.cpp

The global hooks `StartEmbeddedTor()` and `StopEmbeddedTor()` need to be called
from `init.cpp`. Add these calls:

### In AppInit2() (after network init, before starting node):
```cpp
#include "tor/tor_embedded.h"

// Near the end of AppInit2, after network initialization:
if (!StartEmbeddedTor()) {
    printf("WARNING: Embedded Tor failed to start. .onion connectivity unavailable.\n");
    // Non-fatal: wallet works without Tor, just no .onion
}
```

### In Shutdown():
```cpp
StopEmbeddedTor();
```

## Step 5: Configure SOCKS Proxy for Outbound Connections

After Tor starts, the wallet needs to route `.onion` connections through the
SOCKS5 proxy. In `net.cpp`, after Tor is initialized:

```cpp
// If embedded Tor is running, use its SOCKS proxy for .onion addresses
CTorEmbedded* tor = CTorEmbedded::GetInstance();
if (tor->IsRunning()) {
    // Set proxy for .onion connections
    proxyType addrProxy(CService("127.0.0.1", tor->GetSocksPort()), 5);
    SetNameProxy(addrProxy);
}
```

## Runtime Flags

The embedded Tor respects these command-line flags:

| Flag | Default | Description |
|------|---------|-------------|
| `-notor` | false | Disable Tor entirely |
| `-torsocks=PORT` | 19099 | SOCKS5 proxy port |
| `-torhsport=PORT` | 24112 | Hidden service virtual port |

## File Layout After Integration

```
src/tor/
├── tor-src/             ← git submodule (official Tor repo)
│   └── src/
│       ├── core/libtor-app.a
│       ├── lib/libor-*.a
│       ├── trunnel/libor-trunnel.a
│       └── feature/api/tor_api.h
├── tor_embedded.h       ← CTorEmbedded class header
├── tor_embedded.cpp     ← implementation (calls tor_run_main)
├── tor_process.h        ← external Tor process manager (fallback)
├── tor_process.cpp
├── onion_v3.h           ← V3 onion address utilities
├── onion_v3.cpp
├── anonymize.h          ← data dir helpers
├── anonymize.cpp
└── LICENSE
```

## Reference: How VERGE (XVG) Does It

VERGE uses the same pattern. Their implementation is at:
- `src/torcontroller.cpp` (~100 lines)
- They use `tor_main()` (older API, pre-0.4.5)
- Git submodule at `src/tor/` pointing to `release-0.4.8` branch
- Build Tor as part of their `depends/` system

Key difference: modern Tor (0.4.5+) uses `tor_run_main()` with a configuration
object instead of raw `tor_main(int argc, char** argv)`.

## Troubleshooting

**Tor fails to bootstrap**: Check firewall rules. Tor needs outbound TCP to the
Tor network (ports 80, 443, 9001, 9030).

**Link errors with libtor**: The Tor static libraries must be linked in
dependency order. If you get undefined symbols, reorder the `-l` flags or use
`-Wl,--start-group ... -Wl,--end-group` to resolve circular deps:
```
LIBS += -Wl,--start-group -ltor-app -lor -lor-ctime ... -Wl,--end-group
```

**OpenSSL version mismatch**: Both Tor and Triangles must link against the same
OpenSSL version (3.x). If Tor was built against a different OpenSSL, rebuild it
with the same `--with-openssl-dir`.
