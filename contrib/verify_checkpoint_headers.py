#!/usr/bin/env python3
"""Query Triangles headers through Tor without trusting a local chain database."""

import argparse
import hashlib
import json
import os
import socket
import struct
import sys
import time


MAGIC = bytes.fromhex("70352205")
PROTOCOL_VERSION = 70206
DEFAULT_PORT = 24112


def read_exact(sock, length):
    chunks = []
    remaining = length
    while remaining:
        chunk = sock.recv(remaining)
        if not chunk:
            raise ConnectionError("peer closed the connection")
        chunks.append(chunk)
        remaining -= len(chunk)
    return b"".join(chunks)


def compact_size(value):
    if value < 253:
        return bytes([value])
    if value <= 0xFFFF:
        return b"\xfd" + struct.pack("<H", value)
    if value <= 0xFFFFFFFF:
        return b"\xfe" + struct.pack("<I", value)
    return b"\xff" + struct.pack("<Q", value)


def read_compact_size(payload, offset):
    marker = payload[offset]
    offset += 1
    if marker < 253:
        return marker, offset
    if marker == 253:
        return struct.unpack_from("<H", payload, offset)[0], offset + 2
    if marker == 254:
        return struct.unpack_from("<I", payload, offset)[0], offset + 4
    return struct.unpack_from("<Q", payload, offset)[0], offset + 8


def message(command, payload=b""):
    checksum = hashlib.sha256(hashlib.sha256(payload).digest()).digest()[:4]
    command_bytes = command.encode("ascii").ljust(12, b"\0")
    return MAGIC + command_bytes + struct.pack("<I", len(payload)) + checksum + payload


def network_address(port):
    # This fork extends CService with a Tor-v3 key and Tor/I2P discriminator bits.
    return (
        struct.pack("<Q", 1)
        + (b"\0" * 16)
        + (b"\0" * 32)
        + b"\0\0"
        + struct.pack(">H", port)
    )


def version_payload(port):
    user_agent = b"/triangles-checkpoint-audit:0.1/"
    return (
        struct.pack("<iQq", PROTOCOL_VERSION, 1, int(time.time()))
        + network_address(port)
        + network_address(0)
        + os.urandom(8)
        + compact_size(len(user_agent))
        + user_agent
        + struct.pack("<i", 0)
    )


def connect_via_socks(socks_host, socks_port, peer, peer_port, timeout):
    sock = socket.create_connection((socks_host, socks_port), timeout=timeout)
    sock.settimeout(timeout)
    sock.sendall(b"\x05\x01\x00")
    if read_exact(sock, 2) != b"\x05\x00":
        raise ConnectionError("SOCKS proxy refused no-auth negotiation")

    peer_bytes = peer.encode("ascii")
    request = b"\x05\x01\x00\x03" + bytes([len(peer_bytes)]) + peer_bytes
    request += struct.pack(">H", peer_port)
    sock.sendall(request)

    prefix = read_exact(sock, 4)
    if prefix[1] != 0:
        raise ConnectionError(f"SOCKS connect failed with status {prefix[1]}")
    address_type = prefix[3]
    if address_type == 1:
        read_exact(sock, 4)
    elif address_type == 3:
        read_exact(sock, read_exact(sock, 1)[0])
    elif address_type == 4:
        read_exact(sock, 16)
    else:
        raise ConnectionError(f"SOCKS proxy returned address type {address_type}")
    read_exact(sock, 2)
    return sock


def read_message(sock):
    header = read_exact(sock, 24)
    if header[:4] != MAGIC:
        raise ValueError(f"unexpected network magic {header[:4].hex()}")
    command = header[4:16].rstrip(b"\0").decode("ascii")
    length = struct.unpack_from("<I", header, 16)[0]
    if length > 32 * 1024 * 1024:
        raise ValueError(f"oversized {command} message: {length}")
    payload = read_exact(sock, length)
    expected = hashlib.sha256(hashlib.sha256(payload).digest()).digest()[:4]
    if header[20:24] != expected:
        raise ValueError(f"invalid checksum on {command}")
    return command, payload


def parse_headers(payload, start_height):
    count, offset = read_compact_size(payload, 0)
    headers = []
    for index in range(count):
        if offset + 80 > len(payload):
            raise ValueError("truncated block header")
        raw_header = payload[offset : offset + 80]
        offset += 80
        tx_count, offset = read_compact_size(payload, offset)
        signature_size, offset = read_compact_size(payload, offset)
        if tx_count != 0 or signature_size != 0:
            raise ValueError("headers response contains transactions or a block signature")
        headers.append(
            {
                "height": start_height + index,
                "previous": raw_header[4:36][::-1].hex(),
                "raw": raw_header.hex(),
            }
        )
    if offset != len(payload):
        raise ValueError(f"unexpected trailing header bytes: {len(payload) - offset}")
    return headers


