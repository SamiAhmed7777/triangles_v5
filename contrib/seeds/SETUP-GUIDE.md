# Triangles Dynamic Seed Node - Setup Guide

## Overview

Triangles v5.5.0+ uses a dynamic HTTP seed list instead of hardcoded addresses.
A collector script runs on a VPS alongside a Triangles node, periodically
querying the node for known .onion peers and publishing them to a static file.
New wallets fetch this file on startup to bootstrap peer discovery.

Once any wallet syncs and obtains its own .onion address, other nodes learn
about it via P2P address exchange. The collector picks it up automatically
on its next run. No manual intervention is needed after initial setup.

## Requirements

- Linux VPS
- Triangles daemon (`trianglesd`) running with Tor enabled
- A web server (Caddy, nginx, Apache, etc.)
- DNS control for the domain serving the seed list
- `jq` and `curl` (`apt install jq curl`)

## Step 1: DNS

Create an A record for the seed list hostname pointing to the VPS IP address.

The default hostname the wallet fetches is `seeds.cryptographic-triangles.org`.
This can be overridden per-node with the `-seedurl` flag.

## Step 2: Web Server

Create a directory for the seed file:

```bash
sudo mkdir -p /var/www/seeds
sudo chown $USER:$USER /var/www/seeds
```

Configure the web server to serve that directory on the seed list hostname.

**Caddy example** (add to Caddyfile):

```
seeds.cryptographic-triangles.org {
    root * /var/www/seeds
    file_server
}
```

**nginx example** (add server block):

```
server {
    listen 80;
    server_name seeds.cryptographic-triangles.org;
    root /var/www/seeds;
}
```

Reload the web server after making changes.

## Step 3: Install the Collector Script

```bash
sudo cp contrib/seeds/collect-seeds.sh /usr/local/bin/collect-seeds.sh
sudo chmod +x /usr/local/bin/collect-seeds.sh
```

## Step 4: Configure and Test

The script communicates with `trianglesd` via JSON-RPC. It reads credentials
from environment variables. Check `triangles.conf` for `rpcuser` and `rpcpassword`.

Run it manually to verify:

```bash
export RPC_USER="your_rpc_username"
export RPC_PASSWORD="your_rpc_password"
export RPC_PORT="19112"
export OUTPUT_FILE="/var/www/seeds/seeds.txt"

/usr/local/bin/collect-seeds.sh
```

Expected output: `Updated /var/www/seeds/seeds.txt with N seeds`

The resulting file should contain one `.onion:port` entry per line:

```
# Triangles seed nodes - auto-generated 2026-04-01T12:00:00Z
exampleaddress1234567890abcdefghijklmnopqrstuvwxyz234567.onion:24112
anotheraddress1234567890abcdefghijklmnopqrstuvwxyz23456.onion:24112
```

## Step 5: Cron Job

Schedule the collector to run every 5 minutes:

```bash
crontab -e
```

Add:

```
*/5 * * * * RPC_USER="your_rpc_username" RPC_PASSWORD="your_rpc_password" OUTPUT_FILE="/var/www/seeds/seeds.txt" /usr/local/bin/collect-seeds.sh >> /var/log/triangles-seeds.log 2>&1
```

## Step 6: Verify End-to-End

From any machine:

```bash
curl http://seeds.cryptographic-triangles.org/seeds.txt
```

The response should list .onion addresses.

## Troubleshooting

**"no onion seeds found"**
The node has not yet learned any .onion peer addresses. Ensure Tor is enabled
and the node has at least one connected peer. Check with `trianglesd getpeerinfo`.

**"RPC call failed"**
Verify `trianglesd` is running and RPC credentials are correct:
```bash
curl -s --user "user:pass" --data-binary \
  '{"jsonrpc":"1.0","method":"getinfo","params":[]}' \
  http://127.0.0.1:19112/
```

**seeds.txt not updating**
Check the cron log: `tail /var/log/triangles-seeds.log`

## How It Works

1. The collector calls the `getseedlist` RPC, which returns all known .onion
   addresses from the node's address manager
2. Results are written to a static text file served by the web server
3. On startup, Triangles wallets fetch this file and add the addresses to
   their peer database
4. As wallets connect and exchange addresses via P2P, new .onion addresses
   propagate across the network
5. The collector discovers newly-propagated addresses on its next run

This creates a fully automatic cycle where every online wallet with a Tor
hidden service becomes a discoverable seed node.
