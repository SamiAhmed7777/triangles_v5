// Copyright (c) 2026 The Triangles developers.
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "walletdb-sqlite.h"
#include "util.h"

#include <cstring>

namespace fs = std::filesystem;

// ─── helpers ────────────────────────────────────────────────────────────────

// Bind a byte buffer as a BLOB parameter (1-based index). SQLITE_TRANSIENT so
// SQLite copies the bytes; the source vector need not outlive the step.
static int BindBlob(sqlite3_stmt* stmt, int idx, const std::vector<unsigned char>& v)
{
    // A zero-length blob still binds correctly with a non-null pointer.
    const void* p = v.empty() ? "" : static_cast<const void*>(v.data());
    return sqlite3_bind_blob(stmt, idx, p, static_cast<int>(v.size()), SQLITE_TRANSIENT);
}

static void ColumnBlob(sqlite3_stmt* stmt, int col, std::vector<unsigned char>& out)
{
    const unsigned char* p = static_cast<const unsigned char*>(sqlite3_column_blob(stmt, col));
    int n = sqlite3_column_bytes(stmt, col);
    out.assign(p, p + (n > 0 ? n : 0));
}

// ─── SQLiteDatabase ──────────────────────────────────────────────────────────

SQLiteDatabase::SQLiteDatabase(const fs::path& file_path)
    : m_file_path(file_path)
{
}

SQLiteDatabase::~SQLiteDatabase()
{
    Close();
}

bool SQLiteDatabase::ExecOrError(const char* sql, std::string& strError) const
{
    char* errmsg = nullptr;
    int rc = sqlite3_exec(m_db, sql, nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        strError = strprintf("SQLite: '%s' failed: %s", sql, errmsg ? errmsg : sqlite3_errstr(rc));
        if (errmsg) sqlite3_free(errmsg);
        return false;
    }
    return true;
}

bool SQLiteDatabase::Open(std::string& strError)
{
    if (m_db)
        return true;

    int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX;
    int rc = sqlite3_open_v2(m_file_path.string().c_str(), &m_db, flags, nullptr);
    if (rc != SQLITE_OK) {
        strError = strprintf("Failed to open SQLite wallet %s: %s",
                             m_file_path.string().c_str(), sqlite3_errstr(rc));
        if (m_db) { sqlite3_close(m_db); m_db = nullptr; }
        return false;
    }

    // Block (rather than fail) for up to 5s if another handle holds the lock.
    sqlite3_busy_timeout(m_db, 5000);

    // Durability + integrity pragmas. FULL fsync on commit — a wallet must not
    // lose a freshly-written key on power loss.
    if (!ExecOrError("PRAGMA synchronous = FULL;", strError)) return false;
    if (!ExecOrError("PRAGMA journal_mode = DELETE;", strError)) return false;
    if (!ExecOrError("PRAGMA secure_delete = ON;", strError)) return false;
    if (!ExecOrError("PRAGMA temp_store = MEMORY;", strError)) return false;
    if (!ExecOrError("PRAGMA foreign_keys = ON;", strError)) return false;
    // Fail loudly instead of silently truncating an over-long blob.
    if (!ExecOrError("PRAGMA cell_size_check = ON;", strError)) return false;

    // Identify our schema via application_id / user_version. A brand-new file
    // reports 0/0; an existing file must match ours (refuse foreign DBs).
    int appId = 0, userVer = 0;
    {
        sqlite3_stmt* st = nullptr;
        if (sqlite3_prepare_v2(m_db, "PRAGMA application_id;", -1, &st, nullptr) == SQLITE_OK &&
            sqlite3_step(st) == SQLITE_ROW)
            appId = sqlite3_column_int(st, 0);
        sqlite3_finalize(st);
        st = nullptr;
        if (sqlite3_prepare_v2(m_db, "PRAGMA user_version;", -1, &st, nullptr) == SQLITE_OK &&
            sqlite3_step(st) == SQLITE_ROW)
            userVer = sqlite3_column_int(st, 0);
        sqlite3_finalize(st);
    }

    if (appId != 0 && appId != SQLITE_WALLET_APP_ID) {
        strError = strprintf("%s is not a Triangles SQLite wallet (application_id=0x%08x)",
                             m_file_path.string().c_str(), appId);
        sqlite3_close(m_db);
        m_db = nullptr;
        return false;
    }
    if (userVer > SQLITE_WALLET_SCHEMA_VERSION) {
        strError = strprintf("%s was written by a newer wallet (schema v%d > v%d)",
                             m_file_path.string().c_str(), userVer, SQLITE_WALLET_SCHEMA_VERSION);
        sqlite3_close(m_db);
        m_db = nullptr;
        return false;
    }

    // Create schema (idempotent) and stamp identity on fresh files.
    if (!ExecOrError("CREATE TABLE IF NOT EXISTS main "
                     "(key BLOB PRIMARY KEY NOT NULL, value BLOB NOT NULL);", strError))
        return false;
    if (appId == 0) {
        std::string set = strprintf("PRAGMA application_id = %d;", SQLITE_WALLET_APP_ID);
        if (!ExecOrError(set.c_str(), strError)) return false;
    }
    {
        std::string set = strprintf("PRAGMA user_version = %d;", SQLITE_WALLET_SCHEMA_VERSION);
        if (!ExecOrError(set.c_str(), strError)) return false;
    }

    printf("SQLite wallet opened: %s\n", m_file_path.string().c_str());
    return true;
}

