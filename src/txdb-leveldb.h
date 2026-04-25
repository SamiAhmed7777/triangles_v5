// Copyright (c) 2009-2012 The Bitcoin developers.
// Authored by Google, Inc.
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef TRIANGLES_LEVELDB_H
#define TRIANGLES_LEVELDB_H

#include "txdb-base.h"

#include <leveldb/db.h>
#include <leveldb/write_batch.h>

// LevelDB backend for the chain database.
//
// Cheap to construct/destruct: every instance shares a single global
// leveldb::DB pointer, opened lazily on first use. Most of the codebase
// instantiates a CTxDB on the stack for short-lived operations.
//
// The protected templated Read/Write/Erase/Exists live in CTxDBBase and
// dispatch to ReadRaw/WriteRaw/EraseRaw/ExistsRaw below, which handle the
// active-batch logic so reads-after-writes within an open batch see their
// own pending changes.
class CTxDB final : public CTxDBBase
{
public:
    CTxDB(const char* pszMode = "r+");
    ~CTxDB() override {
        delete activeBatch;
    }

    void Close() override;

    bool TxnBegin() override;
    bool TxnCommit() override;
    bool TxnAbort() override
    {
        delete activeBatch;
        activeBatch = NULL;
        return true;
    }

    bool LoadBlockIndex() override;

protected:
    bool ReadRaw(const std::string& key, std::string& value) const override;
    bool WriteRaw(const std::string& key, const std::string& value) override;
    bool EraseRaw(const std::string& key) override;
    bool ExistsRaw(const std::string& key) const override;
    std::unique_ptr<CTxDBIteratorBase> NewIterator() const override;

private:
    leveldb::DB* pdb;                  // Points to the global instance.
    leveldb::WriteBatch* activeBatch;  // When non-NULL, writes/deletes go here.
    leveldb::Options options;
    int nVersion;

    // Returns true and sets (value,false) if activeBatch contains the given
    // key, or leaves value alone and sets deleted=true if activeBatch contains
    // a delete for it.
    bool ScanBatch(const std::string& key, std::string* value, bool* deleted) const;

    bool LoadBlockIndexGuts();
};

#endif // TRIANGLES_LEVELDB_H
