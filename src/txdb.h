// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Copyright (c) 2026 The Triangles developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#ifndef TRIANGLES_TXDB_H
#define TRIANGLES_TXDB_H

#include "txdb-base.h"
#include "txdb-leveldb.h"
#include "txdb-rocksdb.h"

#include <memory>

// Factory: returns a chain-database handle whose concrete backend is chosen
// by the -chaindb command-line argument:
//
//   -chaindb=leveldb  (default — pending Phase-4 retirement)
//   -chaindb=rocksdb
//
// Callers receive a CTxDBBase*, so the rest of the codebase stays
// backend-agnostic. Mode strings ("r", "r+", "cr+") match the pre-existing
// CTxDB constructor convention.
std::unique_ptr<CTxDBBase> MakeChainDB(const char* pszMode = "r+");

// True when the configured chain-DB backend is RocksDB. Used by code paths
// (e.g. UtxoSnapshot::LoadSnapshot) that haven't yet been ported off direct
// LevelDB calls — they error out cleanly instead of corrupting state.
bool IsRocksDbChainBackend();

#endif // TRIANGLES_TXDB_H
