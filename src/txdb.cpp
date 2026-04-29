// Copyright (c) 2026 The Triangles developers.
// Distributed under the MIT/X11 software license.

#include "txdb.h"

#include "util.h"

bool UseRocksDbBackend()
{
#ifdef BUILD_ROCKSDB
    return GetBoolArg("-rocksdb", false);
#else
    return false;
#endif
}

const char* GetActiveChainDbBackendName()
{
#ifdef BUILD_ROCKSDB
    return UseRocksDbBackend() ? "rocksdb" : "leveldb";
#else
    return "leveldb";
#endif
}

const char* GetActiveChainDbDirName()
{
#ifdef BUILD_ROCKSDB
    return UseRocksDbBackend() ? "rocksdb" : "txleveldb";
#else
    return "txleveldb";
#endif
}

CActiveTxDB::CActiveTxDB(const char* pszMode)
{
#ifdef BUILD_ROCKSDB
    if (UseRocksDbBackend())
        impl.reset(new CRocksTxDB(pszMode));
    else
        impl.reset(new CTxDB(pszMode));
#else
    impl.reset(new CTxDB(pszMode));
#endif
}

CActiveTxDB::~CActiveTxDB() = default;

void CActiveTxDB::Close() { impl->Close(); }
bool CActiveTxDB::TxnBegin() { return impl->TxnBegin(); }
bool CActiveTxDB::TxnCommit() { return impl->TxnCommit(); }
bool CActiveTxDB::TxnAbort() { return impl->TxnAbort(); }
bool CActiveTxDB::LoadBlockIndex() { return impl->LoadBlockIndex(); }

bool CActiveTxDB::ReadRaw(const std::string& key, std::string& value) const { return impl->ReadRawBytes(key, value); }
bool CActiveTxDB::WriteRaw(const std::string& key, const std::string& value) { return impl->WriteRawBytes(key, value); }
bool CActiveTxDB::EraseRaw(const std::string& key) { return impl->EraseRawBytes(key); }
bool CActiveTxDB::ExistsRaw(const std::string& key) const { return impl->ExistsRawBytes(key); }
std::unique_ptr<CTxDBIteratorBase> CActiveTxDB::NewIterator() const { return impl->NewRawIterator(); }
