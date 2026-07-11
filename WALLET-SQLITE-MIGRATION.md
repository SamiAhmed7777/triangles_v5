# Wallet storage: Berkeley DB → SQLite

Goal: retire Berkeley DB as the wallet store and make **SQLite the default**
wallet backend, with a transparent, non-destructive migration of existing
`wallet.dat` files. This removes the single ugliest build dependency (BDB 5.3
with C++ bindings, hand-built on RHEL/MSYS2) and gives the wallet a modern,
maintainable, single-file store — the kind exchanges expect.

No consensus or wire behavior changes. The on-disk *record encoding* is
unchanged: keys and values are the exact `SER_DISK / CLIENT_VERSION` bytes
`CWalletDB` already produces, just stored as `(key BLOB, value BLOB)` rows in
SQLite instead of Berkeley B-tree entries. That byte-for-byte identity is what
makes migration a verbatim copy.

## Delivered in this pass

New, self-contained modules (do not disturb the working Berkeley path):

| File | Purpose |
|------|---------|
| `src/walletdb-base.h` | Backend-agnostic seam: `WalletDatabase`, `WalletBatch` (raw byte Read/Write/Erase/Has + cursor + txn), `WalletCursor`; `ResolveWalletDbKind()` / `MakeWalletDatabase()` declarations. |
| `src/walletdb-sqlite.h/.cpp` | `SQLiteDatabase` / `SQLiteBatch` — single `main(key BLOB PRIMARY KEY, value BLOB)` table, `synchronous=FULL`, prepared statements, transactions, cursor, online-backup, `integrity_check`. App-id/user-version stamping to reject foreign DBs. |
| `src/walletmigrate.h/.cpp` | `MaybeMigrateBerkeleyWalletToSQLite()` — detects a Berkeley `wallet.dat`, copies every record verbatim into a temp SQLite file, verifies the row count, backs up the original to `wallet.dat.bdb.bak`, then swaps SQLite into place. Idempotent and non-destructive. |
| `src/walletdb-factory.cpp` | `ResolveWalletDbKind()` (default **sqlite**, `-walletdb=bdb` fallback) and `MakeWalletDatabase()` (SQLite implemented). |
| `src/walletdb-batch.h` | `CWalletBatchTyped` — typed Read/Write/Erase/Exists + cursor over `WalletBatch`, byte-identical to the old `CDB` templates. The drop-in base for `CWalletDB`. |

Build wiring:
- `find_package(SQLite3 REQUIRED)` in the top-level `CMakeLists.txt`.
- `SQLite::SQLite3` linked into `triangles_common`; the new sources added to `CORE_SOURCES`.

## Remaining integration (compile-in-the-loop)

The new modules are complete but `CWalletDB` is not yet routed through the seam
— it still inherits Berkeley `CDB`. This is the mechanical-but-careful step that
needs a compiler in the loop. **It must be done and landed as one unit** (it
touches `walletdb.h`, `walletdb.cpp`, `wallet.cpp`, `db.cpp`, and `init.cpp`):
re-basing ~800 lines of funds-critical code is exactly the kind of change that
should be compiled and run against a real `wallet.dat` rather than committed
blind.

1. **Typed wrappers over the batch — DONE.** `src/walletdb-batch.h`
   (`CWalletBatchTyped`) provides `Read/Write/Erase/Exists` + cursor over a
   `WalletBatch`, byte-identical to `CDB`'s templates. `CWalletDB` derives from
   it instead of `CDB`.
2. **Re-base `CWalletDB`.** Hold a `std::unique_ptr<WalletDatabase>` +
   `WalletBatch` obtained from `MakeWalletDatabase("wallet.dat", err)` instead of
   deriving from `CDB`. Route `TxnBegin/Commit/Abort` to the batch.
3. **Cursors.** Replace `GetAtCursor` / `GetTxnCursor` / `ReadAtCursor`
   (Berkeley `Dbc*`, `DB_NEXT`) in `walletdb.cpp` (`LoadWallet`,
   `ReorderTransactions`) with `WalletBatch::GetNewCursor()` + `WalletCursor::Next()`.
4. **Berkeley-specific call sites.**
   - `BackupWallet()` / `AutoBackupWallet()` → `WalletDatabase::Backup()`.
   - `CDB::Rewrite()` (used by `CWallet::EncryptWallet`) → `WalletDatabase::Rewrite()`
     (VACUUM). Unencrypted-key cleanup already happens via explicit `Erase`.
   - `bitdb.Flush()` / env shutdown in `init.cpp` → `WalletDatabase::Flush()/Close()`
     (no-op for SQLite).
5. **Berkeley behind the same seam (optional but recommended).** Add a thin
   `BerkeleyDatabase`/`BerkeleyBatch` adapter wrapping the existing `CDBEnv`/`CDB`
   so `-walletdb=bdb` routes through `MakeWalletDatabase` too, instead of the
   legacy path. Keeps one code path for one release, then delete BDB entirely.
6. **Run the migration on startup.** In `init.cpp`, before the wallet is loaded
   and when the backend is SQLite, call
   `MaybeMigrateBerkeleyWalletToSQLite(GetDataDir()/strWalletFileName, err)`.

## Gating

```
trianglesd                 # SQLite (default)
trianglesd -walletdb=bdb   # Berkeley fallback (retained for one release)
```

## Validation checklist (must pass before release)

Cannot be verified without a build + a real wallet. Run on a node:

1. **Build** with `-DBUILD_TESTS=ON`; confirm SQLite is found and linked.
2. **Fresh wallet**: start with no wallet → a SQLite `wallet.dat` is created;
   `getnewaddress`, `getinfo` work; restart preserves keys/balance.
3. **Migration**: copy a real Berkeley `wallet.dat` into the datadir, start the
   node. Confirm: `wallet.dat.bdb.bak` is created, `wallet.dat` is now SQLite
   (`sqlite3 wallet.dat "PRAGMA integrity_check;"` → `ok`), and
   `listaddressgroupings` / `getbalance` / `dumpwallet` match a `-walletdb=bdb`
   run against the `.bdb.bak` original.
4. **Key parity**: `dumpwallet` before (bdb) and after (sqlite); diff must be
   empty (same keys, labels, metadata, HD seed).
5. **Encryption**: `encryptwallet`, restart, `walletpassphrase`, sign/spend.
6. **Backup/restore**: `backupwallet`, restore into a fresh datadir, verify
   balance and spend.
7. **Send/receive + staking** over a few blocks; confirm new keys/txns persist
   across restart.
8. **Crash safety**: kill -9 mid-write; restart; `integrity_check` ok, no loss.

## Follow-ups

- Add `test_wallet_sqlite` unit tests (round-trip, migration parity, cursor).
- Once SQLite is validated for a release, remove `-walletdb=bdb`, delete
  `db.cpp`/`walletdb`'s Berkeley code, and drop the `BerkeleyDB` CMake
  dependency — completing the retirement.
