# I2P support (SAM v3)

Triangles runs over I2P in addition to Tor, giving the wallet a second
anonymous network and a `.b32.i2p` address shown directly above the `.onion`
address in the status bar.

I2P is **on by default** and works the same way as the embedded Tor: the wallet
auto-launches a bundled **i2pd** router as a managed child process, enables its
SAM bridge, and connects to it. The user does not have to install or configure
anything — provided the i2pd binary ships with the wallet.

## Shipping the i2pd binary

Like `tor.exe`, the wallet looks for an `i2pd` executable in several places and
launches the first one it finds:

1. Next to the wallet executable (recommended): `i2pd.exe` (Windows) / `i2pd`
   (Linux/macOS), or in an `i2pd/` subfolder beside it.
2. In the data directory (or its `i2pd/` subfolder).
3. Common system locations (`/usr/bin/i2pd`, Homebrew, `C:\i2pd\…`, etc.).

Get i2pd from https://i2pd.website/ (or your package manager) and place the
binary next to the wallet in your build/packaging step. That's the only manual
part, and it's a packaging concern, not something the end user does.

If no i2pd binary is found, the wallet logs a notice and continues with **Tor
only** — I2P is strictly additive and never blocks start-up.

## What happens at start-up

1. If a SAM bridge is already listening on `127.0.0.1:7656` (e.g. you run your
   own router), the wallet uses it and does **not** launch its own.
2. Otherwise it writes `i2pd.conf` into `<datadir>/i2pd/` (SAM enabled, other
   services off), launches i2pd, and waits for the SAM bridge to come up.
3. The SAM client then loads/creates a persistent destination
   (`<datadir>/i2p_private_key`), opens a STREAM session, derives the
   `.b32.i2p` address (`base32(SHA-256(destination))`), accepts inbound I2P
   streams, and dials outbound `.b32.i2p` peers.
4. On wallet exit, the SAM session is closed and the i2pd child process is
   terminated (an external router you started yourself is left running).

The first session takes a little longer while i2pd builds tunnels; the address
appears once the bridge is ready.

## Options

```
-i2p                 Enable I2P; auto-launches bundled i2pd (default: 1; -i2p=0 to disable)
-i2psam=<ip:port>    SAM bridge address (default: 127.0.0.1:7656).
                     A non-loopback address disables the bundled router and
                     connects to that external bridge instead.
```

## Checking it

* GUI: the `.b32.i2p` address sits on top of the `.onion` in the status bar;
  click either to copy.
* RPC: `getinfo` shows `toraddress` and `i2paddress`; `getnetworkinfo` shows
  `toraddress` and an `i2p` object (`enabled`, `active`, `address`, `peers`).

## Notes / limitations

* The address serialization format carries a flag for I2P addresses, so **all
  nodes must run this build** to exchange I2P peers; an old `peers.dat` is
  discarded.
* `i2p_private_key` is your stable I2P identity — back it up, don't delete it.
* This was implemented without a build/CI environment here; build and test
  against a real i2pd before relying on it.
