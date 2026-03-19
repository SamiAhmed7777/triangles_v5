# Embedded Tor Rebase Notes

This repository currently contains a legacy Tor source snapshot under
`src/tor/`, but the wallet target does not build most of that tree.

## Current state

- The vendored Tor headers report `0.2.5.1-alpha-dev` in:
  - `src/tor/orconfig_linux.h`
  - `src/tor/orconfig_apple.h`
  - `src/tor/orconfig_win32.h`
- The Qt wallet target currently builds only these Tor-related sources:
  - `src/tor_embed_hooks.cpp`
  - `src/tor/onion_v3.cpp`
  - `src/tor/tor_process.cpp`
- This means the large legacy `src/tor/` tree is mostly dormant from the
  wallet build's perspective.

## Rebase target

- Target upstream Tor line: `0.4.9.x`
- Imported source tree: `src/tor/tor-src`
- Imported branch: `release-0.4.9`
- Imported commit: `1442ca4`

## Why this matters

Attempting to "upgrade embedded Tor" by rebasing the entire old source tree in
place is unnecessarily expensive if the wallet is only relying on:

- process management for a bundled Tor executable
- Tor v3 onion address/key handling
- a few local embedding hooks

The migration should preserve the embedded product experience while reducing
coupling to legacy upstream Tor internals.

## Strategy

1. Keep the product-level embedding model.
   - The wallet can still ship with Tor and launch it automatically.
2. Separate Triangles-owned glue from vendored Tor code.
   - `src/tor_embed_hooks.*` now holds local process/bootstrap helpers that
     previously lived under `src/tor/anonymize.*`.
3. Treat `src/tor/onion_v3.cpp` and `src/tor/tor_process.cpp` as the active
   compatibility boundary.
4. Re-vendor a newer upstream Tor snapshot only after deciding whether the
   product truly needs upstream Tor source in-tree or only a bundled Tor
   runtime plus the wallet's own v3/onion management code.

## Immediate next tasks

1. Audit whether any live build target still includes legacy `src/tor/*.c`
   sources beyond the current wallet target.
2. Decide whether `onion_v3.cpp` should remain wallet-owned code or be reduced
   further in favor of runtime Tor control/provisioning.
3. Add build metadata recording the intended upstream Tor version and source.
4. If full upstream vendoring is still required, import a fresh `0.4.8.19`
   tree side-by-side instead of trying to patch the legacy `0.2.5.1` tree.
