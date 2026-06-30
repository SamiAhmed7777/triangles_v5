// Copyright (c) 2026 The Triangles developers.
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// Berkeley-only wallet recovery helpers — moved out of CWalletDB so the
// mainline wallet code path (SQLite via the typed batch seam) does not have
// to include <db_cxx.h>.
//
// These functions operate directly on bitdb / CDB and are used only:
//   * during startup, before the wallet migration hook (on a possible BDB
//     wallet.dat), and
//   * on the .bdb.bak copy that migration leaves behind, for diagnostic /
//     manual recovery if migration ever needs investigation.
//
// They are intentionally NOT methods of CWalletDB — that class is on the
// SQLite seam now and has no Berkeley state.

#ifndef TRIANGLES_WALLETDB_RECOVER_H
#define TRIANGLES_WALLETDB_RECOVER_H

#include "db.h"
#include <string>

// Aggressive salvage of a Berkeley wallet.dat file. Moves the file aside to
// wallet.<timestamp>.bak, then walks the salvaged records and re-writes them
// into a fresh Berkeley database at the original path.
//
// If fOnlyKeys is true, only key-type records are kept (used for recovery
// when transaction history is corrupt). Returns true on success.
bool BerkeleyRecoverWallet(CDBEnv& dbenv, std::string filename, bool fOnlyKeys);
inline bool BerkeleyRecoverWallet(CDBEnv& dbenv, std::string filename)
{
    return BerkeleyRecoverWallet(dbenv, filename, false);
}

// Strip every "tx" record from a Berkeley wallet.dat, leaving keys and other
// metadata intact. A rescan rebuilds the transaction list from the chain.
// Used for `-zapwallettxes` on legacy (pre-migration) wallets.
bool BerkeleyZapWalletTx(const std::string& strWalletFile);

#endif // TRIANGLES_WALLETDB_RECOVER_H