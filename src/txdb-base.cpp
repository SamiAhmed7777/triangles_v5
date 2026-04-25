// Copyright (c) 2009-2012 The Bitcoin developers.
// Copyright (c) 2026 The Triangles developers.
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "txdb-base.h"

#include "addressindex.h"
#include "main.h"
#include "sync.h"

#include <unordered_map>

using namespace std;

// ============================================================================
// Schema versioning
// ============================================================================
bool CTxDBBase::ReadVersion(int& nVersion)
{
    nVersion = 0;
    return Read(string("version"), nVersion);
}

bool CTxDBBase::WriteVersion(int nVersion)
{
    return Write(string("version"), nVersion);
}

bool CTxDBBase::ReadDbFormat(int& nDbFormat)
{
    nDbFormat = 1;
    return Read(string("dbformat"), nDbFormat);
}

bool CTxDBBase::WriteDbFormat(int nDbFormat)
{
    return Write(string("dbformat"), nDbFormat);
}

// ============================================================================
// Tx index
// ============================================================================
bool CTxDBBase::ReadTxIndex(uint256 hash, CTxIndex& txindex)
{
    assert(!fClient);
    txindex.SetNull();
    return Read(make_pair(string("tx"), hash), txindex);
}

bool CTxDBBase::UpdateTxIndex(uint256 hash, const CTxIndex& txindex)
{
    assert(!fClient);
    return Write(make_pair(string("tx"), hash), txindex);
}

bool CTxDBBase::AddTxIndex(const CTransaction& tx, const CDiskTxPos& pos, int nHeight)
{
    assert(!fClient);
    uint256 hash = tx.GetHash();
    CTxIndex txindex(pos, tx.vout.size());
    return Write(make_pair(string("tx"), hash), txindex);
}

bool CTxDBBase::EraseTxIndex(const CTransaction& tx)
{
    assert(!fClient);
    uint256 hash = tx.GetHash();
    return Erase(make_pair(string("tx"), hash));
}

bool CTxDBBase::ContainsTx(uint256 hash)
{
    assert(!fClient);
    return Exists(make_pair(string("tx"), hash));
}

bool CTxDBBase::ReadDiskTx(uint256 hash, CTransaction& tx, CTxIndex& txindex)
{
    assert(!fClient);
    tx.SetNull();
    if (!ReadTxIndex(hash, txindex))
        return false;
    return tx.ReadFromDisk(txindex.pos);
}

bool CTxDBBase::ReadDiskTx(uint256 hash, CTransaction& tx)
{
    CTxIndex txindex;
    return ReadDiskTx(hash, tx, txindex);
}

bool CTxDBBase::ReadDiskTx(COutPoint outpoint, CTransaction& tx, CTxIndex& txindex)
{
    return ReadDiskTx(outpoint.hash, tx, txindex);
}

bool CTxDBBase::ReadDiskTx(COutPoint outpoint, CTransaction& tx)
{
    CTxIndex txindex;
    return ReadDiskTx(outpoint.hash, tx, txindex);
}

// ============================================================================
// Block index
// ============================================================================
bool CTxDBBase::WriteBlockIndex(const CDiskBlockIndex& blockindex)
{
    return Write(make_pair(string("blockindex"), blockindex.GetBlockHash()), blockindex);
}

// ============================================================================
// Best chain / checkpoint metadata
// ============================================================================
bool CTxDBBase::ReadHashBestChain(uint256& hashBestChain)
{
    return Read(string("hashBestChain"), hashBestChain);
}

bool CTxDBBase::WriteHashBestChain(uint256 hashBestChain)
{
    return Write(string("hashBestChain"), hashBestChain);
}

bool CTxDBBase::ReadAddressIndexBestChain(uint256& hashBestChain)
{
    return Read(string("addressIndexBestChain"), hashBestChain);
}

bool CTxDBBase::WriteAddressIndexBestChain(uint256 hashBestChain)
{
    return Write(string("addressIndexBestChain"), hashBestChain);
}

