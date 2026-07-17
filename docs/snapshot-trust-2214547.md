# Snapshot trust record: height 2,214,547

This release pins the canonical mainnet UTXO snapshot at height 2,214,547.
The snapshot is accepted only when both its complete-file SHA-256 and its tip
block hash match values compiled into `src/checkpoints.cpp`.

## Pinned values

- Snapshot URL: `https://bootstrap.cryptographic-triangles.org/utxo-snapshot-2214547.utx`
- Size: `989800942` bytes
- File SHA-256: `f2f277c70536c1acb0cf96b331c5e3398d92d69d88061190c3673e336b104895`
- Tip height: `2214547`
- Tip block hash: `e7a1363144b39c5ae70e4c32757055e07ac7fa859cb60cbe1acd883d9010f8ba`
- Previous compiled checkpoint: height `2214400`, hash
  `8ebb818f7280850c5a3916b7c8a2bca603f7c4f9926d3cdc2262f726035d96ed`

## Verification performed

On 2026-07-12, the file was downloaded over certificate-verified HTTPS. Its
byte count and complete-file SHA-256 matched the values above. Its internal v3
header reported the same network, height, and tip block hash.

Two distinct live Tor peers were queried directly from the previous compiled
checkpoint. Their raw headers matched byte-for-byte from heights 2,214,401
through 2,214,547:

`d5aa5d57d367a0f83686aafbea3072c90832a571fcedd681b09006abfb6fb229`

One peer was five blocks beyond the snapshot and independently linked height
2,214,548 to the pinned snapshot block hash. The other peer's tip was exactly
height 2,214,547 and produced the same raw snapshot-tip header.

## Reproduce the peer check

Run `contrib/verify_checkpoint_headers.py` through a local Tor SOCKS proxy and
provide at least two independently operated peers:

```sh
python3 contrib/verify_checkpoint_headers.py \
  --socks-port 19099 \
  --checkpoint-height 2214400 \
  --checkpoint-hash 8ebb818f7280850c5a3916b7c8a2bca603f7c4f9926d3cdc2262f726035d96ed \
  --snapshot-height 2214547 \
  --expected-snapshot-block-hash e7a1363144b39c5ae70e4c32757055e07ac7fa859cb60cbe1acd883d9010f8ba \
  peer-one.onion peer-two.onion
```

The command exits nonzero if a peer fails, the checkpoint anchor is wrong,
the peers disagree on the raw header sequence, or the expected snapshot block
hash is not independently linked by a later header.

This record does not make a UTXO snapshot trustless. Release maintainers must
repeat the independent chain and file checks before updating either compiled
value for a future snapshot.
