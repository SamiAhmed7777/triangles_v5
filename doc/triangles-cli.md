# Triangles CLI Operations

> Operator-facing guide for `triangles-cli`, the JSON-RPC client that ships
> with the Triangles daemon. Companion to `contrib/triangles.conf.example`
> (daemon config) and `scripts/tri/README.md` (friendly wrapper).

## What `triangles-cli` is

`triangles-cli` is a small standalone binary that talks JSON-RPC over TCP
to a running `trianglesd` daemon. It is the canonical way to read chain
state, manage the wallet, and trigger node actions from the shell.

It does **not** start, stop, or manage the daemon. It just talks to one
that is already running.

The binary lives in the same directory as `trianglesd` after build:

| Platform | Default install path |
|---|---|
| Linux (Debian package) | `/usr/lib/cryptographic-triangles/triangles-cli` |
| Linux (manual)         | wherever you put it; this doc assumes `/usr/local/bin` |
| macOS (Homebrew)       | `/usr/local/bin/triangles-cli` |
| Windows                | `<install-dir>\triangles-cli.exe` |

## Connection parameters

`triangles-cli` needs four pieces of information to reach the daemon:

| Param | Default | Override flag |
|---|---|---|
| RPC host   | `127.0.0.1` | `-rpcconnect=<ip>` |
| RPC port   | `19111` (mainnet) / `19112` (testnet) | `-rpcport=<port>` |
| RPC user   | *(none — required)* | `-rpcuser=<user>` |
| RPC pass   | *(none — required)* | `-rpcpassword=<pw>` |

**RPC user and password have no default.** The daemon refuses to start
RPC unless `rpcuser` and `rpcpassword` are set in its `triangles.conf`.
You must either set them in the conf, or pass them on the command line.

The conf is found in this order (highest precedence first):

1. **`-conf=<absolute-path>`** flag on the command line
2. **`<datadir>/triangles.conf`** — datadir resolved from `-datadir`
   if given, otherwise from the default per-platform path (see below)
3. **Hard-coded fallback** — `triangles.conf` in the current working
   directory (rarely useful; only fires if neither `-conf` nor `-datadir`
   is set and the cwd happens to contain the file)

## Default data directories

When `-datadir` is not passed, `triangles-cli` looks in:

| Platform | Path |
|---|---|
| Linux   | `$HOME/.cryptographic-triangles` |
| macOS   | `$HOME/Library/Application Support/CryptographicTriangles` |
| Windows | `%APPDATA%\CryptographicTriangles` |

The conf lookup in step 2 above resolves to
`<default-datadir>/triangles.conf`. **If you keep your conf anywhere
else — common for ops setups with custom data dirs — you must either
pass `-conf` explicitly, or pass `-datadir` so the conf is found
alongside it.**

## Operating a node with a non-default data directory

Most production nodes do **not** use the default datadir. The most
common ops shapes are:

### Shape 1: Custom datadir, conf in the same directory

```bash
# Daemon runs with:
trianglesd -datadir=/var/lib/triangles -conf=/var/lib/triangles/triangles.conf

# CLI uses the same -datadir, and the conf is found automatically:
triangles-cli -datadir=/var/lib/triangles getinfo
```

`-conf` is omitted because `triangles-cli` infers
`<datadir>/triangles.conf` when `-conf` is not given.

### Shape 2: Custom datadir, conf at an unrelated path

```bash
# Conf lives somewhere else entirely (e.g. under /etc):
triangles-cli -conf=/etc/triangles/triangles.conf -datadir=/var/lib/triangles getinfo
```

When `-conf` is an **absolute path**, the `-datadir` flag is only used
for resolving other relative paths (logs, pid file, etc.) — the conf
itself is read from the absolute `-conf` path.

### Shape 3: Default datadir, override a single flag

