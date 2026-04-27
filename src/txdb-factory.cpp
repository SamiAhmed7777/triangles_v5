// Copyright (c) 2026 The Triangles developers.
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "txdb.h"
#include "util.h"

#include <stdexcept>
#include <string>

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

        if (s == "rocksdb") {
#ifdef BUILD_ROCKSDB
            return ChainDbKind::RocksDB;
#else
            throw std::runtime_error(
                "-chaindb=rocksdb requested but this binary was built without "
                "BUILD_ROCKSDB. Rebuild with -DBUILD_ROCKSDB=ON, or use "
                "-chaindb=leveldb.");
#endif
        }

        throw std::runtime_error(
            "-chaindb=" + s + " is not a recognized backend. "
            "Valid values: leveldb"
#ifdef BUILD_ROCKSDB
            ", rocksdb"
#endif
            ".");
    }();
    return kKind;
}

} // anonymous namespace

std::unique_ptr<CTxDBBase> MakeChainDB(const char* pszMode)
{
    switch (ResolveChainDbKind()) {
    case ChainDbKind::LevelDB:
        return std::unique_ptr<CTxDBBase>(new CTxDB(pszMode));
#ifdef BUILD_ROCKSDB
    case ChainDbKind::RocksDB:
        return std::unique_ptr<CTxDBBase>(new CRocksTxDB(pszMode));
#endif
    }
    // Unreachable — ResolveChainDbKind throws on bad input.
    return nullptr;
}
