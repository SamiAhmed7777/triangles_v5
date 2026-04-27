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

#include <filesystem>
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

// True when the configured chain-DB backend is RocksDB.
bool IsRocksDbChainBackend();

// On-disk directory of the chain DB for the configured backend, e.g.
// <datadir>/txleveldb (LevelDB) or <datadir>/rocksdb (RocksDB).
std::filesystem::path GetChainDataDir();

// Remove the chain DB directory for the configured backend. Callers that
// need a fresh DB (-reindex, snapshot load) must invoke this BEFORE
// MakeChainDB() opens the global handle for the first time.
void WipeChainDataDir();

#endif // TRIANGLES_TXDB_H