```bash
# Use the default datadir but connect to a daemon on a different port
# (e.g. testnet daemon, or remote node via SSH tunnel):
triangles-cli -rpcport=19112 -rpcuser=tripi -rpcpassword=secret getinfo
```

### Shape 4: Multiple nodes on the same box (no flag conflicts)

```bash
# Mainnet node, datadir /var/lib/triangles-mainnet
triangles-cli -datadir=/var/lib/triangles-mainnet -rpcport=19111 getinfo

# Testnet node, datadir /var/lib/triangles-testnet
triangles-cli -datadir=/var/lib/triangles-testnet -rpcport=19112 -testnet getinfo
```

## Common operations

All examples assume `-datadir=/var/lib/triangles` for the production
node. Drop the flag if your conf lives at the default path.

```bash
# ── Chain state ──────────────────────────────────────────────
triangles-cli -datadir=/var/lib/triangles getblockchaininfo
triangles-cli -datadir=/var/lib/triangles getbestblockhash
triangles-cli -datadir=/var/lib/triangles getblockcount
triangles-cli -datadir=/var/lib/triangles getdifficulty
triangles-cli -datadir=/var/lib/triangles getnetworkinfo
triangles-cli -datadir=/var/lib/triangles getconnectioncount

# ── Wallet ───────────────────────────────────────────────────
# List unspent outputs
triangles-cli -datadir=/var/lib/triangles listunspent

# Balance
triangles-cli -datadir=/var/lib/triangles getbalance
triangles-cli -datadir=/var/lib/triangles getbalance "*" 6   # 6-confirmations

# Send
triangles-cli -datadir=/var/lib/triangles sendtoaddress <addr> <amount> ["comment"]

# Backup wallet — ALWAYS back up before any operation that
# mutates the wallet (sendtoaddress, importprivkey, keypoolrefill...)
triangles-cli -datadir=/var/lib/triangles backupwallet /root/tri-wallet-$(date +%F).dat

# ── Staking ──────────────────────────────────────────────────
triangles-cli -datadir=/var/lib/triangles getstakinginfo
triangles-cli -datadir=/var/lib/triangles setstaking true|false

# ── Snapshots (if your node is a snapshot publisher) ─────────
triangles-cli -datadir=/var/lib/triangles getsnapshotinfo
```

For the full list of available RPC commands, run:

```bash
triangles-cli -datadir=/var/lib/triangles help
triangles-cli -datadir=/var/lib/triangles help <command>     # help for one
```

## Output formats

The default output is **pretty-printed JSON**. For piping into `jq`
or other tools, add `-raw`:

```bash
triangles-cli -datadir=/var/lib/triangles -raw getblockcount
#  2418017

triangles-cli -datadir=/var/lib/triangles -raw getbestblockhash | head -c 64
```

For a synthesized summary (version, balance, blocks, connections,
stake weight) without having to chain multiple calls:

```bash
triangles-cli -datadir=/var/lib/triangles -getinfo
```

## The `tri` wrapper (recommended for humans)

`scripts/tri/` ships a friendly bash wrapper that takes care of
`-datadir` / `-rpcuser` / `-rpcpassword` from a single config file.
See `scripts/tri/README.md` for install + config. Once installed:

```bash
tri getinfo
tri getblockchaininfo
tri sendtoaddress <addr> <amount>
```

…with no need to remember flags. The wrapper reads
`/etc/tri/nodes.conf` (or whatever you set `TRI_NODES_CONF` to).

## Reading JSON-RPC responses into shell variables

`triangles-cli` is one-shot — each invocation connects, sends one
request, prints the result, exits. To grab a field:

```bash
# Single field, no jq
HEIGHT=$(triangles-cli -datadir=/var/lib/triangles -raw getblockcount)
echo "Chain height: $HEIGHT"

# With jq for nested fields
NETWORK=$(triangles-cli -datadir=/var/lib/triangles -raw getnetworkinfo \
          | jq -r .networkid)
```

## Cross-host operation (SSH tunnel)

