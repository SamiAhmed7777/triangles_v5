# Trusted Snapshot Publisher — Operator Guide

This document explains how the trusted snapshot publisher mechanism works
in Triangles and how to rotate the publisher without rebuilding the
daemon. It is written for the person who operates the Triangles network
after Sami — whoever that turns out to be.

## Background

The Triangles daemon verifies that any UTXO snapshot it loads was
**signed by a trusted publisher**. This prevents a malicious snapshot
file from tricking a node into accepting a fake chain state.

In versions before v6.1.8, the trusted publisher list was hardcoded
in the binary. To rotate keys, the daemon had to be rebuilt and
re-released. That was bad for handover.

Starting with v6.1.8, the daemon supports a **runtime-configurable
single-slot trusted publisher** via RPC. The compiled-in fallback list
is still consulted if no runtime publisher is set, so a fresh daemon
never fails to verify an old snapshot.

## The model — Design A (single-slot, auto-replace)

- **At most ONE runtime publisher exists at any time.**
- Calling `settrustedv2snapshotpublisher <addr>` **atomically
  replaces** the current publisher. The previous one is dropped
  immediately. There is no grace period, no retirement list, no
  rollback path. Pure single-slot.
- The active publisher is persisted to
  `<datadir>/snapshot-publisher.json`, so it survives daemon
  restarts.
- The built-in fallback list (read-only, compiled into the binary) is
  consulted only if no runtime publisher is set. That list contains:
  - `TG8f76yktTxDrT7JJymY3wVAusXiD3fVvX` — Sami's legacy snapshot
    publisher key (the original, used from v6.1.5 through v6.1.7).

## The three RPCs

### `settrustedv2snapshotpublisher <address>`

Atomically replaces the active trusted publisher. The previous
publisher is dropped immediately. The new publisher is persisted to
`<datadir>/snapshot-publisher.json` so the choice survives restarts.

```
triangles-cli settrustedv2snapshotpublisher TGotWuftzH7rD9tXC7whE8EXiyC3mr1CrH
```

Result:
```json
{
  "previous": "TG8f76yktTxDrT7JJymY3wVAusXiD3fVvX",
  "current":  "TGotWuftzH7rD9tXC7whE8EXiyC3mr1CrH"
}
```

The `previous` field is empty if no runtime publisher was set before.

### `gettrustedv2snapshotpublisher`

Returns the currently active runtime publisher.

```
triangles-cli gettrustedv2snapshotpublisher
```

Result:
```json
{
  "active": "TGotWuftzH7rD9tXC7whE8EXiyC3mr1CrH",
  "has_runtime_override": true
}
```

If `has_runtime_override` is `false`, only the built-in fallback list
is consulted. The fallback currently contains `TG8f76yktTxDrT7JJymY3wVAusXiD3fVvX`.

### `unsettrustedv2snapshotpublisher`

Clears the runtime override. Reverts to the built-in fallback list.
Also removes `<datadir>/snapshot-publisher.json`.

```
triangles-cli unsettrustedv2snapshotpublisher
```

Use this if you want to "go back to the legacy trusted signer"
without a rebuild.

## Common rotation scenarios

### Rotate to a new key (forward rotation)

1. Generate a new key in the wallet:
   ```
   triangles-cli getnewaddress
   # returns: TNewAddressHere...
   ```
2. (Optional but recommended) Label it so you remember its role:
   ```
   triangles-cli setaccount TNewAddressHere... "snapshot publisher"
   ```
3. Set it as the trusted publisher:
   ```
   triangles-cli settrustedv2snapshotpublisher TNewAddressHere...
   ```
4. Verify:
   ```
   triangles-cli gettrustedv2snapshotpublisher
   ```
   Should show `active: TNewAddressHere...`.

Old publisher is dropped immediately. New one is in effect for this
daemon and any daemon that syncs from `<datadir>/snapshot-publisher.json`.

### Roll back to the legacy publisher

If the new key is lost / compromised / you just want to revert:

```
triangles-cli unsettrustedv2snapshotpublisher
```

This reverts to the built-in fallback (`TG8f76ykt...`). No rebuild
required. The legacy address will continue to verify any snapshot
that was signed before your rotation.

