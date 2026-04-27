# Chain DB benchmark harness

Measures `FastImportBlockFile()` speed under each chain-DB backend
(LevelDB vs RocksDB) using a user-supplied `blk0001.dat` block stream.

## Prerequisites

- A `trianglesd` binary built with both backends:
  ```
  cmake -B build -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_QT=OFF \
      -DBUILD_DAEMON=ON \
      -DBUILD_ROCKSDB=ON
  cmake --build build
  ```
- An `blk0001.dat` file (old-style block stream). If you have a synced
  node, copy `~/.triangles/blk0001.dat` (Linux) or `%APPDATA%\triangles\blk0001.dat` (Windows).
- Free disk space: ~3× the size of `blk0001.dat` per backend run
  (raw blocks + chain DB index + working space).

## Usage

```bash
contrib/bench/bench-chaindb.sh \
  --binary=$(pwd)/build/bin/trianglesd \
  --bootstrap=/path/to/blk0001.dat
```

Runs each backend in turn, appends a CSV row to `./bench-results.csv`,
and prints a summary to stdout. Default `--dbcache=2048` (MB).

### Options

| Flag | Default | Notes |
| --- | --- | --- |
| `--binary=PATH` | (required) | Path to `trianglesd` |
| `--bootstrap=PATH` | (required) | Path to `blk0001.dat` |
| `--backends=LIST` | `leveldb,rocksdb` | Comma-separated subset |
| `--workdir=DIR` | `/tmp/triangles-bench-XXXXXX` | Per-backend datadirs go here |
| `--dbcache=MB` | `2048` | Chain DB cache size |
| `--results-csv=FILE` | `./bench-results.csv` | Appended to |
| `--keep-datadirs` | off | Preserve datadirs after run for inspection |
| `--rpc-port=BASE` | `19112` | Each backend uses `BASE+offset` |

## What it measures

| Column | Source |
| --- | --- |
| `wall_ms` | The daemon's own log line: `FastImportBlockFile: indexed N blocks in Mms` |
| `peak_rss_kb` | `ps -o rss=` sampled once per second |
| `datadir_bytes` | `du -sb` of the working datadir (includes `blk0001.dat`) |
| `blocks_indexed` | Parsed from the same log line |

## What it does not measure

- Network IBD (peer fetch, header sync) — this is pure DB ingest.
- UTXO snapshot load — `LoadSnapshot` is currently rocksdb-guarded
  (see `src/utxosnapshot.cpp`); will be unblocked when LevelDB is retired.
- Reorg cost — separate test, not yet implemented.
- Disk I/O bytes (read/written) — could be added with `iostat` integration.

## Interpreting results

A meaningful comparison requires both rows to have run on the same machine
with the same `blk0001.dat`. The `host` column makes mixing runs across
machines visible in the CSV.

Backend-relevant size comparisons should subtract `bootstrap_size_bytes`
from `datadir_bytes` to isolate the chain DB tree.

## One-liners

```bash
# LevelDB only
./bench-chaindb.sh --binary=... --bootstrap=... --backends=leveldb

# Compare 2GB vs 4GB cache on RocksDB
./bench-chaindb.sh --binary=... --bootstrap=... --backends=rocksdb --dbcache=2048
./bench-chaindb.sh --binary=... --bootstrap=... --backends=rocksdb --dbcache=4096

# Keep the datadirs for poking around afterwards
./bench-chaindb.sh --binary=... --bootstrap=... --keep-datadirs
```
