// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#ifndef TRIANGLES_TXDB_H
#define TRIANGLES_TXDB_H

#include "txdb-leveldb.h"
#ifdef BUILD_ROCKSDB
#include "txdb-rocksdb.h"
#endif

#include <memory>

bool UseRocksDbBackend();
const char* GetActiveChainDbBackendName();
const char* GetActiveChainDbDirName();

class CActiveTxDB final : public CTxDBBase
{
public:
    explicit CActiveTxDB(const char* pszMode = "r+");
    ~CActiveTxDB() override;

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
    std::unique_ptr<CTxDBBase> impl;
};

#endif  // TRIANGLES_TXDB_H
