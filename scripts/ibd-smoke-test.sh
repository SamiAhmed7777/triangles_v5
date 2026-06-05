#!/usr/bin/env bash
set -euo pipefail

# Fresh-datadir IBD smoke test for TRI.
# Goal: detect the classic "starts from zero but stalls early / loops around 570"
# failure mode, and verify that sync keeps making forward progress.
#
# Example:
#   bash scripts/ibd-smoke-test.sh \
#     --bin ./build/src/trianglesd \
#     --bootstrap-url http://100.104.4.5:8085/triangles-bootstrap.tar.gz \
#     --addnode 74.208.167.19 --addnode 194.233.88.206

BIN="${BIN:-./build/src/trianglesd}"
TIMEOUT_SECONDS="${TIMEOUT_SECONDS:-1800}"   # 30 minutes target window
POLL_SECONDS="${POLL_SECONDS:-15}"
STALL_WINDOW_SECONDS="${STALL_WINDOW_SECONDS:-180}"
BOOTSTRAP_URL="${BOOTSTRAP_URL:-}"
WORKDIR="${WORKDIR:-}"
RPC_PORT="${RPC_PORT:-19192}"
P2P_PORT="${P2P_PORT:-24193}"
MIN_EXPECTED_HEIGHT="${MIN_EXPECTED_HEIGHT:-5000}"
ALLOW_IBD="${ALLOW_IBD:-0}"
WHITELIST="${WHITELIST:-127.0.0.1}"
ADDNODES=()

usage() {
  cat <<EOF
Usage: $0 [options]

Options:
  --bin PATH               trianglesd binary (default: $BIN)
  --bootstrap-url URL      optional bootstrap tar.gz URL to preload
  --workdir PATH           use an explicit temp workdir
  --rpc-port N             RPC port for test node (default: $RPC_PORT)
  --p2p-port N             P2P port for test node (default: $P2P_PORT)
  --timeout N              total test timeout seconds (default: $TIMEOUT_SECONDS)
  --poll N                 poll interval seconds (default: $POLL_SECONDS)
  --stall-window N         no-progress failure window seconds (default: $STALL_WINDOW_SECONDS)
  --min-height N           minimum expected height/progress floor (default: $MIN_EXPECTED_HEIGHT)
  --allow-ibd              allow test to pass while still in IBD if progress is strong
  --addnode HOST           trusted peer to add (repeatable)
  -h, --help               show this help
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --bin) BIN="$2"; shift 2 ;;
    --bootstrap-url) BOOTSTRAP_URL="$2"; shift 2 ;;
    --workdir) WORKDIR="$2"; shift 2 ;;
    --rpc-port) RPC_PORT="$2"; shift 2 ;;
    --p2p-port) P2P_PORT="$2"; shift 2 ;;
    --timeout) TIMEOUT_SECONDS="$2"; shift 2 ;;
    --poll) POLL_SECONDS="$2"; shift 2 ;;
    --stall-window) STALL_WINDOW_SECONDS="$2"; shift 2 ;;
    --min-height) MIN_EXPECTED_HEIGHT="$2"; shift 2 ;;
    --allow-ibd) ALLOW_IBD=1; shift ;;
    --addnode) ADDNODES+=("$2"); shift 2 ;;
    -h|--help) usage; exit 0 ;;
    *) echo "unknown arg: $1" >&2; usage; exit 2 ;;
  esac
done

if [[ ! -x "$BIN" ]]; then
  echo "ERROR: trianglesd binary not executable: $BIN" >&2
  exit 2
fi

if [[ -z "$WORKDIR" ]]; then
  WORKDIR="$(mktemp -d /tmp/tri-ibd-smoke-XXXXXX)"
fi
DATADIR="$WORKDIR/datadir"
mkdir -p "$DATADIR"

RPCUSER="tri_test"
RPCPASSWORD="tri_test_$(date +%s)_$RANDOM"
CONF="$DATADIR/triangles.conf"
cat > "$CONF" <<EOF
server=1
daemon=1
staking=0
listen=1
discover=0
upnp=0
tor=0
irc=0
dnsseed=1
checkpoints=1
rpcuser=$RPCUSER
rpcpassword=$RPCPASSWORD
rpcport=$RPC_PORT
port=$P2P_PORT
maxconnections=32
whitelist=$WHITELIST
logtimestamps=1
EOF

