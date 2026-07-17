# Security Policy

Triangles is wallet software and should be treated as security-sensitive. Do
not use an experimental build to custody funds that you cannot afford to lose.

## Reporting a vulnerability

Please report suspected vulnerabilities through a private GitHub security
advisory for this repository. Do not include secrets, wallet files, seed
phrases, private keys, or live RPC credentials in an issue, pull request, log,
or test fixture.

Include the affected commit, platform, reproduction steps, impact, and a
minimal proof of concept when possible. Public disclosure should wait until a
fix is available and users have had a reasonable upgrade window.

## Deployment boundary

The JSON-RPC protocol uses HTTP Basic authentication and does not provide TLS.
Keep it on loopback or a private Unix host boundary. Never expose the RPC port
directly to the internet.

For application integrations:

- Run `trianglesd` as a dedicated, unprivileged operating-system user.
- Bind RPC explicitly to loopback with `rpcbind=127.0.0.1`.
- Use a unique random RPC username and password stored in a mode `0600` file.
- Set `rpcallowip=127.0.0.1` and an exact `rpcallowmethod` list.
- Keep `rest=0`, `upnp=0`, and wallet RPC methods disabled unless required.
- Do not pass RPC passwords on a process command line.
- Separate the node wallet and files from the integrating application's user.
- Start new integrations with an empty wallet and no production funds.

The container image runs as UID/GID `10001` and intentionally does not create
or print RPC credentials. Mount a private `/var/lib/triangles` volume containing
an owner-only `triangles.conf`; startup without valid RPC credentials fails with
a nonzero exit status. Do not provide wallet or RPC secrets through Docker
command arguments or environment variables.

Set `listen=0` when inbound P2P is unnecessary. When inbound peers are needed,
use `bind=<address>` and publish only the P2P port. The RPC port must remain
unpublished and loopback-bound.

Remote snapshot bootstrap is opt-in. A snapshot is accepted only when its file
hash and checkpoint are compiled into the client. Treat changes to snapshot
hashes, checkpoints, seed hosts, release keys, submodule revisions, and CI
workflows as security-critical review items.

## Wallet handling

- Encrypt wallets before funding them.
- Record the HD mnemonic offline and test recovery on an isolated machine.
- Keep multiple offline backups; filesystem permissions are not a backup.
- Encrypting the live wallet does not retroactively encrypt old copies,
  migration backups, snapshots, or filesystem remnants. Inventory and protect
  every pre-encryption copy as if it contains plaintext private keys.
- Never share a seed phrase with support personnel or paste it into an RPC call.
- Stop the node and investigate any wallet database integrity error rather than
  attempting to continue with a partially loaded wallet.

## Build trust

Build from a reviewed commit, initialize submodules at the recorded revisions,
and verify release signatures against a key fingerprint obtained through an
independent trusted channel. A valid signature proves key possession, not the
identity of the key owner.
