#!/usr/bin/env bash
# Benchmark FastImportBlockFile() speed across chain-DB backends.
#
# Reads a user-supplied blk0001.dat (old-style block stream) and times the
# full block-index rebuild under each backend. Output: a CSV row per backend
# with wall time, peak RSS, and resulting datadir size on disk.
#
# Usage:
#   ./bench-chaindb.sh \
#     --binary=/path/to/trianglesd \
#     --bootstrap=/path/to/blk0001.dat \
#     [--backends=leveldb,rocksdb]       default: both
#     [--workdir=/tmp/triangles-bench]   parent dir for per-backend datadirs
#     [--dbcache=2048]                   in MB
#     [--results-csv=./bench-results.csv]
#     [--keep-datadirs]                  preserve datadirs after run
#     [--rpc-port=BASE]                  default 19112; each run uses BASE+offset
#
# Notes:
#   - RocksDB is a hard build dep, so any current trianglesd has both backends.
#   - This script does not assume Tor is configured. It launches with -nolisten
#     and -connect=0 to keep the run network-isolated.
#   - Wall time comes from the daemon's own perf log line:
#         "FastImportBlockFile: indexed N blocks in Mms"
#   - Peak RSS is sampled via `ps -o rss=` once a second.

set -euo pipefail

# ── Defaults ────────────────────────────────────────────────────────────────
BINARY=""
BOOTSTRAP=""
BACKENDS="leveldb,rocksdb"
WORKDIR=""
DBCACHE=2048
RESULTS_CSV="./bench-results.csv"
KEEP=0
RPC_BASE=19112

# ── Arg parsing ─────────────────────────────────────────────────────────────
for arg in "$@"; do
    case "$arg" in
        --binary=*)         BINARY="${arg#*=}" ;;
        --bootstrap=*)      BOOTSTRAP="${arg#*=}" ;;
        --backends=*)       BACKENDS="${arg#*=}" ;;
        --workdir=*)        WORKDIR="${arg#*=}" ;;
        --dbcache=*)        DBCACHE="${arg#*=}" ;;
        --results-csv=*)    RESULTS_CSV="${arg#*=}" ;;
        --keep-datadirs)    KEEP=1 ;;
        --rpc-port=*)       RPC_BASE="${arg#*=}" ;;
        -h|--help)
            sed -n '2,28p' "$0" | sed 's/^# \?//'
            exit 0 ;;
        *)
            echo "Unknown argument: $arg" >&2
            exit 2 ;;
    esac
done

[ -n "$BINARY" ]    || { echo "--binary is required" >&2; exit 2; }
[ -n "$BOOTSTRAP" ] || { echo "--bootstrap is required" >&2; exit 2; }
[ -x "$BINARY" ]    || { echo "Binary not executable: $BINARY" >&2; exit 2; }
[ -f "$BOOTSTRAP" ] || { echo "Bootstrap file not found: $BOOTSTRAP" >&2; exit 2; }

if [ -z "$WORKDIR" ]; then
    WORKDIR="$(mktemp -d -t triangles-bench-XXXXXX)"
fi
mkdir -p "$WORKDIR"
echo "Workdir: $WORKDIR"

# ── CSV header (only if file is new) ───────────────────────────────────────
if [ ! -f "$RESULTS_CSV" ]; then
    echo "timestamp,backend,bootstrap_size_bytes,dbcache_mb,blocks_indexed,wall_ms,peak_rss_kb,datadir_bytes,binary,host" > "$RESULTS_CSV"
fi

bootstrap_size="$(stat -c%s "$BOOTSTRAP" 2>/dev/null || stat -f%z "$BOOTSTRAP")"
host="$(hostname)"
ts_run="$(date -u +%Y-%m-%dT%H:%M:%SZ)"

