# Triangles (TRI) RPC Command Reference

This document describes every RPC command available in the Triangles daemon (`trianglesd`) and Qt wallet. Connect via JSON-RPC on port **19112** (default). All commands can also be run from the Qt wallet's debug console.

Triangles is a Tor-only PoS cryptocurrency. PoW ended at block 9000; from block 9001 onward the chain is pure Proof-of-Stake with 33% annual interest (coin-age based). Block time is 2 minutes. Max supply is 2,222,222 TRI.

---

## Server Control

| Command | Parameters | Description |
|---------|-----------|-------------|
| `help` | `[command]` | List all commands, or get detailed help for a specific command. |
| `stop` | | Shut down the daemon. |

---

## Blockchain

| Command | Parameters | Description |
|---------|-----------|-------------|
| `getbestblockhash` | | Returns the hash of the tip of the best chain. |
| `getblockcount` | | Returns the current block height. |
| `getblockhash` | `<index>` | Returns the block hash at the given height. |
| `getblock` | `<hash> [txinfo]` | Returns block details for the given hash. Set `txinfo=true` for full transaction data. |
| `getblockbynumber` | `<number> [txinfo]` | Same as `getblock` but accepts a height instead of a hash. |
| `getblockheader` | `<hash> [verbose=true]` | Returns block header data. If verbose is false, returns hex-encoded header. |
| `getblockchaininfo` | | Returns chain state info: chain name, block height, best hash, difficulty, etc. |
| `getdifficulty` | | Returns current PoW and PoS difficulty values. |
| `gettxoutsetinfo` | | Returns statistics about the UTXO set (total txouts, size, etc.). |
| `getrawmempool` | | Returns all transaction IDs currently in the mempool. |
| `getcheckpoint` | | Returns info about the current synchronized checkpoint. |
| `getchaintips` | | Returns info about all known chain tips (forks). |
| `invalidateblock` | `<hash>` | Permanently marks a block as invalid and rewinds the chain past it. |
| `reconsiderblock` | `<hash>` | Removes the invalid mark from a previously invalidated block. |
| `recalculatesupply` | | Recalculates money supply by summing all UTXOs. Updates the stored value at the chain tip and persists to disk. Returns old/new supply and difference. |
| `settxfee` | `<amount>` | Sets the transaction fee per kB. Amount is rounded to nearest 0.01. |
| `estimatefee` | `<nblocks>` | Estimates the fee per kB needed for confirmation within `nblocks` blocks. |

---

## Address Index

These commands query the address index. The daemon must be running with `-addressindex=1`.

| Command | Parameters | Description |
|---------|-----------|-------------|
| `getaddressbalance` | `{"addresses":["addr",...]}` | Returns confirmed balance for the given address(es). |
| `getaddressutxos` | `{"addresses":["addr",...]}` | Returns all unspent outputs for the given address(es). |
| `getaddresstxids` | `{"addresses":["addr",...], "start":n, "end":n}` | Returns transaction IDs for the given address(es), optionally filtered by block range. |

---

## Mining & Staking

| Command | Parameters | Description |
|---------|-----------|-------------|
| `getmininginfo` | | Returns mining-related info: height, difficulty, network hashrate, etc. |
| `getstakinginfo` | | Returns staking-related info: whether staking is active, weight, expected time to stake, etc. |
| `getsubsidy` | `[nTarget]` | Returns the PoW subsidy value for the given target height (historical reference only since PoW ended at block 9000). |

---

## Network

| Command | Parameters | Description |
|---------|-----------|-------------|
| `getconnectioncount` | | Returns the number of peer connections. |
| `getpeerinfo` | | Returns detailed info about each connected peer (address, version, ping time, etc.). |
| `getnetworkinfo` | | Returns P2P network state: version, protocol, peer mix, connections, relay fee, etc. |
| `getseedlist` | | Returns the list of configured seed nodes. |
| `addnode` | `<node> <add\|remove\|onetry>` | Add or remove a node from the manual peer list, or try connecting once. For Tor nodes use the `.onion` address. |
| `disconnectnode` | `<node>` | Immediately disconnects from the specified peer. |
| `sendalert` | `<message> <privatekey> <minver> <maxver> <priority> <id> [cancelupto]` | Broadcasts a network alert (requires the alert master private key). |

---

## Wallet — General

