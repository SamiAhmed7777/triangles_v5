// Copyright (c) 2026 The Triangles developers.
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "walletmigrate.h"
#include "walletdb-sqlite.h"
#include "util.h"

#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

#include <db_cxx.h>

namespace fs = std::filesystem;

bool IsSQLiteFile(const fs::path& path)
{
    std::error_code ec;
    if (!fs::exists(path, ec) || fs::file_size(path, ec) < 16)
        return false;
    std::ifstream in(path, std::ios::binary);
    char hdr[16] = {};
    in.read(hdr, sizeof(hdr));
    if (!in)
        return false;
    // SQLite database files always start with this exact 16-byte string,
    // including the trailing NUL. Berkeley DB files do not.
    static const char kMagic[16] = {'S','Q','L','i','t','e',' ','f','o','r','m','a','t',' ','3','\0'};
    return std::memcmp(hdr, kMagic, 16) == 0;
}

namespace {

// Count rows currently in the SQLite "main" table.
bool SQLiteRowCount(SQLiteDatabase& db, int64_t& nOut, std::string& strError)
{
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db.Handle(), "SELECT COUNT(*) FROM main;", -1, &st, nullptr) != SQLITE_OK) {
        strError = strprintf("count prepare failed: %s", sqlite3_errmsg(db.Handle()));
        return false;
    }
    bool ok = false;
    if (sqlite3_step(st) == SQLITE_ROW) {
        nOut = sqlite3_column_int64(st, 0);
        ok = true;
    } else {
        strError = "count query returned no rows";
    }
    sqlite3_finalize(st);
    return ok;
}

} // namespace

bool MaybeMigrateBerkeleyWalletToSQLite(const fs::path& walletPath, std::string& strError)
{
    strError.clear();

    std::error_code ec;
    if (!fs::exists(walletPath, ec))
        return true;  // fresh install — the SQLite backend will create it
    if (IsSQLiteFile(walletPath))
        return true;  // already migrated / already SQLite

    const fs::path dir  = walletPath.parent_path();
    const std::string file = walletPath.filename().string();
    const fs::path tmpPath = dir / (file + ".sqlite.tmp");
    const fs::path bakPath = dir / (file + ".bdb.bak");

    printf("Wallet migration: converting Berkeley %s to SQLite...\n", walletPath.string().c_str());

    fs::remove(tmpPath, ec);  // clear any stale temp from a prior aborted run

    int64_t nCopied = 0;

    // ── Read side: a private, read-only Berkeley environment over the wallet
    // directory, then the "main" sub-database (matches CDB::CDB's open call). ──
    DbEnv env(0u);
    env.set_error_stream(&std::cerr);
    env.set_cachesize(0, 1 << 20, 1);  // 1 MiB cache is plenty for sequential read
    u_int32_t envFlags = DB_CREATE | DB_INIT_MPOOL | DB_PRIVATE;
    if (env.open(dir.string().c_str(), envFlags, 0) != 0) {
        strError = "migration: cannot open Berkeley environment on wallet directory";
        return false;
    }

    bool ok = false;
    {
        Db db(&env, 0);
        if (db.open(nullptr, file.c_str(), "main", DB_BTREE, DB_RDONLY, 0) != 0) {
            strError = "migration: cannot open Berkeley wallet (is it a valid wallet.dat?)";
            env.close(0);
            return false;
        }

        // ── Write side: fresh SQLite database in the temp file. ──
        SQLiteDatabase sqlite(tmpPath);
        std::string sqlErr;
        if (!sqlite.Open(sqlErr)) {
            strError = "migration: cannot create SQLite wallet: " + sqlErr;
            db.close(0);
            env.close(0);
            return false;
        }

        auto batch = sqlite.MakeBatch();
        if (!batch || !batch->TxnBegin()) {
            strError = "migration: cannot begin SQLite transaction";
            db.close(0);
            env.close(0);
            return false;
        }

        Dbc* pcursor = nullptr;
        if (db.cursor(nullptr, &pcursor, 0) != 0) {
            strError = "migration: cannot open Berkeley cursor";
            batch->TxnAbort();
            db.close(0);
            env.close(0);
            return false;
        }

        Dbt datKey, datValue;  // BDB-owned buffers, valid until the next get()
        int ret;
        bool writeFailed = false;
        while ((ret = pcursor->get(&datKey, &datValue, DB_NEXT)) == 0) {
            const unsigned char* kp = static_cast<const unsigned char*>(datKey.get_data());
            const unsigned char* vp = static_cast<const unsigned char*>(datValue.get_data());
            KeyBytes   key(kp, kp + datKey.get_size());
            ValueBytes val(vp, vp + datValue.get_size());
            if (!batch->WriteKey(key, val, /*fOverwrite=*/true)) {
                writeFailed = true;
                break;
            }
            ++nCopied;
        }
        pcursor->close();

        if (writeFailed || (ret != DB_NOTFOUND && ret != 0)) {
            strError = strprintf("migration: copy aborted after %lld records (bdb get=%d)",
                                 (long long)nCopied, ret);
            batch->TxnAbort();
            db.close(0);
            env.close(0);
            return false;
        }

        if (!batch->TxnCommit()) {
            strError = "migration: SQLite commit failed";
            db.close(0);
            env.close(0);
            return false;
        }

        // ── Verify the destination row count matches what we copied. ──
        int64_t nDst = -1;
        if (!SQLiteRowCount(sqlite, nDst, strError)) {
            db.close(0);
            env.close(0);
            return false;
        }
        if (nDst != nCopied) {
            strError = strprintf("migration: record count mismatch (copied=%lld sqlite=%lld)",
                                 (long long)nCopied, (long long)nDst);
            db.close(0);
            env.close(0);
            return false;
        }

        batch.reset();
        sqlite.Close();
        db.close(0);
        ok = true;
    }
    env.close(0);

    if (!ok) {
        fs::remove(tmpPath, ec);
        return false;
    }

    // ── Atomic-ish swap: back up the Berkeley original, then move SQLite in. ──
    fs::rename(walletPath, bakPath, ec);
    if (ec) {
        strError = strprintf("migration: cannot back up Berkeley wallet to %s: %s",
                             bakPath.string().c_str(), ec.message().c_str());
        fs::remove(tmpPath, ec);
        return false;
    }
    fs::rename(tmpPath, walletPath, ec);
    if (ec) {
        // Roll the original back into place so the wallet is never left missing.
        std::error_code ec2;
        fs::rename(bakPath, walletPath, ec2);
        strError = strprintf("migration: cannot move SQLite wallet into place: %s",
                             ec.message().c_str());
        fs::remove(tmpPath, ec2);
        return false;
    }

    printf("Wallet migration: complete. %lld records migrated to SQLite. "
           "Berkeley original preserved at %s\n",
           (long long)nCopied, bakPath.string().c_str());
    return true;
}
