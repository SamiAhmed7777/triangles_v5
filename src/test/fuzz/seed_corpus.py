#!/usr/bin/env python3
"""
seed_corpus.py — Convert Triangles script test JSON fixtures into
libFuzzer corpus files for the script_fuzz harness.

Each fixture is `["<scriptSig>", "<scriptPubKey>", ...]` where each
script is a string in the same format the in-tree ParseScript()
understands (asm + hex + quoted-byte + decimal forms). We parse each
script into a raw CScript byte sequence (matching CScript's << operator
for opcodes and push-data) and write `[len_byte][script_bytes]` to
corpus/<name>-<idx>.bin so script_fuzz can replay it.

Why this matters: without a seeded corpus, the fuzzer starts from a
single empty input and only finds trivial inputs in the first hour. With
a seeded corpus of 200+ real Triangles script fixtures, it starts
exploring meaningful corners of the opcode dispatch immediately.

Usage:
    ./seed_corpus.py <fixture.json> <output_dir> [<prefix>]
    ./seed_corpus.py src/test/data/script_valid.json /tmp/corpus valid
    ./seed_corpus.py src/test/data/script_invalid.json /tmp/corpus invalid
"""
import json
import os
import sys

# Mirror of GetOpName() from src/script.cpp. Only the opcodes we expect
# to see in fixtures are listed — anything else raises ParseError.
OPCODES = {
    # Push values
    "OP_PUSHBYTES_0": 0x00, "OP_FALSE": 0x00, "OP_0": 0x00,
    "OP_PUSHBYTES_1": 0x01, "OP_TRUE": 0x51, "OP_1": 0x51,
    "OP_2": 0x52, "OP_3": 0x53, "OP_4": 0x54, "OP_5": 0x55,
    "OP_6": 0x56, "OP_7": 0x57, "OP_8": 0x58, "OP_9": 0x59,
    "OP_10": 0x5a, "OP_11": 0x5b, "OP_12": 0x5c, "OP_13": 0x5d,
    "OP_14": 0x5e, "OP_15": 0x5f, "OP_16": 0x60,
    # Control flow
    "OP_NOP": 0x61, "OP_VER": 0x62, "OP_IF": 0x63, "OP_NOTIF": 0x64,
    "OP_VERIF": 0x65, "OP_VERNOTIF": 0x66, "OP_ELSE": 0x67,
    "OP_ENDIF": 0x68, "OP_VERIFY": 0x69, "OP_RETURN": 0x6a,
    # Stack
    "OP_TOALTSTACK": 0x6b, "OP_FROMALTSTACK": 0x6c,
    "OP_2DROP": 0x6d, "OP_2DUP": 0x6e, "OP_3DUP": 0x6f,
    "OP_2OVER": 0x70, "OP_2ROT": 0x71, "OP_2SWAP": 0x72,
    "OP_IFDUP": 0x73, "OP_DEPTH": 0x74, "OP_DROP": 0x75,
    "OP_DUP": 0x76, "OP_NIP": 0x77, "OP_OVER": 0x78,
    "OP_PICK": 0x79, "OP_ROLL": 0x7a, "OP_ROT": 0x7b,
    "OP_SWAP": 0x7c, "OP_TUCK": 0x7d,
    # Splice / cat
    "OP_CAT": 0x7e, "OP_SUBSTR": 0x7f, "OP_LEFT": 0x80,
    "OP_RIGHT": 0x81, "OP_SIZE": 0x82,
    # Bitwise (disabled in Bitcoin, but defined in script.cpp)
    "OP_INVERT": 0x83, "OP_AND": 0x84, "OP_OR": 0x85, "OP_XOR": 0x86,
    "OP_EQUAL": 0x87, "OP_EQUALVERIFY": 0x88,
    "OP_RESERVED1": 0x89, "OP_RESERVED2": 0x8a,
    # Arithmetic
    "OP_1ADD": 0x8b, "OP_1SUB": 0x8c, "OP_2MUL": 0x8d, "OP_2DIV": 0x8e,
    "OP_NEGATE": 0x8f, "OP_ABS": 0x90, "OP_NOT": 0x91, "OP_0NOTEQUAL": 0x92,
    "OP_ADD": 0x93, "OP_SUB": 0x94, "OP_MUL": 0x95, "OP_DIV": 0x96,
    "OP_MOD": 0x97, "OP_LSHIFT": 0x98, "OP_RSHIFT": 0x99,
    "OP_BOOLAND": 0x9a, "OP_BOOLOR": 0x9b,
    "OP_NUMEQUAL": 0x9c, "OP_NUMEQUALVERIFY": 0x9d,
    "OP_NUMNOTEQUAL": 0x9e, "OP_LESSTHAN": 0x9f, "OP_GREATERTHAN": 0xa0,
    "OP_LESSTHANOREQUAL": 0xa1, "OP_GREATERTHANOREQUAL": 0xa2,
    "OP_MIN": 0xa3, "OP_MAX": 0xa4,
    "OP_WITHIN": 0xa5,
    # Crypto
    "OP_RIPEMD160": 0xa6, "OP_SHA1": 0xa7, "OP_SHA256": 0xa8,
    "OP_HASH160": 0xa9, "OP_HASH256": 0xaa,
    "OP_CODESEPARATOR": 0xab, "OP_CHECKSIG": 0xac, "OP_CHECKSIGVERIFY": 0xad,
    "OP_CHECKMULTISIG": 0xae, "OP_CHECKMULTISIGVERIFY": 0xaf,
    # Expansion
    "OP_NOP1": 0xb0, "OP_NOP2": 0xb1, "OP_NOP3": 0xb2, "OP_NOP4": 0xb3,
    "OP_NOP5": 0xb4, "OP_NOP6": 0xb5, "OP_NOP7": 0xb6, "OP_NOP8": 0xb7,
    "OP_NOP9": 0xb8, "OP_NOP10": 0xb9,
    # Locktime (in script.cpp but rarely used in script-tests)
    "OP_CHECKLOCKTIMEVERIFY": 0xb1, "OP_CHECKSEQUENCEVERIFY": 0xb2,
    # Multi-byte opcodes (only the tag in CScript; data follows)
    "OP_PUSHDATA1": 0x4c, "OP_PUSHDATA2": 0x4d, "OP_PUSHDATA4": 0x4e,
}

