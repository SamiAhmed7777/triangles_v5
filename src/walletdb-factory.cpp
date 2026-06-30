// Copyright (c) 2026 The Triangles developers.
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "walletdb-base.h"
#include "walletdb-sqlite.h"
#include "util.h"

#include <cctype>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

WalletDbKind ResolveWalletDbKind()
{
    // SQLite is the default wallet backend. Berkeley DB is retained for one
    // release as a fallback (-walletdb=bdb) and as the migration source.
    std::string s = GetArg("-walletdb", std::string("sqlite"));
    for (auto& c : s) c = std::tolower(static_cast<unsigned char>(c));

    if (s == "sqlite")
        return WalletDbKind::SQLite;
    if (s == "bdb" || s == "berkeley")
        return WalletDbKind::Berkeley;

    throw std::runtime_error(
        "-walletdb=" + s + " is not a recognized wallet backend. "
        "Valid values: sqlite, bdb.");
}

std::unique_ptr<WalletDatabase> MakeWalletDatabase(const std::string& strFilename,
                                                   std::string& strError)
{
    const fs::path path = GetDataDir() / strFilename;

    switch (ResolveWalletDbKind()) {
    case WalletDbKind::SQLite: {
        auto db = std::make_unique<SQLiteDatabase>(path);
        if (!db->Open(strError))
            return nullptr;
        return db;
    }
    case WalletDbKind::Berkeley:
        // The Berkeley backend is still served by the legacy CWalletDB/CDB code
        // path. The thin BerkeleyDatabase adapter that plugs the existing
        // CDBEnv/CDB into this seam is added during CWalletDB integration; see
        // WALLET-SQLITE-MIGRATION.md. Until then, selecting -walletdb=bdb keeps
        // the original code path rather than routing through MakeWalletDatabase.
        strError = "Berkeley backend uses the legacy wallet path; not served by MakeWalletDatabase yet.";
        return nullptr;
    }
    return nullptr;  // unreachable
}
