// Copyright (c) 2026 The Triangles developers.
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef TRIANGLES_WALLETMIGRATE_H
#define TRIANGLES_WALLETMIGRATE_H

#include <filesystem>
#include <string>

// Migrate a Berkeley DB wallet (wallet.dat) to a SQLite wallet of the same
// name, IN PLACE and NON-DESTRUCTIVELY:
//
//   1. If walletPath does not exist, or is already a SQLite database, there is
//      nothing to do — returns true.
//   2. Otherwise the Berkeley records are copied verbatim (raw key/value bytes)
//      into a fresh SQLite database written to a temporary file.
//   3. The record count is verified to match.
//   4. The original Berkeley file is renamed to "<name>.bdb.bak" (kept as a
//      fallback, never deleted), and the SQLite file is moved into place as
//      "<name>".
//
// On any failure the original Berkeley wallet is left exactly as it was and the
// temporary SQLite file is removed; strError describes the problem.
bool MaybeMigrateBerkeleyWalletToSQLite(const std::filesystem::path& walletPath,
                                        std::string& strError);

// True if the file begins with the SQLite format-3 magic header.
bool IsSQLiteFile(const std::filesystem::path& path);

#endif // TRIANGLES_WALLETMIGRATE_H
