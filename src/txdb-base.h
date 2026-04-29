// Copyright (c) 2009-2012 The Bitcoin developers.
// Copyright (c) 2026 The Triangles developers.
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef TRIANGLES_TXDB_BASE_H
#define TRIANGLES_TXDB_BASE_H

#include "main.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

class CScript;
class CTransaction;
class CDiskTxPos;
class CTxIndex;
class CDiskBlockIndex;
class CUtxoEntry;
class CBigNum;

// ----------------------------------------------------------------------------
// Backend-agnostic key/value iterator.
//
// Each CTxDBBase backend returns a std::unique_ptr<CTxDBIteratorBase> from
// NewIterator(). Iterators yield raw serialized key/value bytes; callers
// deserialize using the same SER_DISK / CLIENT_VERSION conventions used by
// CTxDBBase's templated Read/Write paths.
//
// Iterators do NOT see uncommitted writes in an active batch. All current
// iteration sites (block-index scan, address-index range queries, UTXO sum)
// run outside transactions, so this is safe.
// ----------------------------------------------------------------------------
class CTxDBIteratorBase
{
public:
    virtual ~CTxDBIteratorBase() = default;

    virtual void Seek(const std::string& key) = 0;
    virtual bool Valid() const = 0;
    virtual void Next() = 0;
    virtual std::string KeyStr() const = 0;
    virtual std::string ValueStr() const = 0;
};

// ----------------------------------------------------------------------------
// Abstract chain database interface.
//
// All key/value serialization happens in this base class via CDataStream with
// SER_DISK / CLIENT_VERSION. Backends only implement byte-level I/O, so every
// backend produces bit-identical key bytes — required for migration and
// dual-backend parity testing.
//
// Named operations (ReadTxIndex, WriteBlockIndex, etc.) are implemented in
// terms of the templated Read/Write/Erase/Exists, which dispatch to the
// virtual byte-level methods. To add a new backend:
//
//   1. Subclass CTxDBBase.
//   2. Implement Close, TxnBegin/Commit/Abort.
//   3. Implement ReadRaw, WriteRaw, EraseRaw, ExistsRaw.
//   4. Implement NewIterator (return a subclass of CTxDBIteratorBase).
//   5. Implement LoadBlockIndex (still backend-specific in M1; will be
//      extracted to the base in a later phase).
// ----------------------------------------------------------------------------
class CTxDBBase
{
public:
    virtual ~CTxDBBase() = default;

    // Destroys the underlying shared global state accessed by this DB.
    virtual void Close() = 0;

    // Batches (transaction-like atomic groups of writes/deletes).
    virtual bool TxnBegin() = 0;
    virtual bool TxnCommit() = 0;
    virtual bool TxnAbort() = 0;

    bool IsReadOnly() const { return fReadOnly; }

    // Wrapper accessors for backend forwarding (used by CActiveTxDB).
    bool ReadRawBytes(const std::string& key, std::string& value) const { return ReadRaw(key, value); }
    bool WriteRawBytes(const std::string& key, const std::string& value) { return WriteRaw(key, value); }
    bool EraseRawBytes(const std::string& key) { return EraseRaw(key); }
    bool ExistsRawBytes(const std::string& key) const { return ExistsRaw(key); }
    std::unique_ptr<CTxDBIteratorBase> NewRawIterator() const { return NewIterator(); }

    // ── Schema versioning ────────────────────────────────────────────────────
    bool ReadVersion(int& nVersion);
    bool WriteVersion(int nVersion);
    bool ReadDbFormat(int& nDbFormat);
    bool WriteDbFormat(int nDbFormat);

    // ── Tx index ─────────────────────────────────────────────────────────────
    bool ReadTxIndex(uint256 hash, CTxIndex& txindex);
    bool UpdateTxIndex(uint256 hash, const CTxIndex& txindex);
    bool AddTxIndex(const CTransaction& tx, const CDiskTxPos& pos, int nHeight);
    bool EraseTxIndex(const CTransaction& tx);
    bool ContainsTx(uint256 hash);
    bool ReadDiskTx(uint256 hash, CTransaction& tx, CTxIndex& txindex);
    bool ReadDiskTx(uint256 hash, CTransaction& tx);
    bool ReadDiskTx(COutPoint outpoint, CTransaction& tx, CTxIndex& txindex);
    bool ReadDiskTx(COutPoint outpoint, CTransaction& tx);

    // ── Block index ──────────────────────────────────────────────────────────
    bool WriteBlockIndex(const CDiskBlockIndex& blockindex);

