# tri — Cryptographic Triangles CLI

A friendly bash wrapper around `trianglesd` RPC for humans and agents.

## Install

```bash
# System-wide
sudo cp tri /usr/local/bin/tri
sudo chmod +x /usr/local/bin/tri
sudo mkdir -p /etc/tri
sudo cp nodes.conf.example /etc/tri/nodes.conf
# Edit /etc/tri/nodes.conf with your node's RPC credentials

# Bash completion
sudo cp tri-completion.bash /etc/bash_completion.d/

# Zsh completion
sudo cp _tri_zsh_completion /usr/local/share/zsh/site-functions/_tri
```

## Config

Edit `/etc/tri/nodes.conf`:

```bash
TRI_SSH_HOST="100.81.59.99"       # Node IP (or remove for local)
TRI_SSH_USER="root"
TRI_RPC_PORT="19112"
TRI_RPC_USER="your-rpc-user"
TRI_RPC_PASS="your-rpc-password"
# TRI_WALLET_PASSPHRASE="wallet-passphrase"  # If wallet is encrypted
```

## Commands

### Info
- `tri` — Status overview
- `tri status` — Detailed node status
- `tri balance` — Wallet balance + UTXO count
- `tri peers` — Connected peers
- `tri stake` — Staking info

### Wallet
- `tri address new` — New address
- `tri address list` — List addresses
- `tri address balance` — Per-address balances
- `tri send <addr> <amt> [memo]` — Send TRI
- `tri tx [N]` — Recent transactions
- `tri tx <txid>` — Transaction details

### Secure Messaging
- `tri msg inbox` — Read messages
- `tri msg outbox` — Sent messages
- `tri msg send <from> <to> <msg>` — Send encrypted message
- `tri msg anon <to> <msg>` — Anonymous message
- `tri msg keys` — Messaging keys
- `tri msg enable` — Enable secure messaging
- `tri msg pubkey <addr>` — Get public key

### Advanced
- `tri raw <method> [params...]` — Raw RPC passthrough

## Agent Integration (Hermes, Krystie)

Both agents on DNS2 share the same `/etc/tri/nodes.conf` and can execute all commands.
For inter-agent messaging via TRI's encrypted P2P network:

1. Each agent needs a TRI address: `tri address new`
2. Enable messaging: `tri msg enable`
3. Register key: `tri raw smsglocalkeys recv + <address>`
4. Exchange addresses between agents
5. Send: `tri msg send <hermes_addr> <krystie_addr> "message"`
6. Read: `tri msg inbox`

Messages are encrypted (ECDH), routed through the Tor P2P network,
stored for 48 hours, max 4096 bytes each.
