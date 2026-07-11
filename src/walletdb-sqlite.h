// Copyright (c) 2026 The Triangles developers.
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// SQLite backend for the wallet database. Stores every wallet record as a row
// in a single table:
//
//     CREATE TABLE main (key BLOB PRIMARY KEY NOT NULL, value BLOB NOT NULL);
//
// The key/value blobs are the exact serialized bytes CWalletDB already
// produces (SER_DISK / CLIENT_VERSION), so a SQLite wallet is byte-for-byte
// equivalent in content to the Berkeley wallet.dat it was migrated from.
//
// Modeled on Bitcoin Core's SQLiteDatabase / SQLiteBatch.

#ifndef TRIANGLES_WALLETDB_SQLITE_H
#define TRIANGLES_WALLETDB_SQLITE_H

#include "walletdb-base.h"

#include <filesystem>
#include <string>

#include <sqlite3.h>

class SQLiteDatabase;

// A batch (and optional transaction) against a SQLiteDatabase. Holds prepared
// statements bound to the shared connection owned by SQLiteDatabase.
class SQLiteBatch final : public WalletBatch
{
public:
    explicit SQLiteBatch(SQLiteDatabase& database);
    ~SQLiteBatch() override { Close(); }

    bool ReadKey(const KeyBytes& key, ValueBytes& value) override;
    bool WriteKey(const KeyBytes& key, const ValueBytes& value, bool fOverwrite = true) override;
    bool EraseKey(const KeyBytes& key) override;
    bool HasKey(const KeyBytes& key) override;

    std::unique_ptr<WalletCursor> GetNewCursor() override;

    bool TxnBegin() override;
    bool TxnCommit() override;
    bool TxnAbort() override;

    void Close() override;

private:
    SQLiteDatabase& m_database;

    // Prepared statements (lazily compiled on first use, finalized on Close).
    sqlite3_stmt* m_read_stmt   = nullptr;
    sqlite3_stmt* m_insert_stmt = nullptr;  // INSERT OR REPLACE
    sqlite3_stmt* m_overwrite_stmt = nullptr; // INSERT (fail if exists)
    sqlite3_stmt* m_delete_stmt = nullptr;

    bool PrepareStatements();
};

// The on-disk SQLite wallet database. Owns the single sqlite3 connection that
// all of its batches share (wallet access is serialized by the wallet's own
// locks, matching the Berkeley backend's single-environment model).
class SQLiteDatabase final : public WalletDatabase
{
public:
    // file_path: absolute path to the .dat file on disk.
    explicit SQLiteDatabase(const std::filesystem::path& file_path);
    ~SQLiteDatabase() override;

    // Open the connection, apply pragmas, and create the schema if absent.
    // Returns false (with strError set) on failure.
    bool Open(std::string& strError);

    std::unique_ptr<WalletBatch> MakeBatch(bool flush_on_close = true) override;

    bool Rewrite(const char* pszSkip = nullptr) override;
    bool Backup(const std::string& strDest) const override;
    void Flush() override;
    void Close() override;
    bool Verify(std::string& strError) override;
    std::string Filename() const override { return m_file_path.string(); }

    sqlite3* Handle() const { return m_db; }

private:
    std::filesystem::path m_file_path;
    sqlite3* m_db = nullptr;

    bool ExecOrError(const char* sql, std::string& strError) const;
};

// Magic written into PRAGMA application_id so we can recognize our wallet files
// and refuse to open foreign SQLite databases. ASCII "TRIw".
static constexpr int SQLITE_WALLET_APP_ID = 0x54526977;
// Schema version in PRAGMA user_version.
static constexpr int SQLITE_WALLET_SCHEMA_VERSION = 1;

#endif // TRIANGLES_WALLETDB_SQLITE_H
