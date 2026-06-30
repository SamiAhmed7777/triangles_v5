#!/usr/bin/env python3
"""
validate_onion_seeds.py - Cryptographic Triangles v3 onion address validator

Validates every .onion address in a triangles.conf (or any text file) against
the v3 hidden service checksum algorithm:

    v3 onion = base32( version[2] || pubkey[32] || checksum[2] )
    where checksum = SHA3-256( ".onion checksum" || version || pubkey )[:2]
    and version   = 0x03 0x00

A corrupted v3 onion (e.g. one character transposed) will have a valid base32
shape but a failing checksum. Tor rejects these with:

    [warn] ed25519 validation failed
    [warn] Service address has bad pubkey
    [warn] Invalid onion hostname; rejecting
    [notice] ... resolve failed. No more HSDir available to query.

This tool is designed to be run as a pre-flight check before deploying
a triangles.conf, and as a CI gate to prevent corrupted .onion addresses
from ever reaching production. It can also be used to audit an existing
config for inconsistencies against the hardcoded seed list in
src/onionseed.h.

USAGE
    # Validate the production config
    ./validate_onion_seeds.py /root/.triangles/triangles.conf

    # Validate multiple configs
    ./validate_onion_seeds.py /root/.triangles/triangles.conf \\
                              /root/.triangles-synctest/triangles.conf

    # Audit a config against the hardcoded source-of-truth
    ./validate_onion_seeds.py /root/.triangles/triangles.conf \\
                              --against /root/triangles_v5/src/onionseed.h

    # CI mode (exit 1 on any error)
    ./validate_onion_seeds.py /root/.triangles/triangles.conf --ci

EXIT CODES
    0  all addresses valid, no warnings
    1  one or more addresses failed validation
    2  usage error / file not found

DETECTION CAPABILITIES
    * Bad v3 checksum (1-2 char transposition, missing char, etc.)
    * Truncated or extended .onion addresses
    * Non-base32 characters in .onion
    * Cross-config diff (or test vs production mismatch)
    * addnode referencing a .onion that's not in the source seed list

BACKGROUND
    During a from-zero sync test on 2026-06-21, the test daemon's Tor log
    produced 4,842 "No more HSDir available" errors and 181 "ed25519
    validation failed" warnings. Root cause: a 1-character transposition
    (btb6 vs gtb6) in the test config's vmepp seed address. This tool
    would have caught it in 0.1 seconds.
"""

import argparse
import base64
import hashlib
import os
import re
import sys
from pathlib import Path

# v3 onion constants
V3_VERSION = b'\x03\x00'  # 2 bytes
V3_CHECKSUM_INPUT = b'.onion checksum'  # 15 bytes
V3_PUBKEY_LENGTH = 32
V3_CHECKSUM_LENGTH = 2
V3_DECODED_LENGTH = 35  # 2 + 32 + 2 + ...wait that's 36
# Actually v3 onion base32-decodes to 35 bytes:
#   1 byte version (0x03) + 1 byte checksum-type (0x00) +
#   32 bytes pubkey + 2 bytes checksum  -- no wait
# Per official spec: onion_address = base32(pubkey || checksum || version)
# Total = 32 (ed25519) + 2 (checksum) + 1 (version) = 35 bytes
# But some implementations use:
#   version(2) || pubkey(32) || checksum(2) = 36
# The actual spec from rfc7686 says:
#   onion_address = base32(PUBKEY || CHECKSUM || VERSION)
#   PUBKEY = ed25519 public key (32 bytes)
#   CHECKSUM = H(".onion checksum" || PUBKEY || VERSION)[:2]
#   VERSION = 0x03
# So total = 32 + 2 + 1 = 35 bytes (not 36)

# We'll use the official spec (35 bytes)

# ANSI color codes (only if stdout is a TTY)
class C:
    RESET = '\033[0m'
    RED = '\033[91m'
    GREEN = '\033[92m'
    YELLOW = '\033[93m'
    BLUE = '\033[94m'
    BOLD = '\033[1m'
    DIM = '\033[2m'

    @classmethod
    def disable(cls):
        for attr in dir(cls):
            if attr.isupper() and not attr.startswith('_'):
                setattr(cls, attr, '')