bool CTxDBBase::ReadAddressIndexStartHeight(int& nHeight)
{
    return Read(string("addressIndexStartHeight"), nHeight);
}

bool CTxDBBase::WriteAddressIndexStartHeight(int nHeight)
{
    return Write(string("addressIndexStartHeight"), nHeight);
}

bool CTxDBBase::ReadBestInvalidTrust(CBigNum& bnBestInvalidTrust)
{
    return Read(string("bnBestInvalidTrust"), bnBestInvalidTrust);
}

bool CTxDBBase::WriteBestInvalidTrust(CBigNum bnBestInvalidTrust)
{
    return Write(string("bnBestInvalidTrust"), bnBestInvalidTrust);
}

bool CTxDBBase::ReadSyncCheckpoint(uint256& hashCheckpoint)
{
    return Read(string("hashSyncCheckpoint"), hashCheckpoint);
}

bool CTxDBBase::WriteSyncCheckpoint(uint256 hashCheckpoint)
{
    return Write(string("hashSyncCheckpoint"), hashCheckpoint);
}

bool CTxDBBase::ReadCheckpointPubKey(string& strPubKey)
{
    return Read(string("strCheckpointPubKey"), strPubKey);
}

bool CTxDBBase::WriteCheckpointPubKey(const string& strPubKey)
{
    return Write(string("strCheckpointPubKey"), strPubKey);
}

// ============================================================================
// Address index
// ============================================================================
bool CTxDBBase::ReadAddressBalance(int nType, const uint160& hashBytes, int64_t& nBalance)
{
    return Read(make_pair(string("addrbal"), CAddressBalanceKey(nType, hashBytes)), nBalance);
}

bool CTxDBBase::WriteAddressBalance(int nType, const uint160& hashBytes, int64_t nBalance)
{
    return Write(make_pair(string("addrbal"), CAddressBalanceKey(nType, hashBytes)), nBalance);
}

bool CTxDBBase::ReadAddressUtxo(int nType, const uint160& hashBytes,
                                const uint256& txhash, int nIndex,
                                int64_t& nValue, int& nHeight)
{
    CAddressUtxoValue val;
    if (!Read(make_pair(string("addrutxo"),
                        CAddressUtxoKey(nType, hashBytes, txhash, nIndex)), val))
        return false;
    nValue = val.nValue;
    nHeight = val.nHeight;
    return true;
}

bool CTxDBBase::WriteAddressUtxo(int nType, const uint160& hashBytes,
                                 const uint256& txhash, int nIndex,
                                 int64_t nValue, int nHeight, const CScript& script)
{
    return Write(make_pair(string("addrutxo"),
                           CAddressUtxoKey(nType, hashBytes, txhash, nIndex)),
                 CAddressUtxoValue(nValue, nHeight, script));
}

bool CTxDBBase::EraseAddressUtxo(int nType, const uint160& hashBytes,
                                 const uint256& txhash, int nIndex)
{
    return Erase(make_pair(string("addrutxo"),
                           CAddressUtxoKey(nType, hashBytes, txhash, nIndex)));
}

bool CTxDBBase::WriteAddressTxId(int nType, const uint160& hashBytes, int nHeight,
                                 int nTxIndex, const uint256& txhash)
{
    return Write(make_pair(string("addrtxid"),
                           CAddressTxIdKey(nType, hashBytes, nHeight, nTxIndex, txhash)),
                 (char)0);
}

bool CTxDBBase::EraseAddressTxId(int nType, const uint160& hashBytes, int nHeight,
                                 int nTxIndex, const uint256& txhash)
{
    return Erase(make_pair(string("addrtxid"),
                           CAddressTxIdKey(nType, hashBytes, nHeight, nTxIndex, txhash)));
}