# ── Per-backend run ─────────────────────────────────────────────────────────
run_backend() {
    local backend="$1"
    local idx="$2"
    local datadir="$WORKDIR/$backend"
    local rpc_port=$((RPC_BASE + idx))
    local rss_log="$WORKDIR/$backend.rss.log"

    echo
    echo "════════════════════════════════════════════════════════════════════"
    echo "  Backend: $backend  (datadir: $datadir, rpcport: $rpc_port)"
    echo "════════════════════════════════════════════════════════════════════"

    # Fresh datadir, copy bootstrap into place. FastImportBlockFile() picks
    # this up automatically when the block index is empty.
    rm -rf "$datadir"
    mkdir -p "$datadir"
    cp "$BOOTSTRAP" "$datadir/blk0001.dat"

    # Minimal config — disable network so we measure only the import path.
    cat > "$datadir/triangles.conf" <<EOF
chaindb=$backend
dbcache=$DBCACHE
nolisten=1
connect=0
rpcuser=bench
rpcpassword=bench
rpcport=$rpc_port
debug=1
printtoconsole=0
EOF

    # Launch in background. -daemon would daemonize but we want to track the
    # process tree; run in foreground and background it ourselves so we keep
    # the PID for RSS sampling and clean shutdown.
    local pid
    "$BINARY" -datadir="$datadir" -conf="triangles.conf" >"$datadir/stdout.log" 2>&1 &
    pid=$!
    echo "Launched $backend (pid $pid)"

    # RSS sampler: log peak every second to a file.
    (
        while kill -0 "$pid" 2>/dev/null; do
            ps -o rss= -p "$pid" 2>/dev/null | tr -d ' ' >> "$rss_log" || true
            sleep 1
        done
    ) &
    local sampler_pid=$!

    # Watch for "FastImportBlockFile: indexed N blocks in Mms" in the daemon's
    # debug.log, which is the deterministic completion signal.
    local debug_log="$datadir/debug.log"
    local wait_start
    wait_start="$(date +%s)"
    local timeout_s=86400  # 24 hours hard cap
    local indexed_line=""
    while :; do
        if [ -f "$debug_log" ]; then
            indexed_line="$(grep -E "FastImportBlockFile: indexed [0-9]+ blocks in [0-9]+ms" "$debug_log" | tail -1 || true)"
            if [ -n "$indexed_line" ]; then
                break
            fi
        fi
        if ! kill -0 "$pid" 2>/dev/null; then
            echo "Daemon exited before completion line appeared. Check $datadir/stdout.log" >&2
            kill "$sampler_pid" 2>/dev/null || true
            return 1
        fi
        local elapsed=$(( $(date +%s) - wait_start ))
        if [ "$elapsed" -gt "$timeout_s" ]; then
            echo "Timeout after ${timeout_s}s without completion line" >&2
            kill "$pid" 2>/dev/null || true
            kill "$sampler_pid" 2>/dev/null || true
            return 1
        fi
        sleep 5
    done

    echo "Completion: $indexed_line"

    # Parse blocks_indexed and wall_ms from the line.
    local blocks_indexed wall_ms
    blocks_indexed="$(echo "$indexed_line" | sed -E 's/.*indexed ([0-9]+) blocks.*/\1/')"
    wall_ms="$(echo "$indexed_line" | sed -E 's/.*in ([0-9]+)ms.*/\1/')"

    # Stop daemon cleanly via RPC, fall back to SIGTERM.
    "$BINARY" -datadir="$datadir" -conf="triangles.conf" stop >/dev/null 2>&1 || \
        kill -TERM "$pid" 2>/dev/null || true

    # Wait up to 60s for clean exit.
    local stop_wait=0
    while kill -0 "$pid" 2>/dev/null && [ "$stop_wait" -lt 60 ]; do
        sleep 1
        stop_wait=$((stop_wait + 1))
    done
    kill -KILL "$pid" 2>/dev/null || true
    wait "$sampler_pid" 2>/dev/null || true

    # Peak RSS: max of the sampler's recorded values.
    local peak_rss_kb=0
    if [ -f "$rss_log" ] && [ -s "$rss_log" ]; then
        peak_rss_kb="$(sort -nr "$rss_log" | head -1)"
    fi

    # Datadir size — separate the chain DB from blk0001.dat (which is ~constant
    # across backends). We report the total datadir size; the consumer can
    # subtract bootstrap_size_bytes if they want chain-DB-only.
    local datadir_bytes
    datadir_bytes="$(du -sb "$datadir" 2>/dev/null | awk '{print $1}' || du -sk "$datadir" | awk '{print $1*1024}')"

    # Append CSV row.
    echo "$ts_run,$backend,$bootstrap_size,$DBCACHE,$blocks_indexed,$wall_ms,$peak_rss_kb,$datadir_bytes,$BINARY,$host" >> "$RESULTS_CSV"

    # Stdout summary.
    printf "  blocks indexed: %s\n" "$blocks_indexed"
    printf "  wall time:      %s ms (%.1f min)\n" "$wall_ms" "$(awk "BEGIN{print $wall_ms/60000}")"
    printf "  peak RSS:       %s KB (%.1f GB)\n" "$peak_rss_kb" "$(awk "BEGIN{print $peak_rss_kb/1024/1024}")"
    printf "  datadir size:   %s bytes (%.1f GB)\n" "$datadir_bytes" "$(awk "BEGIN{print $datadir_bytes/1024/1024/1024}")"

    # Cleanup unless --keep-datadirs.
    if [ "$KEEP" -eq 0 ]; then
        rm -rf "$datadir"
    fi
}

# ── Main loop ──────────────────────────────────────────────────────────────
idx=0
IFS=',' read -r -a backends_arr <<< "$BACKENDS"
for backend in "${backends_arr[@]}"; do
    case "$backend" in
        leveldb|rocksdb) ;;
        *) echo "Unknown backend: $backend" >&2; exit 2 ;;
    esac
    run_backend "$backend" "$idx"
    idx=$((idx + 1))
done

echo
echo "Done. Results appended to $RESULTS_CSV"