def query_peer(args, peer):
    sock = connect_via_socks(
        args.socks_host, args.socks_port, peer, args.peer_port, args.timeout
    )
    try:
        sock.sendall(message("version", version_payload(args.peer_port)))
        got_version = False
        got_verack = False
        peer_version = None
        deadline = time.monotonic() + args.timeout
        while time.monotonic() < deadline and not (got_version and got_verack):
            command, payload = read_message(sock)
            if command == "version":
                peer_version = struct.unpack_from("<i", payload, 0)[0]
                got_version = True
                sock.sendall(message("verack"))
            elif command == "verack":
                got_verack = True
            elif command == "ping":
                sock.sendall(message("pong", payload))
        if not (got_version and got_verack):
            raise TimeoutError("version handshake did not complete")

        locator_hash = bytes.fromhex(args.checkpoint_hash)[::-1]
        payload = struct.pack("<i", PROTOCOL_VERSION)
        payload += compact_size(1) + locator_hash + (b"\0" * 32)
        sock.sendall(message("getheaders", payload))

        while time.monotonic() < deadline:
            command, payload = read_message(sock)
            if command == "ping":
                sock.sendall(message("pong", payload))
            elif command == "headers":
                headers = parse_headers(payload, args.checkpoint_height + 1)
                target = next(
                    (
                        item["previous"]
                        for item in headers
                        if item["height"] == args.snapshot_height + 1
                    ),
                    None,
                )
                through_snapshot = [
                    item for item in headers if item["height"] <= args.snapshot_height
                ]
                snapshot_header = next(
                    (
                        item["raw"]
                        for item in headers
                        if item["height"] == args.snapshot_height
                    ),
                    None,
                )
                return {
                    "peer": peer,
                    "peer_version": peer_version,
                    "header_count": len(headers),
                    "first_height": headers[0]["height"] if headers else None,
                    "last_height": headers[-1]["height"] if headers else None,
                    "checkpoint_anchor_matches": bool(headers)
                    and headers[0]["previous"] == args.checkpoint_hash.lower(),
                    "snapshot_height": args.snapshot_height,
                    "snapshot_block_hash": target,
                    "snapshot_header_sha256": (
                        hashlib.sha256(bytes.fromhex(snapshot_header)).hexdigest()
                        if snapshot_header
                        else None
                    ),
                    "headers_through_snapshot_sha256": hashlib.sha256(
                        b"".join(
                            bytes.fromhex(item["raw"]) for item in through_snapshot
                        )
                    ).hexdigest(),
                    "headers_sha256": hashlib.sha256(
                        b"".join(bytes.fromhex(item["raw"]) for item in headers)
                    ).hexdigest(),
                }
        raise TimeoutError("peer did not return headers")
    finally:
        sock.close()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("peers", nargs="+")
    parser.add_argument("--socks-host", default="127.0.0.1")
    parser.add_argument("--socks-port", type=int, default=19099)
    parser.add_argument("--peer-port", type=int, default=DEFAULT_PORT)
    parser.add_argument("--checkpoint-height", type=int, required=True)
    parser.add_argument("--checkpoint-hash", required=True)
    parser.add_argument("--snapshot-height", type=int, required=True)
    parser.add_argument("--expected-snapshot-block-hash")
    parser.add_argument("--timeout", type=int, default=45)
    args = parser.parse_args()

    results = []
    for peer in args.peers:
        try:
            results.append(query_peer(args, peer))
        except Exception as exc:  # Keep querying independent peers after a failure.
            results.append({"peer": peer, "error": str(exc)})
    print(json.dumps(results, indent=2, sort_keys=True))

    if any("error" in result for result in results):
        return 1
    if any(not result["checkpoint_anchor_matches"] for result in results):
        return 2
    if len({result["headers_through_snapshot_sha256"] for result in results}) != 1:
        return 3

    reported_hashes = {
        result["snapshot_block_hash"]
        for result in results
        if result["snapshot_block_hash"] is not None
    }
    if len(reported_hashes) > 1:
        return 4
    if args.expected_snapshot_block_hash:
        expected = args.expected_snapshot_block_hash.lower()
        if reported_hashes != {expected}:
            return 5
    return 0


if __name__ == "__main__":
    sys.exit(main())