for host in "${ADDNODES[@]}"; do
  echo "addnode=$host" >> "$CONF"
done

cleanup() {
  "$BIN" -datadir="$DATADIR" -conf="$CONF" stop >/dev/null 2>&1 || true
  sleep 2 || true
  pkill -f "$DATADIR" >/dev/null 2>&1 || true
}
trap cleanup EXIT

if [[ -n "$BOOTSTRAP_URL" ]]; then
  echo "[ibd-test] downloading bootstrap: $BOOTSTRAP_URL"
  curl -L --fail --max-time 1800 "$BOOTSTRAP_URL" -o "$WORKDIR/bootstrap.tar.gz"
  tar xzf "$WORKDIR/bootstrap.tar.gz" -C "$DATADIR"
  rm -f "$DATADIR/database/log."* "$DATADIR/txleveldb/LOCK" "$DATADIR/smsgDB/LOCK" 2>/dev/null || true
fi

echo "[ibd-test] starting node from datadir: $DATADIR"
"$BIN" -daemon -datadir="$DATADIR" -conf="$CONF" >/dev/null
sleep 6

rpc() {
  local method="$1"
  local params="${2:-[]}"
  curl -sS --fail --user "$RPCUSER:$RPCPASSWORD" \
    --data-binary "{\"jsonrpc\":\"1.0\",\"id\":\"ibd\",\"method\":\"$method\",\"params\":$params}" \
    -H 'content-type: text/plain;' "http://127.0.0.1:$RPC_PORT/"
}

extract_json() {
  python3 -c 'import json,sys; obj=json.load(sys.stdin); print(obj["result"])'
}

extract_field() {
  local field="$1"
  python3 -c 'import json,sys; obj=json.load(sys.stdin); val=obj["result"].get(sys.argv[1]); print(val if val is not None else "")' "$field"
}

start_ts=$(date +%s)
last_progress_ts=$start_ts
last_height=-1
samples=0
same_570_loops=0
best_height=0

while true; do
  now=$(date +%s)
  elapsed=$((now - start_ts))
  if (( elapsed > TIMEOUT_SECONDS )); then
    echo "FAIL: timeout after ${elapsed}s"
    break
  fi

  if info_json="$(rpc getblockchaininfo 2>/dev/null)"; then
    height=$(printf '%s' "$info_json" | extract_field blocks)
    ibd=$(printf '%s' "$info_json" | extract_field initialblockdownload)
    headers=$(printf '%s' "$info_json" | extract_field headers)
  else
    height=""
    ibd=""
    headers=""
  fi

  peers=0
  if peer_json="$(rpc getconnectioncount 2>/dev/null)"; then
    peers=$(printf '%s' "$peer_json" | extract_json)
  fi

  if [[ -n "$height" && "$height" != "$last_height" ]]; then
    last_progress_ts=$now
    last_height="$height"
    if (( height > best_height )); then
      best_height=$height
    fi
  fi

  log_file="$DATADIR/debug.log"
  if [[ -f "$log_file" ]]; then
    loop_hits=$(tail -n 400 "$log_file" | grep -c 'start=571' || true)
    if (( loop_hits >= 3 )); then
      same_570_loops=$loop_hits
    fi
  fi

  echo "[ibd-test] t=${elapsed}s height=${height:-?} headers=${headers:-?} ibd=${ibd:-?} peers=$peers best=$best_height"

  if [[ -n "$height" ]] && (( best_height >= MIN_EXPECTED_HEIGHT )) && [[ "$ibd" == "False" || "$ibd" == "false" ]]; then
    echo "PASS: left IBD and reached height $best_height"
    exit 0
  fi

  if [[ "$ALLOW_IBD" == "1" && -n "$height" ]] && (( best_height >= MIN_EXPECTED_HEIGHT )); then
    echo "PASS: strong sync progress observed (height $best_height) even though IBD remains true"
    exit 0
  fi

  if (( now - last_progress_ts > STALL_WINDOW_SECONDS )); then
    echo "FAIL: no block-height progress for $((now - last_progress_ts))s"
    if (( same_570_loops > 0 )); then
      echo "HINT: detected repeated start=571 loop pattern ($same_570_loops hits in recent log tail)"
    fi
    echo "--- debug tail ---"
    tail -n 120 "$log_file" 2>/dev/null || true
    exit 1
  fi

  ((samples++)) || true
  sleep "$POLL_SECONDS"
done

exit 1
