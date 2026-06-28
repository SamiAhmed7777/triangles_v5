// Copyright (c) 2026 The Triangles developers.
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef TRIANGLES_TXDB_ROCKSDB_H
#define TRIANGLES_TXDB_ROCKSDB_H

#include "txdb-base.h"

#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <rocksdb/write_batch.h>
#include <rocksdb/utilities/db_ttl.h>

// RocksDB backend for the chain database.
//
// Mirrors CTxDB (LevelDB) for byte-level compatibility. CTxDBBase owns all
// key serialization, so keys produced by this backend are bit-identical to
// the LevelDB backend. That property is what lets the M1.4 dual-backend
// parity harness verify equivalence.
//
// Data lives under <datadir>/rocksdb/, separate from <datadir>/txleveldb/,
// so both backends can coexist for migration and side-by-side testing.
class CRocksTxDB final : public CTxDBBase
{
public:
    CRocksTxDB(const char* pszMode = "r+");
    ~CRocksTxDB() override;

    void Close() override;

    bool TxnBegin() override;
    bool TxnCommit() override;
    bool TxnAbort() override;

    bool LoadBlockIndex() override;

    // Write a raw serialized key/value pair, bypassing the typed Write<>()
    // overloads. Intended for the chaindb migration utility, which carries
    // bytes directly across from a CTxDB (LevelDB) iterator. Honors the
    // active write batch if one is open.
    bool WriteRawRecordForMigration(const std::string& key, const std::string& value)
    {
        return WriteRaw(key, value);
    }

    std::unique_ptr<CTxDBIteratorBase> NewIterator() const override;

    // ─── Test-only friend accessor ──────────────────────────────────────────
    // test_chaindb_runtime exercises the protected raw methods (ReadRaw /
    // WriteRaw / EraseRaw / ExistsRaw) directly to verify the wrapper layer
    // that the daemon uses at runtime when launched with -chaindb=rocksdb.
    // We don't widen the public API just for the test — instead the test
    // declares a ChainDbRuntimeTestAccessor struct that this class befriends,
    // giving it the same access the class itself has. White-box test pattern,
    // zero impact on production callers.
    friend struct ChainDbRuntimeTestAccessor;

protected:
    bool ReadRaw(const std::string& key, std::string& value) const override;
    bool WriteRaw(const std::string& key, const std::string& value) override;
    bool EraseRaw(const std::string& key) override;
    bool ExistsRaw(const std::string& key) const override;

private:
    rocksdb::DB* pdb;                   // Points to the global instance.
    rocksdb::WriteBatch* activeBatch;   // When non-NULL, writes/deletes go here.
    rocksdb::Options options;
    int nVersion;

    // ─── Column family support ──────────────────────────────────────────────
    // Data is split into CFs for independent compaction and caching.
    // cf_handles[0] is always the default CF (for backward compatibility
    // with pre-CF databases that have all data in "default").
    enum CfId : int { CF_DEFAULT = 0, CF_BLOCKINDEX, CF_TXINDEX, CF_UTXO, CF_ADDRINDEX, CF_COUNT };
    rocksdb::ColumnFamilyHandle* cf_handles[CF_COUNT] = {};
    bool cf_enabled = false;  // True if CFs were created/opened successfully

    // Route a key to the correct column family handle based on its prefix.
    // Falls back to CF_DEFAULT for keys that don't match any known prefix
    // (metadata like "version", "hashBestChain", etc.) or if CFs aren't enabled.
    rocksdb::ColumnFamilyHandle* GetCF(const std::string& key) const;

    // Parallel record of every pending write (value) or delete (nullopt) on
    // activeBatch. Used by ScanBatch to answer "is this key already in the
    // active batch?" without iterating the WriteBatch via Handler — Ubuntu's
    // librocksdb-dev hides typeinfo for rocksdb::WriteBatch::Handler so a
    // subclass-based scan fails to link there.
    std::unordered_map<std::string, std::optional<std::string>> pendingBatch;

    bool ScanBatch(const std::string& key, std::string* value, bool* deleted) const;
};

#endif // TRIANGLES_TXDB_ROCKSDB_H
