// Copyright (c) 2026 The Triangles developers.
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// Backend-agnostic wallet storage seam.
//
// Historically CWalletDB derived directly from CDB (Berkeley DB). To allow the
// wallet to be stored in SQLite instead, storage is abstracted behind two
// interfaces modeled on Bitcoin Core's WalletDatabase / DatabaseBatch:
//
//   WalletDatabase  - owns the on-disk database (open/close/flush/backup/
//                     rewrite) and hands out batches.
//   WalletBatch     - a unit of work against the database: raw byte-level
//                     Read/Write/Erase/Exists, a cursor for full scans, and an
//                     optional atomic transaction.
//
// Only RAW BYTES cross this interface. All key/value (de)serialization stays in
// CWalletDB via CDataStream with SER_DISK / CLIENT_VERSION, exactly as before,
// so the on-disk record encoding is identical across backends. That byte
// identity is what makes the Berkeley -> SQLite migration a verbatim key/value
// copy.

#ifndef TRIANGLES_WALLETDB_BASE_H
#define TRIANGLES_WALLETDB_BASE_H

#include <memory>
#include <string>
#include <vector>

using KeyBytes   = std::vector<unsigned char>;
using ValueBytes = std::vector<unsigned char>;

// Result of advancing a cursor.
enum class WalletCursorStatus { MORE, DONE, FAIL };

// Forward scan over every record in a database. Yields raw serialized
// key/value bytes; the caller deserializes. Cursors do not observe uncommitted
// writes in an open transaction (all wallet scan sites run outside txns).
class WalletCursor
{
public:
    virtual ~WalletCursor() = default;
    virtual WalletCursorStatus Next(KeyBytes& key, ValueBytes& value) = 0;
};

// A unit of work against a wallet database.
class WalletBatch
{
public:
    virtual ~WalletBatch() = default;

    // Byte-level accessors. WriteKey honors fOverwrite (false => fail if the
    // key already exists, matching Berkeley's DB_NOOVERWRITE). EraseKey returns
    // true when the key is gone afterwards (including "was not present").
    virtual bool ReadKey(const KeyBytes& key, ValueBytes& value) = 0;
    virtual bool WriteKey(const KeyBytes& key, const ValueBytes& value, bool fOverwrite = true) = 0;
    virtual bool EraseKey(const KeyBytes& key) = 0;
    virtual bool HasKey(const KeyBytes& key) = 0;

    // Full-database scan.
    virtual std::unique_ptr<WalletCursor> GetNewCursor() = 0;

    // Atomic transaction around a group of writes/erases. At most one may be
    // open per batch at a time.
    virtual bool TxnBegin() = 0;
    virtual bool TxnCommit() = 0;
    virtual bool TxnAbort() = 0;

    virtual void Close() = 0;
};

// An on-disk wallet database.
class WalletDatabase
{
public:
    virtual ~WalletDatabase() = default;

    // Hand out a batch. flush_on_close asks the backend to flush durable state
    // when the batch is destroyed (Berkeley parity for the common write path).
    virtual std::unique_ptr<WalletBatch> MakeBatch(bool flush_on_close = true) = 0;

    // Rewrite the database compactly, optionally skipping records whose key
    // begins with pszSkip (used by the wallet to drop the unencrypted "key"
    // records after encryption). Berkeley implements this via CDB::Rewrite;
    // SQLite implements it via VACUUM (+ optional delete of skipped keys).
    virtual bool Rewrite(const char* pszSkip = nullptr) = 0;

    // Copy the live database to a destination path (wallet backup).
    virtual bool Backup(const std::string& strDest) const = 0;

    // Durability / lifecycle.
    virtual void Flush() = 0;
    virtual void Close() = 0;

    // Integrity check before first use. Fills strError on failure.
    virtual bool Verify(std::string& strError) = 0;

    // Human-readable identifier for logging (filename or path).
    virtual std::string Filename() const = 0;
};

// Backend selector, parsed from -walletdb. SQLite is the default; Berkeley is
// retained for one release as a fallback and as the migration source.
enum class WalletDbKind { SQLite, Berkeley };

// Resolve the configured wallet backend from -walletdb (default: SQLite).
WalletDbKind ResolveWalletDbKind();

// Open (creating if needed) the wallet database for the configured backend.
// strFilename is the logical wallet name (e.g. "wallet.dat"); the SQLite
// backend stores it as "<name>" under the data dir, Berkeley as before.
std::unique_ptr<WalletDatabase> MakeWalletDatabase(const std::string& strFilename,
                                                   std::string& strError);

#endif // TRIANGLES_WALLETDB_BASE_H