To run a CLI command against a node on a different host without
exposing RPC publicly, tunnel the port over SSH first:

```bash
# Local:19111 -> remote:19111 over SSH
ssh -f -N -L 19111:127.0.0.1:19111 user@node.example.com

# Now talk to the remote daemon as if it were local:
triangles-cli -rpcconnect=127.0.0.1 -rpcport=19111 \
              -rpcuser=<user> -rpcpassword=<pw> getinfo
```

Or use the `tri` wrapper, which has a built-in SSH host setting —
see `scripts/tri/README.md`.

## Common pitfalls

### "missing RPC credentials" with no useful error

The CLI prints:
```
triangles-cli: missing RPC credentials. Set rpcuser/rpcpassword in triangles.conf
              or pass -rpcuser=<user> -rpcpassword=<pw> on the command line.
              (RPC config file: /root/.cryptographic-triangles/triangles.conf)
```

This message is **misleading in one case**: the conf path it prints is
the *fallback* path the CLI would have used. The actual conf it
*tried* to read is the one resolved from your `-conf` or `-datadir`
flag. If you passed `-conf` and still see this, your conf is missing
`rpcuser=` or `rpcpassword=`, or has them commented out.

If you **did not** pass `-datadir` or `-conf`, the message is literal:
the CLI looked at `<default-datadir>/triangles.conf` and did not find
`rpcuser`/`rpcpassword` there.

**Fix:** either edit the conf and add credentials, or pass them on the
command line:
```bash
triangles-cli -rpcuser=trianglesrpc -rpcpassword=secret -datadir=/var/lib/triangles getinfo
```

### Daemon not running

If the daemon isn't running, `triangles-cli` will fail to connect
after a few seconds. Verify the daemon is up first:

```bash
systemctl status trianglesd        # systemd-managed install
pgrep -af trianglesd                # manual install
tail -50 /var/log/trianglesd.log    # recent log lines
```

### Testnet vs mainnet port mismatch

Mainnet default is `19111`; testnet is `19112`. If you run a testnet
daemon but invoke the CLI without `-testnet`, the CLI connects to
`19111` (empty mainnet port) and fails. Use either:

```bash
triangles-cli -testnet -datadir=/var/lib/triangles-testnet getinfo
# OR (equivalent):
triangles-cli -rpcport=19112 -datadir=/var/lib/triangles-testnet getinfo
```

### Multiple nodes on one host

If you run two daemons on the same box (e.g. mainnet + testnet), you
need to set **different** `rpcport=` for each in their respective
confs, and pass the matching `-rpcport` to the CLI. Default
`127.0.0.1:<port>` will not route correctly otherwise.

## Reference: all flags

| Flag | Purpose |
|---|---|
| `-conf=<path>`      | Path to triangles.conf (absolute path recommended) |
| `-datadir=<path>`   | Data directory; conf resolved to `<datadir>/triangles.conf` if `-conf` is not absolute |
| `-testnet`          | Use testnet RPC port (19112 instead of 19111) |
| `-rpcconnect=<ip>`  | RPC host (default `127.0.0.1`) |
| `-rpcport=<port>`   | RPC port (default `19111` mainnet, `19112` testnet) |
| `-rpcuser=<user>`   | RPC username (overrides conf) |
| `-rpcpassword=<pw>` | RPC password (overrides conf) |
| `-stdin`            | Read extra command params from stdin, one per line |
| `-raw`              | Print raw JSON, no pretty-printing |
| `-getinfo`          | Synthesized summary from multiple RPCs |
| `-version`          | Print version and exit |
| `-?` / `-h`         | Print help and exit |

## See also

- `contrib/triangles.conf.example` — daemon configuration reference
- `scripts/tri/README.md` — `tri` wrapper (operator-friendly alias)
- `doc/release-process.md` — release pipeline
- `doc/build-unix.txt` — building the CLI from source