bool CTxDBBase::GetAddressUtxos(int nType, const uint160& hashBytes,
                                std::vector<std::pair<COutPoint, std::pair<int64_t, int> > >& vUtxos)
{
    vUtxos.clear();

    CDataStream ssKeyPrefix(SER_DISK, CLIENT_VERSION);
    ssKeyPrefix << make_pair(string("addrutxo"),
                             CAddressUtxoKey(nType, hashBytes, uint256(0), 0));
    string strPrefixBegin = ssKeyPrefix.str();

    auto it = NewIterator();
    for (it->Seek(strPrefixBegin); it->Valid(); it->Next())
    {
        const string keyStr = it->KeyStr();
        CDataStream ssKey(keyStr.data(), keyStr.data() + keyStr.size(),
                          SER_DISK, CLIENT_VERSION);
        string strKeyType;
        CAddressUtxoKey utxoKey;
        ssKey >> strKeyType;
        if (strKeyType != "addrutxo")
            break;
        ssKey >> utxoKey;
        if (utxoKey.nType != nType || utxoKey.hashBytes != hashBytes)
            break;

        const string valueStr = it->ValueStr();
        CDataStream ssValue(valueStr.data(), valueStr.data() + valueStr.size(),
                            SER_DISK, CLIENT_VERSION);
        CAddressUtxoValue utxoValue;
        ssValue >> utxoValue;

        COutPoint outpoint(utxoKey.txhash, utxoKey.nIndex);
        vUtxos.push_back(make_pair(outpoint,
                                   make_pair(utxoValue.nValue, utxoValue.nHeight)));
    }
    return true;
}

bool CTxDBBase::GetAddressTxIds(int nType, const uint160& hashBytes,
                                int nStartHeight, int nEndHeight,
                                std::vector<uint256>& vTxIds)
{
    vTxIds.clear();

    CDataStream ssKeyPrefix(SER_DISK, CLIENT_VERSION);
    ssKeyPrefix << make_pair(string("addrtxid"),
                             CAddressTxIdKey(nType, hashBytes, nStartHeight, 0, uint256(0)));
    string strPrefixBegin = ssKeyPrefix.str();

    auto it = NewIterator();
    for (it->Seek(strPrefixBegin); it->Valid(); it->Next())
    {
        const string keyStr = it->KeyStr();
        CDataStream ssKey(keyStr.data(), keyStr.data() + keyStr.size(),
                          SER_DISK, CLIENT_VERSION);
        string strKeyType;
        CAddressTxIdKey txIdKey;
        ssKey >> strKeyType;
        if (strKeyType != "addrtxid")
            break;
        ssKey >> txIdKey;
        if (txIdKey.nType != nType || txIdKey.hashBytes != hashBytes)
            break;
        if (txIdKey.nHeight > nEndHeight)
            break;

        vTxIds.push_back(txIdKey.txhash);
    }
    return true;
}

// ============================================================================
// In-memory UTXO cache (read-through, backend-agnostic)
//
// Avoids hitting the underlying KV store for every FetchInputs call. On a 2M+
// block chain with millions of UTXOs, this dramatically reduces I/O during
// both IBD (ConnectBlock validation reads inputs) and steady-state (mempool
// acceptance, staking). Writes/erases update both cache and the backend.
// ============================================================================
namespace {

struct COutPointHasher {
    size_t operator()(const COutPoint& op) const {
        return op.hash.Get64() ^
               (std::hash<unsigned int>()(op.n) * 0x9e3779b97f4a7c15ULL);
    }
};

struct CUtxoCacheEntry {
    CUtxoEntry utxo;
    bool fPresent;  // true = exists, false = known absent (negative cache)
    CUtxoCacheEntry() : fPresent(false) {}
    CUtxoCacheEntry(const CUtxoEntry& u, bool p) : utxo(u), fPresent(p) {}
};

std::unordered_map<COutPoint, CUtxoCacheEntry, COutPointHasher> g_mapUtxoCache;
CCriticalSection g_cs_utxoCache;
const size_t UTXO_CACHE_MAX_ENTRIES = 2000000;  // ~400MB at ~200 bytes each

} // anonymous namespace