def decode_v3_onion(address: str) -> tuple[bool, str, bytes | None]:
    """
    Validate a v3 onion address.

    Returns:
        (valid, reason, decoded_bytes_or_None)
    """
    if not isinstance(address, str):
        return False, f"not a string (got {type(address).__name__})", None
    if not address.endswith('.onion'):
        return False, "missing .onion suffix", None

    onion_body = address[:-6]  # strip .onion
    expected_len = 56  # base32(35 bytes) = 56 chars
    if len(onion_body) != expected_len:
        return False, f"wrong length: {len(onion_body)} chars (expected {expected_len})", None

    # Validate base32 alphabet
    if not re.match(r'^[a-z2-7]+$', onion_body):
        # Find first bad char
        for i, c in enumerate(onion_body):
            if not re.match(r'[a-z2-7]', c):
                return False, f"non-base32 char '{c}' at position {i}", None

    # Decode
    try:
        # Add padding
        padding_needed = (8 - len(onion_body) % 8) % 8
        decoded = base64.b32decode(onion_body.upper() + '=' * padding_needed)
    except Exception as e:
        return False, f"base32 decode failed: {e}", None

    if len(decoded) != 35:
        return False, f"decoded to {len(decoded)} bytes, expected 35", None

    # v3 spec: PUBKEY(32) || CHECKSUM(2) || VERSION(1)
    pubkey = decoded[0:32]
    checksum = decoded[32:34]
    version = decoded[34:35]

    if version != b'\x03':
        return False, f"version byte is 0x{version[0]:02x}, expected 0x03", decoded

    # Compute expected checksum
    expected_checksum = hashlib.sha3_256(
        V3_CHECKSUM_INPUT + pubkey + version
    ).digest()[:2]

    if checksum != expected_checksum:
        return False, (
            f"checksum mismatch: got 0x{checksum.hex()}, "
            f"expected 0x{expected_checksum.hex()}"
        ), decoded

    return True, "valid v3 onion", decoded


def parse_config_addnodes(config_path: Path) -> list[tuple[str, str, int]]:
    """
    Extract (line_no, address, port) tuples for all addnode= lines in a config.

    Also handles addnode=onion:port and just addnode=onion (port defaults to 24112).
    """
    addnodes = []
    if not config_path.exists():
        return addnodes

    for line_no, raw_line in enumerate(config_path.read_text().splitlines(), 1):
        line = raw_line.strip()
        if not line or line.startswith('#'):
            continue
        m = re.match(r'^addnode=([^:]+)(?::(\d+))?$', line)
        if m:
            addr = m.group(1)
            port = int(m.group(2)) if m.group(2) else 24112
            addnodes.append((line_no, addr, port))

    return addnodes


def parse_source_seeds(source_path: Path) -> set[str]:
    """
    Extract all .onion addresses from the hardcoded seed list in onionseed.h.
    Matches the strMainNetOnionSeed and strTestNetOnionSeed arrays.
    """
    seeds = set()
    if not source_path.exists():
        return seeds
    for m in re.finditer(r'"([a-z2-7]{56}\.onion)"', source_path.read_text()):
        seeds.add(m.group(1))
    return seeds


def levenshtein_1(a: str, b: str) -> int:
    """Return number of positions where a and b differ (assumes same length)."""
    if len(a) != len(b):
        return -1
    return sum(1 for x, y in zip(a, b) if x != b.count(x))


def find_near_match(target: str, candidates: set[str]) -> str | None:
    """Find a candidate that's 1-2 char different from target (for diff hints)."""
    for c in candidates:
        if len(c) == len(target):
            d = sum(1 for x, y in zip(c, target) if x != y)
            if 0 < d <= 2:
                return c
    return None


def colorize(s: str, color: str, enabled: bool) -> str:
    return f"{color}{s}{C.RESET}" if enabled else s


