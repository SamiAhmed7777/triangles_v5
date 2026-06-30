# RocksDB as the default chain database backend

This change finishes the RocksDB chain-database backend, makes it the default,
and provides a transparent migration path off LevelDB. **No consensus rules
change** — only how the block index / tx index / UTXO set / address index are
stored on disk. On-disk key bytes remain identical across both backends, which
is what the migration and the dual-backend equivalence tests rely on.

## What changed

### 1. Fixed the column-family iteration bug (the real "unfinished" blocker)

The RocksDB backend routed keys into per-prefix **column families**
(`blockindex`, `txindex`, `utxo`, `addrindex`) on write, but the read path —
both `CRocksTxDB::NewIterator()` and `CRocksTxDB::LoadBlockIndex()` — only ever
iterated the **default** column family. With column families enabled:

- `LoadBlockIndex()` loaded **zero** blocks (block-index records were in a
  non-default CF the loader never scanned),
- UTXO snapshot dumps and address-index range scans saw nothing, and
- the migration verifier `CollectStats()` reported a record-count mismatch.

This is why `-chaindb=rocksdb` "compiled clean but was never runtime-valid."

**Fix:** column-family partitioning is disabled. `GetCF()` now always returns
the default CF, so writes, point reads, `Exists`, `Erase`, and full-keyspace
iteration are mutually consistent — and byte-identical to the single-keyspace
LevelDB backend. New databases are created single-CF; pre-existing experimental
multi-CF databases are still opened (for compatibility) but should be
re-migrated or reindexed. RocksDB still delivers its performance win from
parallel compaction, bloom filters, large write buffer, and block cache — the
CF split was a premature optimization, not the source of the speedup.

Re-introducing column families is a tracked follow-up that first requires
CF-aware iterators (a multiplexed merge across CFs) in `NewIterator()` /
`LoadBlockIndex()`.

### 2. Automatic LevelDB -> RocksDB migration on startup

`init.cpp` now runs the migration automatically when RocksDB is the active
backend and the only chain DB present is a legacy `txleveldb/` (no `rocksdb/`
yet). `MaybeMigrateLevelDbToRocksDb()` is a no-op when there is nothing to
migrate, so it is safe on every launch. The LevelDB source is never modified;
it remains a fallback.

### 3. RocksDB is now the default backend

`-chaindb` defaults to `rocksdb` (was `leveldb`). LevelDB stays selectable with
`-chaindb=leveldb` and is retained as migration source + fallback. Full removal
of LevelDB is deferred to a later phase, after live-chain validation.

### 4. Fixed `NeedsBootstrap()` to recognize the RocksDB directory

`Bootstrap::NeedsBootstrap()` checked for `txleveldb/` but not `rocksdb/`. With
RocksDB as default, a fully-synced rocksdb-only node would have been treated as
"fresh" and could have triggered a bootstrap download over a healthy chain on
every restart. It now treats a `rocksdb/` directory as an existing chain DB.

## Files changed

- `src/txdb-rocksdb.cpp` — disable CF routing; single-CF open; remove dead CF tables
- `src/txdb-rocksdb.h` — update CF member docs
- `src/txdb-factory.cpp` — default backend `leveldb` -> `rocksdb`
- `src/txdb.h` — update factory doc comment
- `src/init.cpp` — auto-migrate on startup when RocksDB active + legacy LevelDB present
- `src/bootstrap.cpp` — `NeedsBootstrap()` recognizes `rocksdb/`
- `src/test/chaindb_runtime_tests.cpp` — update default-backend expectations
- `README.md` — document RocksDB default + migration

## Build

```bash
cmake -B build -G Ninja -DBUILD_QT=ON -DBUILD_TESTS=ON
cmake --build build
```

RocksDB is required (`librocksdb-dev` >= 7.4 on Debian/Ubuntu,
`mingw-w64-x86_64-rocksdb` on MSYS2, `rocksdb` on Homebrew).

## Tests

```bash
# RocksDB wrapper runtime smoke tests (the class the daemon uses at runtime)
./build/bin/test_chaindb_runtime

# LevelDB/RocksDB byte-for-byte migration equivalence
./build/bin/test_chaindb_equivalence

# Full unit suite
./build/bin/test_triangles
```

Expected after this change:
- `get_chain_data_dir_default_is_rocksdb` passes (default resolves to rocksdb).
- `iterator_walks_every_key_in_sorted_order` passes (the `"banana"` key, which
  previously routed to a non-default CF the iterator never read, now lives in
  the default CF and is iterated).
- Migration verification (`CollectStats` / `StatsMatch`) passes end-to-end.

## Live-chain validation checklist (V6 task T010)

This is the step that cannot be done without real chain data and must be run on
a node before release:

1. **Migrate a real chain.** On a node with an existing `txleveldb/`, launch the
   new binary (default backend). Confirm the log shows
   `ChainDB: RocksDB backend active with a legacy LevelDB present; migrating
   automatically.` followed by `ChainDB migration: verified N records ... best=<hash>`.
2. **Verify block index loads.** Confirm `LoadBlockIndex()` reports the correct
   `height=` and `hashBestChain=` (matching the prior LevelDB tip), not 0.
3. **Compare RPC output.** `getinfo`, `getblockcount`, `getbestblockhash`, and a
   spot-check of `gettxout` / address-index queries must match a LevelDB run of
   the same datadir (`-chaindb=leveldb`).
4. **Restart twice.** Confirm no spurious bootstrap download fires and the tip is
   stable across restarts.
5. **Sync new blocks.** Let the node accept and stake new blocks; confirm UTXO
   set and money supply stay consistent.
6. **Benchmark.** Use `contrib/bench/bench-chaindb.sh --backends=rocksdb` vs
   `leveldb` to confirm the speedup on this hardware.

## Rollback

Set `-chaindb=leveldb` in `triangles.conf` (or on the command line). The
original `txleveldb/` is untouched by migration, so reverting is immediate.

## Remaining follow-ups

- CF-aware iteration, then re-enable column-family partitioning for independent
  compaction/caching.
- Retire LevelDB entirely (remove `txdb-leveldb.*`, drop the `-chaindb=leveldb`
  option and the bundled LevelDB dependency) once RocksDB is validated in
  production for at least one release cycle.
