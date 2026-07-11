#!/usr/bin/env bash
# Generate a starter fuzz corpus from the existing script test JSON files.
# Each output file is: [uint8 scriptLen][scriptLen raw bytes]
#
# This is a hand-rolled extractor because the JSON uses stringified script
# syntax ("OP_DUP OP_HASH160 ... 0x76a914..."), not raw bytes. For the
# starter set we just dump the first N bytes of each test's scriptPubKey
# field — enough to give libFuzzer a structural starting point. Mutation
# will explore the rest.
set -euo pipefail

CORPUS_DIR="${1:-corpus}"
SRC_JSON="${2:-../data/script_valid.json}"

mkdir -p "$CORPUS_DIR"

# Extract scriptPubKey fields and emit raw-prefixed corpus files.
# The JSON format is [[scriptSig, scriptPubKey, expected, ...], ...]
# We pull out element [1] (scriptPubKey) and dump its raw bytes.
python3 - <<PY
import json, sys, os, pathlib

src = pathlib.Path("$SRC_JSON")
out = pathlib.Path("$CORPUS_DIR")
out.mkdir(parents=True, exist_ok=True)

with src.open() as f:
    tests = json.load(f)

count = 0
for i, t in enumerate(tests):
    if not isinstance(t, list) or len(t) < 2:
        continue
    spk = t[1]
    if isinstance(spk, str):
        # String form ("OP_DUP OP_HASH160 ...") — skip, we want raw bytes.
        continue
    if not isinstance(spk, list):
        continue
    raw = bytes(spk[:10000])
    if not raw:
        continue
    path = out / f"script_{i:04d}.bin"
    path.write_bytes(bytes([len(raw) & 0xff]) + raw)
    count += 1

print(f"wrote {count} corpus files to {out}/")
PY