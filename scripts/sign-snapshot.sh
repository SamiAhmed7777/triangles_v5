#!/usr/bin/env bash
# ============================================================================
# Triangles UTXO Snapshot Signer
# ============================================================================
# Generates a UTXO snapshot from the current node, signs its provenance
# message with the wallet's signing address, and writes the signed manifest.
#
# Usage:
#   ./sign-snapshot.sh [snapshot-name]
#
# Default snapshot name: tri-utxo-snapshot-<timestamp>.utx
# Output (in this dir):
#   <snapshot-name>              - the UTXO snapshot binary
#   <snapshot-name>.sig          - base64 signature
#   <snapshot-name>.msg          - signed message (human-readable provenance)
#   <snapshot-name>.manifest.json - signed manifest (drop into bootstrap dir)
#   <snapshot-name>.pubkey       - signing address
#
# Requirements:
#   - trianglesd running with RPC enabled
#   - wallet unlocked (or passphrase set in triangles.conf)
#   - jq installed (apt: jq / brew: jq)
#
# Verification:
#   ./sign-snapshot.sh verify <manifest.json> <snapshot-file>
#   OR via RPC:
#     verifymessage <addr> <sig> <msg>
# ============================================================================

set -euo pipefail

# ----- Config (override via env) -----
RPC_USER="${RPC_USER:-trianglesrpc}"
RPC_PASS="${RPC_PASS:-2KVK2FvLZBW9Hxv4a2Uj3dMRDAXdh4ei6S5tdZ3z2Mme}"
RPC_HOST="${RPC_HOST:-127.0.0.1}"
RPC_PORT="${RPC_PORT:-19112}"
SIGN_ACCOUNT="${SIGN_ACCOUNT:-}"      # blank = use default account
NHEADERS="${NHEADERS:-2000}"
SNAP_DIR="${SNAP_DIR:-.}"

# ----- Helpers -----
rpc() {
    local method="$1"; shift
    local params="$1"; shift || true
    curl -s --user "${RPC_USER}:${RPC_PASS}" \
         -X POST -H 'Content-Type: application/json' \
         --data "{\"jsonrpc\":\"1.0\",\"method\":\"${method}\",\"params\":${params}}" \
         "http://${RPC_HOST}:${RPC_PORT}/"
}

rpc_field() {
    local method="$1"; shift
    local params="$1"; shift || true
    local field="$1"; shift
    rpc "$method" "$params" | jq -r ".result.${field} // empty"
}

sha256_file() { sha256sum "$1" | awk '{print $1}'; }

# ----- Verify mode -----
if [[ "${1:-}" == "verify" ]]; then
    MANIFEST="${2:?usage: $0 verify <manifest.json> <snapshot-file>}"
    SNAP="${3:?usage: $0 verify <manifest.json> <snapshot-file>}"
    ADDR=$(jq -r '.signing_address' "$MANIFEST")
    SIG=$(jq -r '.signature' "$MANIFEST")
    MSG=$(jq -r '.message' "$MANIFEST")
    EXPECTED_SHA=$(jq -r '.snapshot_sha256' "$MANIFEST")

    echo "==> Verifying snapshot provenance..."
    echo "    Address:  $ADDR"
    echo "    Message:  $MSG"

    ACTUAL_SHA=$(sha256_file "$SNAP")
    if [[ "$ACTUAL_SHA" != "$EXPECTED_SHA" ]]; then
        echo "FAIL: snapshot sha256 mismatch"
        echo "  expected: $EXPECTED_SHA"
        echo "  actual:   $ACTUAL_SHA"
        exit 1
    fi
    echo "OK: sha256 matches"

    PARAMS=$(jq -nc --arg a "$ADDR" --arg s "$SIG" --arg m "$MSG" \
                   '[$a, $s, $m]')
    RESULT=$(rpc verifymessage "$PARAMS" | jq -r '.result')
    if [[ "$RESULT" == "true" ]]; then
        echo "OK: signature valid — snapshot was signed by $ADDR"
        exit 0
    else
        echo "FAIL: signature does not verify"
        exit 1
    fi
fi