std::unique_ptr<WalletBatch> SQLiteDatabase::MakeBatch(bool /*flush_on_close*/)
{
    return std::make_unique<SQLiteBatch>(*this);
}

bool SQLiteDatabase::Rewrite(const char* /*pszSkip*/)
{
    // SQLite reclaims space and defragments via VACUUM. The wallet erases
    // superseded records (e.g. unencrypted keys after encryption) explicitly,
    // so the pszSkip filter that the Berkeley backend used is unnecessary here.
    if (!m_db)
        return false;
    std::string err;
    if (!ExecOrError("VACUUM;", err)) {
        printf("SQLiteDatabase::Rewrite VACUUM failed: %s\n", err.c_str());
        return false;
    }
    return true;
}

bool SQLiteDatabase::Backup(const std::string& strDest) const
{
    if (!m_db)
        return false;

    sqlite3* pDest = nullptr;
    if (sqlite3_open_v2(strDest.c_str(), &pDest,
                        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) != SQLITE_OK) {
        printf("SQLiteDatabase::Backup cannot open destination %s: %s\n",
               strDest.c_str(), pDest ? sqlite3_errmsg(pDest) : "?");
        if (pDest) sqlite3_close(pDest);
        return false;
    }
#ifndef WIN32
    {
        std::error_code ec;
        fs::permissions(strDest,
                        fs::perms::owner_read | fs::perms::owner_write,
                        fs::perm_options::replace, ec);
        if (ec) {
            printf("SQLiteDatabase::Backup cannot restrict destination permissions: %s\n",
                   ec.message().c_str());
            sqlite3_close(pDest);
            return false;
        }
    }
#endif

    sqlite3_backup* bk = sqlite3_backup_init(pDest, "main", m_db, "main");
    bool ok = false;
    if (bk) {
        sqlite3_backup_step(bk, -1);          // copy entire DB in one shot
        int rc = sqlite3_backup_finish(bk);
        ok = (rc == SQLITE_OK);
        if (!ok)
            printf("SQLiteDatabase::Backup failed: %s\n", sqlite3_errstr(rc));
    } else {
        printf("SQLiteDatabase::Backup init failed: %s\n", sqlite3_errmsg(pDest));
    }
    sqlite3_close(pDest);
    return ok;
}

void SQLiteDatabase::Flush()
{
    // No-op: with synchronous=FULL and rollback journaling, each committed
    // transaction is already durable. (If WAL is ever enabled, checkpoint here.)
}

void SQLiteDatabase::Close()
{
    if (m_db) {
        sqlite3_close(m_db);
        m_db = nullptr;
    }
}

bool SQLiteDatabase::Verify(std::string& strError)
{
    if (!m_db) {
        strError = "SQLite database not open";
        return false;
    }
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(m_db, "PRAGMA integrity_check;", -1, &st, nullptr) != SQLITE_OK) {
        strError = strprintf("integrity_check prepare failed: %s", sqlite3_errmsg(m_db));
        return false;
    }
    bool ok = false;
    if (sqlite3_step(st) == SQLITE_ROW) {
        const unsigned char* res = sqlite3_column_text(st, 0);
        ok = (res && std::strcmp(reinterpret_cast<const char*>(res), "ok") == 0);
        if (!ok)
            strError = strprintf("integrity_check: %s", res ? reinterpret_cast<const char*>(res) : "(null)");
    } else {
        strError = "integrity_check returned no rows";
    }
    sqlite3_finalize(st);
    return ok;
}

// ─── SQLiteBatch ──────────────────────────────────────────────────────────────

SQLiteBatch::SQLiteBatch(SQLiteDatabase& database)
    : m_database(database)
{
    PrepareStatements();
}

bool SQLiteBatch::PrepareStatements()
{
    sqlite3* db = m_database.Handle();
    if (!db)
        return false;

    struct { sqlite3_stmt** out; const char* sql; } stmts[] = {
        { &m_read_stmt,      "SELECT value FROM main WHERE key = ?;" },
        { &m_insert_stmt,    "INSERT OR REPLACE INTO main (key, value) VALUES (?, ?);" },
        { &m_overwrite_stmt, "INSERT INTO main (key, value) VALUES (?, ?);" },
        { &m_delete_stmt,    "DELETE FROM main WHERE key = ?;" },
    };
    for (auto& s : stmts) {
        if (*s.out) continue;
        if (sqlite3_prepare_v2(db, s.sql, -1, s.out, nullptr) != SQLITE_OK) {
            printf("SQLiteBatch: prepare failed for '%s': %s\n", s.sql, sqlite3_errmsg(db));
            return false;
        }
    }
    return true;
}