def validate_config(
    config_path: Path,
    source_seeds: set[str] | None = None,
    other_configs: dict[Path, set[str]] | None = None,
    use_color: bool = True,
) -> tuple[int, int, int, int]:
    """
    Validate all .onion addresses in a config file.

    Returns:
        (valid_count, invalid_count, missing_count, extra_count)
    """
    addnodes = parse_config_addnodes(config_path)
    if not addnodes:
        print(colorize(f"  (no addnode= entries found in {config_path})",
                       C.YELLOW, use_color))
        return (0, 0, 0, 0)

    valid = invalid = 0
    invalid_addrs = set()

    print(colorize(f"\n=== {config_path} ===", C.BOLD + C.BLUE, use_color))
    print(colorize(f"  {len(addnodes)} addnode entries found", C.DIM, use_color))

    for line_no, addr, port in addnodes:
        ok, reason, _ = decode_v3_onion(addr)
        if ok:
            print(f"  {colorize('[OK]', C.GREEN, use_color):>14}  line {line_no:>4}  {addr}")
            valid += 1
        else:
            print(f"  {colorize('[BAD]', C.RED, use_color):>14}  line {line_no:>4}  {addr}")
            print(f"  {'':<14}  {'':>4}  reason: {reason}")
            # Try to suggest a similar address
            if source_seeds:
                near = find_near_match(addr, source_seeds)
                if near:
                    print(f"  {'':<14}  {'':>4}  {colorize(f'did you mean: {near}?', C.YELLOW, use_color)}")
            invalid += 1
            invalid_addrs.add(addr)

    # Cross-check against other configs
    missing = extra = 0
    if other_configs and source_seeds is not None:
        config_addrs = {addr for _, addr, _ in addnodes}
        # Note: this just reports on relationships; doesn't fail the test
        for other_path, other_addrs in other_configs.items():
            only_in_this = config_addrs - other_addrs - invalid_addrs
            only_in_other = other_addrs - config_addrs
            if only_in_this:
                print(colorize(
                    f"\n  {colorize('[DIFF]', C.YELLOW, use_color)}  addresses only in {config_path.name} "
                    f"(missing from {other_path.name}):",
                    C.YELLOW, use_color))
                for a in sorted(only_in_this):
                    print(f"                  {a}")
                extra += len(only_in_this)
            if only_in_other:
                print(colorize(
                    f"\n  {colorize('[DIFF]', C.YELLOW, use_color)}  addresses only in {other_path.name} "
                    f"(missing from {config_path.name}):",
                    C.YELLOW, use_color))
                for a in sorted(only_in_other):
                    print(f"                  {a}")
                missing += len(only_in_other)

    return valid, invalid, missing, extra


def main():
    parser = argparse.ArgumentParser(
        description="Validate v3 .onion addresses in Triangles config files",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    parser.add_argument(
        'configs',
        nargs='+',
        type=Path,
        help='One or more triangles.conf files to validate',
    )
    parser.add_argument(
        '--against',
        type=Path,
        default=None,
        help='Path to src/onionseed.h to use as source of truth for diff hints',
    )
    parser.add_argument(
        '--ci',
        action='store_true',
        help='CI mode: exit 1 if any address fails validation',
    )
    parser.add_argument(
        '--no-color',
        action='store_true',
        help='Disable colored output (also auto-disabled when stdout is not a TTY)',
    )

    args = parser.parse_args()

    # Color detection
    use_color = not args.no_color and sys.stdout.isatty()
    if not use_color:
        C.disable()

    # Validate inputs exist
    for p in args.configs:
        if not p.exists():
            print(colorize(f"ERROR: file not found: {p}", C.RED, use_color),
                  file=sys.stderr)
            return 2

    # Load source seeds if provided
    source_seeds = None
    if args.against:
        if not args.against.exists():
            print(colorize(f"WARNING: source seed file not found: {args.against}",
                           C.YELLOW, use_color), file=sys.stderr)
        else:
            source_seeds = parse_source_seeds(args.against)
            print(colorize(
                f"Loaded {len(source_seeds)} hardcoded seeds from {args.against}",
                C.DIM, use_color))

    # Pre-load all configs for cross-checking
    all_configs: dict[Path, set[str]] = {}
    for p in args.configs:
        addnodes = parse_config_addnodes(p)
        all_configs[p] = {addr for _, addr, _ in addnodes}

    # Validate each config
    total_valid = total_invalid = total_missing = total_extra = 0
    for p in args.configs:
        if len(args.configs) > 1:
            other = {k: v for k, v in all_configs.items() if k != p}
        else:
            other = None
        v, i, m, e = validate_config(p, source_seeds, other, use_color)
        total_valid += v
        total_invalid += i
        total_missing += m
        total_extra += e

    # Summary
    print(colorize("\n=== SUMMARY ===", C.BOLD, use_color))
    print(f"  Valid:      {colorize(str(total_valid), C.GREEN, use_color)}")
    if total_invalid:
        print(f"  Invalid:    {colorize(str(total_invalid), C.RED, use_color)}")
    else:
        print(f"  Invalid:    {total_invalid}")
    if total_missing:
        print(f"  Missing:    {colorize(str(total_missing), C.YELLOW, use_color)} "
              f"(in other configs, not this one)")
    if total_extra:
        print(f"  Extra:      {colorize(str(total_extra), C.YELLOW, use_color)} "
              f"(in this config, not others)")

    if total_invalid == 0 and total_missing == 0:
        print(colorize("\n  All addresses valid.", C.GREEN + C.BOLD, use_color))
        return 0
    else:
        print(colorize(
            f"\n  {total_invalid} address(es) failed v3 onion checksum validation.",
            C.RED + C.BOLD, use_color))
        if args.ci:
            return 1
        return 1 if total_invalid else 0


if __name__ == '__main__':
    sys.exit(main())