# ----- Generate + sign -----
SNAP_NAME="${1:-tri-utxo-snapshot-$(date -u +%Y%m%dT%H%M%SZ).utx}"
SNAP_PATH="${SNAP_DIR}/${SNAP_NAME}"

echo "==> Step 1/5: querying chain state..."
HEIGHT=$(rpc_field getblockcount '[]' '' || echo "")
if [[ -z "$HEIGHT" ]]; then
    rpc_field getblockcount '[]' ''  # re-run for error visibility
    echo "FAIL: RPC getblockcount failed"; exit 1
fi
HEIGHT=$(rpc getblockcount '[]' | jq -r '.result')
BLOCKHASH=$(rpc getbestblockhash '[]' | jq -r '.result')
echo "    height:   $HEIGHT"
echo "    blockhash:$BLOCKHASH"

echo "==> Step 2/5: selecting signing address..."
if [[ -n "$SIGN_ACCOUNT" ]]; then
    PARAMS=$(jq -nc --arg a "$SIGN_ACCOUNT" '[$a]')
else
    PARAMS='[""]'
fi
ADDR=$(rpc getaccountaddress "$PARAMS" | jq -r '.result')
echo "    signer:   $ADDR"

echo "==> Step 3/5: dumping UTXO snapshot..."
PARAMS=$(jq -nc --arg f "$SNAP_PATH" --argjson n "$NHEADERS" '[$f, $n]')
DUMP_RESULT=$(rpc dumputxoset "$PARAMS")
echo "$DUMP_RESULT" | jq -r '.result // .error.message // .'
SIZE=$(echo "$DUMP_RESULT" | jq -r '.result.file_size // empty')
if [[ -z "$SIZE" ]]; then
    echo "FAIL: dumputxoset failed"; exit 1
fi
echo "    size:     $SIZE bytes"

echo "==> Step 4/5: signing provenance message..."
SHA=$(sha256_file "$SNAP_PATH")
MSG="Triangles UTXO Snapshot $(date -u +%Y-%m-%d): height=$HEIGHT hash=$BLOCKHASH sha256=$SHA"
echo "    message:  $MSG"
PARAMS=$(jq -nc --arg a "$ADDR" --arg m "$MSG" '[$a, $m]')
SIG=$(rpc signmessage "$PARAMS" | jq -r '.result')
echo "    sig:      $SIG"

echo "==> Step 5/5: writing manifest + sidecars..."
MANIFEST_PATH="${SNAP_PATH}.manifest.json"
jq -n \
   --arg name "$SNAP_NAME" \
   --arg height "$HEIGHT" \
   --arg hash "$BLOCKHASH" \
   --arg sha "$SHA" \
   --arg size "$SIZE" \
   --arg msg "$MSG" \
   --arg sig "$SIG" \
   --arg addr "$ADDR" \
   --arg ts "$(date -u +%Y-%m-%dT%H:%M:%SZ)" \
   --arg ver "$(rpc getnetworkinfo '[]' | jq -r '.result.version // "unknown"')" \
   '{
        schema: "triangles-utxo-snapshot-signed/v1",
        name: $name,
        generated_utc: $ts,
        daemon_version: $ver,
        chain_tip: { height: ($height | tonumber), blockhash: $hash },
        snapshot_sha256: $sha,
        snapshot_bytes: ($size | tonumber),
        signing_address: $addr,
        message: $msg,
        signature: $sig
    }' > "$MANIFEST_PATH"

# Sidecar files for easy reading
echo "$ADDR"  > "${SNAP_PATH}.pubkey"
echo "$MSG"   > "${SNAP_PATH}.msg"
echo "$SIG"   > "${SNAP_PATH}.sig"

echo ""
echo "============================================================"
echo "Snapshot signed."
echo "  snapshot:      $SNAP_PATH"
echo "  signature:     ${SNAP_PATH}.sig"
echo "  manifest:      $MANIFEST_PATH"
echo "  signer:        $ADDR"
echo "  sha256:        $SHA"
echo "============================================================"
echo ""
echo "To verify on any node:"
echo "  verifymessage $ADDR \\"
echo "    '$SIG' \\"
echo "    '$MSG'"
echo ""
echo "Or run: $0 verify $MANIFEST_PATH $SNAP_PATH"