| Command | Parameters | Description |
|---------|-----------|-------------|
| `getinfo` | | Returns general info: version, balance, stake, block height, connections, etc. |
| `getwalletinfo` | | Returns wallet-specific info: balance, unconfirmed, immature, txcount, keypoolsize, etc. |
| `getbalance` | `[account] [minconf=1]` | Returns total available balance (optionally for a specific account). |
| `checkwallet` | | Checks wallet database for consistency errors. |
| `repairwallet` | | Attempts to repair the wallet database. |
| `resendtx` | | Re-broadcasts all unconfirmed wallet transactions. |

---

## Wallet — Addresses & Accounts

| Command | Parameters | Description |
|---------|-----------|-------------|
| `getnewaddress` | `[account]` | Generates a new receiving address (optionally assigned to an account). |
| `getnewpubkey` | `[account]` | Returns a new public key for the wallet. |
| `getaccountaddress` | `<account>` | Returns the current receiving address for the given account. |
| `setaccount` | `<address> <account>` | Assigns an address to the given account label. |
| `getaccount` | `<address>` | Returns the account label for the given address. |
| `getaddressesbyaccount` | `<account>` | Returns all addresses assigned to the given account. |
| `listaddressgroupings` | | Returns addresses grouped by common ownership (based on transaction history). |
| `validateaddress` | `<address>` | Validates a Triangles address and returns info (ismine, account, pubkey, etc.). |
| `validatepubkey` | `<pubkey>` | Validates a Triangles public key. |
| `listaccounts` | `[minconf=1]` | Returns all account names and their balances. |

---

## Wallet — Sending

| Command | Parameters | Description |
|---------|-----------|-------------|
| `sendtoaddress` | `<address> <amount> [comment] [comment-to]` | Sends TRI to an address. Returns the transaction ID. |
| `sendfrom` | `<fromaccount> <address> <amount> [minconf=1] [comment] [comment-to]` | Sends TRI from a specific account. |
| `sendmany` | `<fromaccount> {"addr":amount,...} [minconf=1] [comment]` | Sends TRI to multiple addresses in a single transaction. |
| `move` | `<fromaccount> <toaccount> <amount> [minconf=1] [comment]` | Moves funds between accounts (internal bookkeeping only, no on-chain tx). |

---

## Wallet — Transaction History

| Command | Parameters | Description |
|---------|-----------|-------------|
| `listtransactions` | `[account] [count=10] [from=0]` | Returns the most recent transactions (optionally filtered by account). |
| `listsinceblock` | `[blockhash] [target-confirmations]` | Returns all transactions since the given block. |
| `gettransaction` | `<txid>` | Returns detailed info about a wallet transaction. |
| `getreceivedbyaddress` | `<address> [minconf=1]` | Returns total amount received by an address. |
| `getreceivedbyaccount` | `<account> [minconf=1]` | Returns total amount received by an account. |
| `listreceivedbyaddress` | `[minconf=1] [includeempty=false]` | Returns amounts received for each address. |
| `listreceivedbyaccount` | `[minconf=1] [includeempty=false]` | Returns amounts received for each account. |

---

## Wallet — Staking Control

| Command | Parameters | Description |
|---------|-----------|-------------|
| `reservebalance` | `[reserve] [amount]` | Show or set a reserve balance that will not be used for staking. `reserve` is true/false, `amount` is the TRI to reserve. |

---

## Wallet — Security

| Command | Parameters | Description |
|---------|-----------|-------------|
| `encryptwallet` | `<passphrase>` | Encrypts the wallet with the given passphrase. **This shuts down the daemon.** The wallet must be re-started and unlocked afterward. |
| `walletpassphrase` | `<passphrase> <timeout> [stakingonly]` | Unlocks the wallet for `timeout` seconds. Set `stakingonly=true` to allow staking but prevent sending. |
| `walletpassphrasechange` | `<oldpassphrase> <newpassphrase>` | Changes the wallet encryption passphrase. |
| `walletlock` | | Immediately locks the wallet (removes decryption key from memory). |
| `keypoolrefill` | `[new-size]` | Tops up the pre-generated key pool. |
| `makekeypair` | `[prefix]` | Generates a new public/private keypair (not added to wallet). |

---

## Wallet — Backup & Import

| Command | Parameters | Description |
|---------|-----------|-------------|
| `backupwallet` | `<destination>` | Copies `wallet.dat` to the given file path. |
| `dumpwallet` | `<filename>` | Exports all wallet private keys to a plaintext file. |
| `dumpprivkey` | `<address>` | Returns the private key (WIF format) for the given address. |
| `importwallet` | `<filename>` | Imports keys from a wallet dump file. |
| `importprivkey` | `<privkey> [label]` | Imports a single private key (WIF format) with optional label. |

---

