#!/usr/bin/env python3
"""
Scan a blk0001.dat file to find all blocks and determine the chain tip.
Reads block headers to extract height from the serialized block data.
"""

import struct
import hashlib
import sys
import os

# Triangles mainnet magic bytes (from main.cpp pchMessageStart)
MAGIC = b'\x70\x35\x22\x05'

def double_sha256(data):
    return hashlib.sha256(hashlib.sha256(data).digest()).digest()

def read_varint(f):
    """Read a Bitcoin-style variable-length integer."""
    n = struct.unpack('<B', f.read(1))[0]
    if n < 0xFD:
        return n
    elif n == 0xFD:
        return struct.unpack('<H', f.read(2))[0]
    elif n == 0xFE:
        return struct.unpack('<I', f.read(4))[0]
    else:
        return struct.unpack('<Q', f.read(8))[0]

def scan_blocks(filepath):
    """Scan blk file and collect block headers with their file positions."""
    filesize = os.path.getsize(filepath)
    print(f"File size: {filesize:,} bytes ({filesize/1024/1024:.1f} MB)")

    blocks = {}  # hash -> (prev_hash, position, block_time)
    block_count = 0

    with open(filepath, 'rb') as f:
        while True:
            pos = f.tell()

            # Progress
            if block_count % 50000 == 0:
                pct = pos / filesize * 100
                print(f"  Scanned {block_count:,} blocks ({pct:.1f}% of file)...", flush=True)

            # Read magic bytes
            magic = f.read(4)
            if len(magic) < 4:
                break

            if magic != MAGIC:
                # Try to find next magic bytes
                f.seek(pos + 1)
                continue

            # Read block size
            size_data = f.read(4)
            if len(size_data) < 4:
                break
            block_size = struct.unpack('<I', size_data)[0]

            if block_size == 0 or block_size > 4000000:
                f.seek(pos + 1)
                continue

            # Read block header (80 bytes)
            header_data = f.read(80)
            if len(header_data) < 80:
                break

            # Parse header
            version = struct.unpack('<I', header_data[0:4])[0]
            prev_hash = header_data[4:36][::-1].hex()
            merkle_root = header_data[36:68][::-1].hex()
            block_time = struct.unpack('<I', header_data[68:72])[0]
            bits = struct.unpack('<I', header_data[72:76])[0]
            nonce = struct.unpack('<I', header_data[76:80])[0]

            # Calculate block hash
            block_hash = double_sha256(header_data)[::-1].hex()

            blocks[block_hash] = (prev_hash, pos, block_time, version)
            block_count += 1

            # Skip rest of block
            remaining = block_size - 80
            if remaining > 0:
                f.seek(remaining, 1)

    print(f"\nTotal blocks found in file: {block_count:,}")
    return blocks

def build_chain(blocks):
    """Build the chain from genesis and find the tip."""
    # Find genesis block (prev_hash is all zeros)
    genesis_hash = None
    for bhash, (prev_hash, pos, btime, version) in blocks.items():
        if prev_hash == '0' * 64:
            genesis_hash = bhash
            break

    if not genesis_hash:
        print("ERROR: Genesis block not found!")
        return

    print(f"Genesis block: {genesis_hash}")

    # Build forward chain
    # Create reverse lookup: prev_hash -> next_hash
    children = {}
    for bhash, (prev_hash, pos, btime, version) in blocks.items():
        if prev_hash not in children:
            children[prev_hash] = []
        children[prev_hash].append(bhash)

    # Walk from genesis
    current = genesis_hash
    height = 0
    last_hashes = []

    while current in children:
        nexts = children[current]
        if len(nexts) == 1:
            current = nexts[0]
        else:
            # Multiple children (fork) - pick the one that leads to longest chain
            # For simplicity, just pick the first one
            current = nexts[0]
        height += 1

        if height % 100000 == 0:
            prev_hash, pos, btime, version = blocks[current]
            import datetime
            dt = datetime.datetime.fromtimestamp(btime)
            print(f"  Height {height:,}: {current[:16]}... time={dt}")

        # Keep last 10 blocks for reference
        if height > 0:
            last_hashes.append((height, current))
            if len(last_hashes) > 20:
                last_hashes.pop(0)

    print(f"\n{'='*60}")
    print(f"CHAIN TIP:")
    print(f"  Height: {height:,}")
    print(f"  Hash:   0x{current}")
    prev_hash, pos, btime, version = blocks[current]
    import datetime
    dt = datetime.datetime.fromtimestamp(btime)
    print(f"  Time:   {dt}")
    print(f"  Version: {version}")
    print(f"{'='*60}")

    print(f"\nLast 20 blocks:")
    for h, bhash in last_hashes:
        prev_hash, pos, btime, version = blocks[bhash]
        dt = datetime.datetime.fromtimestamp(btime)
        print(f"  {h:>10,}: 0x{bhash[:40]}... ({dt})")

if __name__ == '__main__':
    filepath = sys.argv[1] if len(sys.argv) > 1 else r'E:\Coins\TRIagain\blk0001.dat'
    print(f"Scanning: {filepath}")
    blocks = scan_blocks(filepath)
    if blocks:
        build_chain(blocks)
