## TRI Node Upgrade to v5.8.0 - April 14, 2026

This document outlines the process and results of upgrading the TRI network nodes to version 5.8.0.

### Initial State

- **DNS2:** `v5.7.9` @ block `2,203,611`
- **DNS3:** `v5.7.5` @ block `2,204,954`
- **Contabo Seeds:** `v5.7.9` @ block `2,203,594`

Nodes were on multiple versions and forks.

### Upgrade Process

1.  **Version Confirmation:** Verified `v5.8.0` was available on GitHub.
2.  **Upgrades:**
    - DNS2 upgraded to `v5.8.0` via `dpkg`.
    - DNS3 upgraded to `v5.8.0` via `dpkg`.
    - Contabo seeds (`tri-seed-1` to `4`) upgraded to `v5.8.0` via `dpkg` inside their containers.
3.  **Chain Reset:** To resolve forks, the chain data (blocks, chainstate, peers) was wiped on DNS2 and all Contabo seeds. Wallets and configs were preserved. DNS3 was left as the canonical chain source.

### Current Status

- All nodes are now running `v5.8.0`.
- Nodes are currently re-syncing to the canonical chain. Monitoring is in progress.

### DNS2 Wallet Corruption and Recovery

- **Symptom:** `triangles.service` on DNS2 was in a crash loop. Logs showed a recurring `CDB() : can't open database file wallet.dat, error -30973` error.
- **Diagnosis:** `wallet.dat` file was corrupted.
- **Recovery:**
    1. The corrupted wallet was moved to `wallet.dat.corrupted` for safety.
    2. The latest wallet backup (`dns2-wallet_20260414_031501.dat`) was restored from Dropbox.
    3. The `triangles.service` was restarted.

This restored the wallet to a healthy state.
