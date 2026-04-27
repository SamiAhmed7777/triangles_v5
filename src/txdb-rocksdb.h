// Copyright (c) 2026 The Triangles developers.
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef TRIANGLES_TXDB_ROCKSDB_H
#define TRIANGLES_TXDB_ROCKSDB_H

#include "txdb-base.h"

#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <rocksdb/write_batch.h>

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

protected:
    bool ReadRaw(const std::string& key, std::string& value) const override;
    bool WriteRaw(const std::string& key, const std::string& value) override;
    bool EraseRaw(const std::string& key) override;
    bool ExistsRaw(const std::string& key) const override;
    std::unique_ptr<CTxDBIteratorBase> NewIterator() const override;

private:
    rocksdb::DB* pdb;                   // Points to the global instance.
    rocksdb::WriteBatch* activeBatch;   // When non-NULL, writes/deletes go here.
    rocksdb::Options options;
    int nVersion;

    bool ScanBatch(const std::string& key, std::string* value, bool* deleted) const;
};

#endif // TRIANGLES_TXDB_ROCKSDB_H