# Aliases: Triangles' GetOpName strips "OP_" prefix for convenience.
SHORT_ALIASES = {k[3:]: v for k, v in OPCODES.items() if k.startswith("OP_")}
OPCODES.update(SHORT_ALIASES)


def parse_hex(s):
    """Parse a hex string like '4b417a7a...' (no 0x prefix)."""
    if len(s) % 2:
        raise ValueError(f"hex string of odd length: {s!r}")
    return bytes.fromhex(s)


def parse_script_to_bytes(src):
    """
    Parse a Triangles script string into raw CScript bytes, matching
    the in-tree ParseScript() in src/test/script_tests.cpp.

    Supports:
      * Decimal integers → push as numeric (small ints use OP_1..OP_16)
      * 0x-prefixed hex → raw bytes inserted (NOT pushed)
      * Single-quoted strings → pushed as data
      * Opcode names (with or without OP_ prefix)

    The numeric push uses the CScript << operator semantics: small ints
    (1..16) become OP_1..OP_16; otherwise we use the minimum-length
    push encoding (OP_PUSHBYTES_N if 1..75 bytes, OP_PUSHDATA1/2/4 for
    larger). For simplicity we use OP_PUSHDATA4 with explicit length
    for any non-tiny number — fuzzer doesn't care about minimal encoding.
    """
    out = bytearray()
    for w in src.split():
        if w.startswith("0x") or w.startswith("0X"):
            out.extend(parse_hex(w[2:]))
        elif len(w) >= 2 and w.startswith("'") and w.endswith("'"):
            data = w[1:-1].encode("latin-1", errors="replace")
            push_data(out, data)
        elif w.startswith("-") and w[1:].isdigit() or w.isdigit():
            n = int(w)
            if 1 <= n <= 16:
                out.append(0x50 + n)
            elif n == 0:
                out.append(0x00)  # OP_FALSE / OP_0
            else:
                # Minimal-encoding push for non-tiny ints: use signed
                # minimal-data encoding matching Bitcoin's CScript::<<int>.
                # For simplicity here, encode as little-endian and push.
                if n < 0:
                    # Encode as signed (rare in fixtures; OP_1NEGATE for -1)
                    if n == -1:
                        out.append(0x4f)  # OP_1NEGATE
                        continue
                    data = (-n).to_bytes(((-n).bit_length() + 7) // 8, "little")
                    data = bytes([b | 0x80 for b in data])  # sign bit
                else:
                    data = n.to_bytes((n.bit_length() + 7) // 8, "little")
                push_data(out, data)
        elif w in OPCODES:
            out.append(OPCODES[w])
        else:
            # Unknown token — skip silently (mirrors old behavior of
            # not crashing on weird fixture entries). Production callers
            # should validate, but this is a seed generator, not a
            # verifier.
            pass
    return bytes(out)


def push_data(out, data):
    """Encode a push of `data` onto the script, using the same encoding
    EvalScript expects on the wire."""
    n = len(data)
    if n == 0:
        # OP_0 (push empty)
        out.append(0x00)
    elif n <= 0x4b:
        out.append(n)
        out.extend(data)
    elif n <= 0xff:
        out.append(0x4c)  # OP_PUSHDATA1
        out.append(n)
        out.extend(data)
    elif n <= 0xffff:
        out.append(0x4d)  # OP_PUSHDATA2
        out.extend(n.to_bytes(2, "little"))
        out.extend(data)
    else:
        out.append(0x4e)  # OP_PUSHDATA4
        out.extend(n.to_bytes(4, "little"))
        out.extend(data)


def fixture_to_corpus_files(fixture_path, output_dir, prefix):
    """Convert a JSON fixture into a directory of corpus files.

    Each inner fixture `["<sig>", "<pubkey>"]` becomes two files:
      <prefix>-<idx>-sig.bin     [len_byte][script_bytes]
      <prefix>-<idx>-pubkey.bin  [len_byte][script_bytes]
    EvalScript runs each side independently when fuzzer replays them.
    """
    os.makedirs(output_dir, exist_ok=True)
    with open(fixture_path) as f:
        fixtures = json.load(f)

    written = 0
    skipped = 0
    for idx, test in enumerate(fixtures):
        if not isinstance(test, list) or len(test) < 2:
            skipped += 1
            continue
        for side, name in [(test[0], "sig"), (test[1], "pubkey")]:
            if not isinstance(side, str):
                skipped += 1
                continue
            try:
                script_bytes = parse_script_to_bytes(side)
            except Exception as e:
                print(f"  [skip {prefix}-{idx}-{name}] {e}", file=sys.stderr)
                skipped += 1
                continue
            # Cap at 255 bytes so the 1-byte length prefix fits. The
            # fuzzer's libFuzzer has no problem with small inputs (it
            # tries small first by default), and 255 covers all real
            # Triangles scripts — anything bigger is either a script
            # with embedded sigs (which the fuzzer doesn't verify) or
            # pathological. We split oversized inputs across multiple
            # 255-byte chunks.
            if len(script_bytes) > 255:
                # Write as multiple short inputs — each sub-script.
                # This loses context but exercises the same byte
                # sequences the fuzzer would discover anyway.
                chunk = 0
                for start in range(0, len(script_bytes), 255):
                    sub = script_bytes[start:start + 255]
                    out_path = os.path.join(
                        output_dir,
                        f"{prefix}-{idx:04d}-{name}-c{chunk:02d}.bin"
                    )
                    with open(out_path, "wb") as f:
                        f.write(bytes([len(sub)]))
                        f.write(sub)
                    written += 1
                    chunk += 1
                continue
            out_path = os.path.join(output_dir, f"{prefix}-{idx:04d}-{name}.bin")
            with open(out_path, "wb") as f:
                # Format: [1-byte len][script bytes]
                f.write(bytes([len(script_bytes)]))
                f.write(script_bytes)
            written += 1
    print(f"  {prefix}: wrote {written} corpus files, skipped {skipped}", file=sys.stderr)


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(__doc__, file=sys.stderr)
        sys.exit(1)
    fixture_path = sys.argv[1]
    output_dir = sys.argv[2]
    prefix = sys.argv[3] if len(sys.argv) > 3 else os.path.basename(fixture_path).split(".")[0]
    fixture_to_corpus_files(fixture_path, output_dir, prefix)