void SQLiteBatch::Close()
{
    sqlite3_stmt* all[] = { m_read_stmt, m_insert_stmt, m_overwrite_stmt, m_delete_stmt };
    for (auto* st : all)
        if (st) sqlite3_finalize(st);
    m_read_stmt = m_insert_stmt = m_overwrite_stmt = m_delete_stmt = nullptr;
}

bool SQLiteBatch::ReadKey(const KeyBytes& key, ValueBytes& value)
{
    if (!m_read_stmt) return false;
    sqlite3_reset(m_read_stmt);
    sqlite3_clear_bindings(m_read_stmt);
    if (BindBlob(m_read_stmt, 1, key) != SQLITE_OK)
        return false;

    bool found = false;
    if (sqlite3_step(m_read_stmt) == SQLITE_ROW) {
        ColumnBlob(m_read_stmt, 0, value);
        found = true;
    }
    sqlite3_reset(m_read_stmt);
    return found;
}

bool SQLiteBatch::WriteKey(const KeyBytes& key, const ValueBytes& value, bool fOverwrite)
{
    sqlite3_stmt* st = fOverwrite ? m_insert_stmt : m_overwrite_stmt;
    if (!st) return false;
    sqlite3_reset(st);
    sqlite3_clear_bindings(st);
    if (BindBlob(st, 1, key) != SQLITE_OK) return false;
    if (BindBlob(st, 2, value) != SQLITE_OK) return false;

    int rc = sqlite3_step(st);
    sqlite3_reset(st);
    if (rc == SQLITE_DONE)
        return true;
    // Non-overwrite insert hitting an existing key => constraint violation,
    // which mirrors Berkeley's DB_NOOVERWRITE returning false (not an error).
    if (!fOverwrite && (rc == SQLITE_CONSTRAINT))
        return false;
    printf("SQLiteBatch::WriteKey step failed: %s\n", sqlite3_errstr(rc));
    return false;
}

bool SQLiteBatch::EraseKey(const KeyBytes& key)
{
    if (!m_delete_stmt) return false;
    sqlite3_reset(m_delete_stmt);
    sqlite3_clear_bindings(m_delete_stmt);
    if (BindBlob(m_delete_stmt, 1, key) != SQLITE_OK)
        return false;
    int rc = sqlite3_step(m_delete_stmt);
    sqlite3_reset(m_delete_stmt);
    // DONE whether or not a row matched — "key is gone" either way.
    return rc == SQLITE_DONE;
}

bool SQLiteBatch::HasKey(const KeyBytes& key)
{
    if (!m_read_stmt) return false;
    sqlite3_reset(m_read_stmt);
    sqlite3_clear_bindings(m_read_stmt);
    if (BindBlob(m_read_stmt, 1, key) != SQLITE_OK)
        return false;
    bool present = (sqlite3_step(m_read_stmt) == SQLITE_ROW);
    sqlite3_reset(m_read_stmt);
    return present;
}

namespace {

class SQLiteCursor final : public WalletCursor
{
public:
    explicit SQLiteCursor(sqlite3_stmt* stmt) : m_stmt(stmt) {}
    ~SQLiteCursor() override { if (m_stmt) sqlite3_finalize(m_stmt); }

    WalletCursorStatus Next(KeyBytes& key, ValueBytes& value) override
    {
        if (!m_stmt) return WalletCursorStatus::FAIL;
        int rc = sqlite3_step(m_stmt);
        if (rc == SQLITE_DONE) return WalletCursorStatus::DONE;
        if (rc != SQLITE_ROW)  return WalletCursorStatus::FAIL;
        ColumnBlob(m_stmt, 0, key);
        ColumnBlob(m_stmt, 1, value);
        return WalletCursorStatus::MORE;
    }

private:
    sqlite3_stmt* m_stmt;
};

} // namespace

std::unique_ptr<WalletCursor> SQLiteBatch::GetNewCursor()
{
    sqlite3* db = m_database.Handle();
    if (!db) return nullptr;
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db, "SELECT key, value FROM main;", -1, &st, nullptr) != SQLITE_OK) {
        printf("SQLiteBatch::GetNewCursor prepare failed: %s\n", sqlite3_errmsg(db));
        return nullptr;
    }
    return std::make_unique<SQLiteCursor>(st);
}

bool SQLiteBatch::TxnBegin()
{
    return sqlite3_exec(m_database.Handle(), "BEGIN TRANSACTION;", nullptr, nullptr, nullptr) == SQLITE_OK;
}

bool SQLiteBatch::TxnCommit()
{
    return sqlite3_exec(m_database.Handle(), "COMMIT TRANSACTION;", nullptr, nullptr, nullptr) == SQLITE_OK;
}

bool SQLiteBatch::TxnAbort()
{
    return sqlite3_exec(m_database.Handle(), "ROLLBACK TRANSACTION;", nullptr, nullptr, nullptr) == SQLITE_OK;
}