    // ── Best chain / checkpoint metadata ─────────────────────────────────────
    bool ReadHashBestChain(uint256& hashBestChain);
    bool WriteHashBestChain(uint256 hashBestChain);
    bool ReadAddressIndexBestChain(uint256& hashBestChain);
    bool WriteAddressIndexBestChain(uint256 hashBestChain);
    bool ReadAddressIndexStartHeight(int& nHeight);
    bool WriteAddressIndexStartHeight(int nHeight);
    bool ReadBestInvalidTrust(CBigNum& bnBestInvalidTrust);
    bool WriteBestInvalidTrust(CBigNum bnBestInvalidTrust);
    bool ReadSyncCheckpoint(uint256& hashCheckpoint);
    bool WriteSyncCheckpoint(uint256 hashCheckpoint);
    bool ReadCheckpointPubKey(std::string& strPubKey);
    bool WriteCheckpointPubKey(const std::string& strPubKey);

    virtual bool LoadBlockIndex() = 0;

    // ── Address index ────────────────────────────────────────────────────────
    bool ReadAddressBalance(int nType, const uint160& hashBytes, int64_t& nBalance);
    bool WriteAddressBalance(int nType, const uint160& hashBytes, int64_t nBalance);
    bool ReadAddressUtxo(int nType, const uint160& hashBytes, const uint256& txhash,
                         int nIndex, int64_t& nValue, int& nHeight);
    bool WriteAddressUtxo(int nType, const uint160& hashBytes, const uint256& txhash,
                          int nIndex, int64_t nValue, int nHeight, const CScript& script);
    bool EraseAddressUtxo(int nType, const uint160& hashBytes, const uint256& txhash,
                          int nIndex);
    bool WriteAddressTxId(int nType, const uint160& hashBytes, int nHeight,
                          int nTxIndex, const uint256& txhash);
    bool EraseAddressTxId(int nType, const uint160& hashBytes, int nHeight,
                          int nTxIndex, const uint256& txhash);
    bool GetAddressUtxos(int nType, const uint160& hashBytes,
                         std::vector<std::pair<COutPoint, std::pair<int64_t, int> > >& vUtxos);
    bool GetAddressTxIds(int nType, const uint160& hashBytes, int nStartHeight,
                         int nEndHeight, std::vector<uint256>& vTxIds);

    // ── UTXO set ─────────────────────────────────────────────────────────────
    bool ReadUtxo(const uint256& hash, unsigned int n, CUtxoEntry& entry);
    bool WriteUtxo(const uint256& hash, unsigned int n, const CUtxoEntry& entry);
    bool EraseUtxo(const uint256& hash, unsigned int n);
    bool HaveUtxo(const uint256& hash, unsigned int n);
    int64_t SumUtxoValues(int& nCount);

protected:
    bool fReadOnly = false;

    // Byte-level I/O — backends implement these.
    virtual bool ReadRaw(const std::string& key, std::string& value) const = 0;
    virtual bool WriteRaw(const std::string& key, const std::string& value) = 0;
    virtual bool EraseRaw(const std::string& key) = 0;
    virtual bool ExistsRaw(const std::string& key) const = 0;
    virtual std::unique_ptr<CTxDBIteratorBase> NewIterator() const = 0;

    // Templated Read/Write/Erase/Exists are non-virtual (templates can't be
    // virtual in C++) — they serialize and dispatch to the byte-level virtuals.
    template<typename K, typename T>
    bool Read(const K& key, T& value) const
    {
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.reserve(1000);
        ssKey << key;
        std::string strValue;
        if (!ReadRaw(ssKey.str(), strValue))
            return false;
        try {
            CDataStream ssValue(strValue.data(),
                                strValue.data() + strValue.size(),
                                SER_DISK, CLIENT_VERSION);
            ssValue >> value;
        } catch (std::exception&) {
            return false;
        }
        return true;
    }

    template<typename K, typename T>
    bool Write(const K& key, const T& value)
    {
        if (fReadOnly)
            assert(!"Write called on database in read-only mode");
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.reserve(1000);
        ssKey << key;
        CDataStream ssValue(SER_DISK, CLIENT_VERSION);
        ssValue.reserve(10000);
        ssValue << value;
        return WriteRaw(ssKey.str(), ssValue.str());
    }

    template<typename K>
    bool Erase(const K& key)
    {
        if (fReadOnly)
            assert(!"Erase called on database in read-only mode");
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.reserve(1000);
        ssKey << key;
        return EraseRaw(ssKey.str());
    }

    template<typename K>
    bool Exists(const K& key) const
    {
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.reserve(1000);
        ssKey << key;
        return ExistsRaw(ssKey.str());
    }
};

#endif // TRIANGLES_TXDB_BASE_H