## Wallet — Multisig

| Command | Parameters | Description |
|---------|-----------|-------------|
| `addmultisigaddress` | `<nrequired> ["key",...] [account]` | Creates an M-of-N multisig address. `nrequired` is the number of signatures needed. |
| `addredeemscript` | `<redeemScript> [account]` | Adds a P2SH redeem script to the wallet. |

---

## Wallet — Message Signing

| Command | Parameters | Description |
|---------|-----------|-------------|
| `signmessage` | `<address> <message>` | Signs a message with the private key of the given address. |
| `verifymessage` | `<address> <signature> <message>` | Verifies a signed message. Returns true/false. |

---

## Raw Transactions

| Command | Parameters | Description |
|---------|-----------|-------------|
| `listunspent` | `[minconf=1] [maxconf=9999999] ["addr",...]` | Returns unspent transaction outputs, optionally filtered by address and confirmation count. |
| `createrawtransaction` | `[{"txid":"id","vout":n},...] {"addr":amount,...}` | Creates an unsigned raw transaction from the given inputs and outputs. |
| `decoderawtransaction` | `<hex>` | Decodes a raw transaction hex string into a JSON object. |
| `decodescript` | `<hex>` | Decodes a hex-encoded script into human-readable form. |
| `signrawtransaction` | `<hex> [prevtxs] [privkeys] [sighashtype="ALL"]` | Signs a raw transaction. Can provide previous tx outputs and private keys for offline signing. |
| `sendrawtransaction` | `<hex>` | Broadcasts a signed raw transaction to the network. Returns the txid. |
| `getrawtransaction` | `<txid> [verbose=0]` | Returns raw transaction data. Set verbose=1 for decoded JSON output. |

---

## Secure Messaging (SMSG)

Triangles has a built-in encrypted peer-to-peer messaging system. Messages are stored in a DHT-like bucket system and relayed through the network.

| Command | Parameters | Description |
|---------|-----------|-------------|
| `smsgenable` | | Enables the secure messaging system. |
| `smsgdisable` | | Disables the secure messaging system. |
| `smsgoptions` | `[list\|set <optname> <value>]` | View or change secure messaging options. |
| `smsglocalkeys` | `[whitelist\|all\|wallet\|recv +/- <addr>\|anon +/- <addr>]` | Manage which local keys participate in secure messaging. |
| `smsgaddkey` | `<address> <pubkey>` | Adds someone's public key so you can send them encrypted messages. |
| `smsggetpubkey` | `<address>` | Retrieves the public key for an address (needed to send messages to it). |
| `smsgsend` | `<fromAddr> <toAddr> <message>` | Sends an encrypted message from one of your addresses to a recipient. |
| `smsgsendanon` | `<toAddr> <message>` | Sends an anonymous encrypted message (no sender address attached). |
| `smsginbox` | `[all\|unread\|clear]` | View received secure messages. Default shows unread. |
| `smsgoutbox` | `[all\|clear]` | View sent secure messages. |
| `smsgscanchain` | | Scans the blockchain for secure message public keys. |
| `smsgscanbuckets` | | Scans stored message buckets for messages addressed to your keys. |
| `smsgbuckets` | `[stats\|dump]` | View secure message bucket statistics or dump contents. |
| `smsgbroadcast` | `<fromAddr> <message>` | Broadcasts a message to all SMSG participants (not encrypted to a single recipient). |

---

## Quick Reference — Common Tasks

**Check node status:**
```
getinfo
getblockcount
getconnectioncount
getstakinginfo
```

**Check balance and transactions:**
```
getbalance
listtransactions
```

**Send coins:**
```
walletpassphrase "yourpassphrase" 60
sendtoaddress "TRIaddress" 100
walletlock
```

**Unlock for staking only:**
```
walletpassphrase "yourpassphrase" 999999999 true
```

**Add a peer manually (Tor .onion):**
```
addnode "abcdef1234567890.onion" "add"
```

**Export/import a private key:**
```
dumpprivkey "TRIaddress"
importprivkey "5KPrivKeyHere" "mylabel"
```

**Fix incorrect money supply display:**
```
recalculatesupply
```

**Full reindex (rebuild block index from raw data):**
```
trianglesd -reindex
```

---

## Connection Info

| Setting | Value |
|---------|-------|
| Default RPC port | 19112 |
| Default P2P port | 24112 |
| Config file (Windows) | `%APPDATA%\triangles\triangles.conf` |
| Config file (Linux) | `~/.triangles/triangles.conf` |
| Protocol version | 70205 |
| Network | Tor-only |
