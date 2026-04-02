#!/bin/bash
# Triangles Dynamic Seed Collector
# Run via cron on a VPS that runs a Triangles node.
# Queries the local node's getseedlist RPC for known .onion peers
# and writes them to a static file served by a web server.
#
# Example cron (every 5 minutes):
#   */5 * * * * /path/to/collect-seeds.sh
#
# The web server (Caddy, nginx, etc.) serves the output file at:
#   http://seeds.cryptographic-triangles.org/seeds.txt

# Configuration
RPC_USER="${RPC_USER:-trianglesrpc}"
RPC_PASSWORD="${RPC_PASSWORD:-}"
RPC_PORT="${RPC_PORT:-19112}"
OUTPUT_FILE="${OUTPUT_FILE:-/var/www/seeds/seeds.txt}"

if [ -z "$RPC_PASSWORD" ]; then
    echo "Error: RPC_PASSWORD not set" >&2
    exit 1
fi

# Query the node for known onion seeds
RESPONSE=$(curl -s --user "${RPC_USER}:${RPC_PASSWORD}" \
    --data-binary '{"jsonrpc":"1.0","id":"seedcollect","method":"getseedlist","params":[]}' \
    -H 'content-type: text/plain;' \
    "http://127.0.0.1:${RPC_PORT}/" 2>/dev/null)

if [ $? -ne 0 ] || [ -z "$RESPONSE" ]; then
    echo "Error: RPC call failed" >&2
    exit 1
fi

# Extract addresses and write to temp file, then atomically move
TMPFILE=$(mktemp)

echo "# Triangles seed nodes - auto-generated $(date -u +%Y-%m-%dT%H:%M:%SZ)" > "$TMPFILE"
echo "$RESPONSE" | jq -r '.result[] | .address + ":" + (.port|tostring)' >> "$TMPFILE" 2>/dev/null

SEED_COUNT=$(grep -c '.onion' "$TMPFILE" 2>/dev/null || echo 0)

if [ "$SEED_COUNT" -gt 0 ]; then
    mv "$TMPFILE" "$OUTPUT_FILE"
    echo "Updated ${OUTPUT_FILE} with ${SEED_COUNT} seeds"
else
    rm -f "$TMPFILE"
    echo "Warning: no onion seeds found, keeping previous file" >&2
fi