bool CTxDBBase::ReadUtxo(const uint256& hash, unsigned int n, CUtxoEntry& entry)
{
    entry.SetNull();
    COutPoint outpoint(hash, n);

    {
        LOCK(g_cs_utxoCache);
        auto it = g_mapUtxoCache.find(outpoint);
        if (it != g_mapUtxoCache.end())
        {
            if (it->second.fPresent) {
                entry = it->second.utxo;
                return true;
            }
            return false;
        }
    }

    bool fFound = Read(make_pair(string("u"), make_pair(hash, n)), entry);

    {
        LOCK(g_cs_utxoCache);
        if (g_mapUtxoCache.size() < UTXO_CACHE_MAX_ENTRIES)
        {
            if (fFound)
                g_mapUtxoCache[outpoint] = CUtxoCacheEntry(entry, true);
            else
                g_mapUtxoCache[outpoint] = CUtxoCacheEntry(CUtxoEntry(), false);
        }
    }

    return fFound;
}

bool CTxDBBase::WriteUtxo(const uint256& hash, unsigned int n, const CUtxoEntry& entry)
{
    COutPoint outpoint(hash, n);

    {
        LOCK(g_cs_utxoCache);
        g_mapUtxoCache[outpoint] = CUtxoCacheEntry(entry, true);

        // Periodic eviction: clear half when over the limit. Simple but
        // effective — the cache repopulates with the hot working set.
        if (g_mapUtxoCache.size() > UTXO_CACHE_MAX_ENTRIES)
        {
            size_t nTarget = UTXO_CACHE_MAX_ENTRIES / 2;
            auto it = g_mapUtxoCache.begin();
            while (g_mapUtxoCache.size() > nTarget && it != g_mapUtxoCache.end())
                it = g_mapUtxoCache.erase(it);
        }
    }

    return Write(make_pair(string("u"), make_pair(hash, n)), entry);
}

bool CTxDBBase::EraseUtxo(const uint256& hash, unsigned int n)
{
    COutPoint outpoint(hash, n);

    {
        LOCK(g_cs_utxoCache);
        g_mapUtxoCache[outpoint] = CUtxoCacheEntry(CUtxoEntry(), false);
    }

    return Erase(make_pair(string("u"), make_pair(hash, n)));
}

bool CTxDBBase::HaveUtxo(const uint256& hash, unsigned int n)
{
    COutPoint outpoint(hash, n);

    {
        LOCK(g_cs_utxoCache);
        auto it = g_mapUtxoCache.find(outpoint);
        if (it != g_mapUtxoCache.end())
            return it->second.fPresent;
    }

    if (Exists(make_pair(string("u"), make_pair(hash, n))))
        return true;

    // Lazy fallback: check old CTxIndex vSpent for databases upgrading from
    // pre-UTXO format. vSpent[n] null = output not spent = UTXO exists.
    CTxIndex txindex;
    if (ReadTxIndex(hash, txindex))
    {
        if (n < txindex.vSpent.size() && txindex.vSpent[n].IsNull())
            return true;
    }

    return false;
}

int64_t CTxDBBase::SumUtxoValues(int& nCount)
{
    nCount = 0;
    int64_t nTotal = 0;

    CDataStream ssKeyPrefix(SER_DISK, CLIENT_VERSION);
    ssKeyPrefix << make_pair(string("u"), make_pair(uint256(0), (unsigned int)0));
    string strPrefixBegin = ssKeyPrefix.str();

    auto it = NewIterator();
    for (it->Seek(strPrefixBegin); it->Valid(); it->Next())
    {
        const string keyStr = it->KeyStr();
        CDataStream ssKey(keyStr.data(), keyStr.data() + keyStr.size(),
                          SER_DISK, CLIENT_VERSION);
        string strKeyType;
        ssKey >> strKeyType;
        if (strKeyType != "u")
            break;

        const string valueStr = it->ValueStr();
        CDataStream ssValue(valueStr.data(), valueStr.data() + valueStr.size(),
                            SER_DISK, CLIENT_VERSION);
        CUtxoEntry entry;
        ssValue >> entry;

        nTotal += entry.nValue;
        nCount++;
    }
    return nTotal;
}