### Rotate during a handover (publisher A hands off to publisher B)

1. Publisher B installs v6.1.8+ daemon.
2. Publisher B sets themselves as the trusted publisher:
   ```
   triangles-cli settrustedv2snapshotpublisher TBsAddress...
   ```
3. Publisher B signs a new snapshot with their key (see
   `publishcheckpoint` in `TRIANGLES-RPC-COMMANDS.md`).
4. Publisher A can leave the network; their key is no longer trusted
   on any node that has called `settrustedv2snapshotpublisher`.

Note: because Design A auto-drops the previous publisher, **only one
operator can publish at a time.** If you need overlap (both A and B
publishing during a transition), that requires Design B (multi-slot
with grace period) — not supported in v6.1.8. Contact Sami for the
upgrade path.

## Files

| Path | Purpose |
|---|---|
| `<datadir>/snapshot-publisher.json` | Runtime publisher override. Plain JSON. Inspectable with `cat`. |
| `<datadir>/wallet.dat` | Must contain the privkey for the active publisher, otherwise `publishcheckpoint` will fail at signing time. (Trust is governed by the override; signing is governed by the wallet.) |

### `<datadir>/snapshot-publisher.json` format

```json
{
  "address": "TGotWuftzH7rD9tXC7whE8EXiyC3mr1CrH",
  "set_at": 1752168000,
  "note": "Set via triangles-cli settrustedv2snapshotpublisher. Replace atomically; previous publisher is dropped."
}
```

`set_at` is the Unix timestamp when the RPC was last called. `note` is
informational only.

## Recovery if RPC fails

If for some reason the runtime override can't be persisted (e.g. JSON
write fails), the RPC returns a warning but the in-memory change is
already live for the current session. To check:

```
triangles-cli gettrustedv2snapshotpublisher
```

If `active` is set, you're good for the current session. The next
daemon restart will lose it unless `snapshot-publisher.json` exists.
Inspect it manually:

```
cat ~/.triangles/snapshot-publisher.json
```

If the file doesn't exist but you need the override to survive restart,
hand-write it:
```json
{
  "address": "TGotWuftzH7rD9tXC7whE8EXiyC3mr1CrH",
  "set_at": 1752168000,
  "note": "Hand-set; rotate via triangles-cli settrustedv2snapshotpublisher."
}
```

The daemon reads this file at startup. Address must be 34 chars and
start with `T`. Anything else is logged and ignored.

## When you DO need a rebuild

- **Adding a new entry to the built-in fallback list** (the
  read-only list compiled into the binary). Edit
  `BUILTIN_TRUSTED_SNAPSHOT_SIGNERS[]` in `src/bootstrap.cpp`, rebuild,
  release. This is only needed if you want a publisher to be trusted
  *without* any operator running the RPC.
- **Changing the RPC names or argument shapes.** Edit source, rebuild.

For everyday "I want to add or rotate a trusted publisher," the RPC
is enough. Don't rebuild.

## Why "single-slot, no grace period"

Sami asked for it explicitly when designing the operator-experience
for this feature. The trade-off: if the active key is lost or
compromised, there's no automatic fallback. The operator must either
re-add the previous key (which requires they kept the JSON file or
remember the address) or rebuild with the new key in
`BUILTIN_TRUSTED_SNAPSHOT_SIGNERS[]`.

If this trade-off becomes painful — for example if multiple
operators need to publish during a handover — the alternative is
Design B (multi-slot with grace period). That's a one-day patch on
top of this one. Ask Sami for the upgrade.

## Versioning

This feature is introduced in **v6.1.8**. Daemons older than v6.1.8
still use the hardcoded `TG8f76ykt...` only — they cannot use the new
key until they upgrade.

## Related RPCs

For the publishing side (signing snapshots, not verifying them),
see:

- `publishcheckpoint <interval> <signing_address> <output_path>` —
  builds and signs a checkpoint document.
- `gencheckpoints` — generates raw checkpoint data without signing.
- `getcheckpoint` — returns the current synchronized checkpoint.

See `TRIANGLES-RPC-COMMANDS.md` for full details on those.