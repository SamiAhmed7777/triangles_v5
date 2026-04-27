// Copyright (c) 2026 The Triangles developers.
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "txdb.h"
#include "util.h"

#include <filesystem>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

namespace {

// Pick the backend once per process. -chaindb is a startup flag; switching at
// runtime would require reopening every CTxDB instance, which the codebase
// doesn't currently support. We cache the resolved choice so subsequent
// MakeChainDB calls don't re-parse the argument.
enum class ChainDbKind { LevelDB, RocksDB };

ChainDbKind ResolveChainDbKind()
{
    static const ChainDbKind kKind = []() {
        std::string s = GetArg("-chaindb", std::string("leveldb"));
        for (auto& c : s) c = std::tolower(static_cast<unsigned char>(c));

        if (s == "leveldb")
            return ChainDbKind::LevelDB;
        if (s == "rocksdb")
            return ChainDbKind::RocksDB;

        throw std::runtime_error(
            "-chaindb=" + s + " is not a recognized backend. "
            "Valid values: leveldb, rocksdb.");
    }();
    return kKind;
}

} // anonymous namespace

std::unique_ptr<CTxDBBase> MakeChainDB(const char* pszMode)
{
    switch (ResolveChainDbKind()) {
    case ChainDbKind::LevelDB:
        return std::unique_ptr<CTxDBBase>(new CTxDB(pszMode));
    case ChainDbKind::RocksDB:
        return std::unique_ptr<CTxDBBase>(new CRocksTxDB(pszMode));
    }
    // Unreachable — ResolveChainDbKind throws on bad input.
    return nullptr;
}

bool IsRocksDbChainBackend()
{
    return ResolveChainDbKind() == ChainDbKind::RocksDB;
}

std::filesystem::path GetChainDataDir()
{
    switch (ResolveChainDbKind()) {
    case ChainDbKind::LevelDB: return GetDataDir() / "txleveldb";
    case ChainDbKind::RocksDB: return GetDataDir() / "rocksdb";
    }
    return GetDataDir() / "txleveldb"; // unreachable
}

void WipeChainDataDir()
{
    fs::path p = GetChainDataDir();
    if (fs::exists(p))
        fs::remove_all(p);
}
