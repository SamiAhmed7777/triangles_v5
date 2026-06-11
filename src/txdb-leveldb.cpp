// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#include <map>

#include <filesystem>

#include <boost/version.hpp>

#include <leveldb/env.h>
#include <leveldb/cache.h>
#include <leveldb/filter_policy.h>
#include <leveldb/iterator.h>
#include <memenv/memenv.h>

#include "kernel.h"
#include "checkpoints.h"
#include "txdb.h"
#include "util.h"
#include "ui_interface.h"
#include "addressindex.h"
#include "main.h"

using namespace std;
namespace fs = std::filesystem;

leveldb::DB *txdb; // global pointer for LevelDB object instance

bool CDiskBlockIndex::fSerializeChainTrust = false;

static leveldb::Options GetOptions() {
    leveldb::Options options;
    int nCacheSizeMB = GetArg("-dbcache", 2048);
    options.block_cache = leveldb::NewLRUCache(nCacheSizeMB * 1048576);
    options.filter_policy = leveldb::NewBloomFilterPolicy(10);
    // Larger write buffer (64MB vs default 4MB) reduces the frequency of
    // memtable flushes and compactions, which is a big win during IBD
    // when millions of tx index entries are written sequentially.
    options.write_buffer_size = 64 * 1048576;
    options.max_open_files = 1000;
    return options;
}

void init_blockindex(leveldb::Options& options, bool fRemoveOld = false) {
    fs::path directory = GetDataDir() / "txleveldb";

    if (fRemoveOld) {
        fs::remove_all(directory);
        unsigned int nFile = 1;
        while (true)
        {
            fs::path strBlockFile = GetDataDir() / strprintf("blk%04u.dat", nFile);
            if(!fs::exists(strBlockFile))
                break;
            fs::remove(strBlockFile);
            nFile++;
        }
    }

    fs::create_directory(directory);
    printf("Opening LevelDB in %s\n", directory.string().c_str());
    leveldb::Status status = leveldb::DB::Open(options, directory.string(), &txdb);
    if (!status.ok()) {
        throw runtime_error(strprintf("init_blockindex(): error opening database environment %s", status.ToString().c_str()));
    }
}

CTxDB::CTxDB(const char* pszMode)
{
    assert(pszMode);
    activeBatch = nullptr;
    fReadOnly = (!strchr(pszMode, '+') && !strchr(pszMode, 'w'));

    if (txdb) {
        pdb = txdb;
        return;
    }

    bool fCreate = strchr(pszMode, 'c');

    options = GetOptions();
    options.create_if_missing = fCreate;
    options.filter_policy = leveldb::NewBloomFilterPolicy(10);

    init_blockindex(options);
    pdb = txdb;

    if (Exists(string("version")))
    {
        ReadVersion(nVersion);
        printf("Transaction index version is %d\n", nVersion);

        if (nVersion < DATABASE_VERSION)
        {
            printf("Required index version is %d, removing old database\n", DATABASE_VERSION);

            delete txdb;
            txdb = pdb = nullptr;
            delete activeBatch;
            activeBatch = nullptr;

            init_blockindex(options, true);
            pdb = txdb;

            bool fTmp = fReadOnly;
            fReadOnly = false;
            WriteVersion(DATABASE_VERSION);
            fReadOnly = fTmp;
        }
    }
    else if (fCreate)
    {
        bool fTmp = fReadOnly;
        fReadOnly = false;
        WriteVersion(DATABASE_VERSION);
        fReadOnly = fTmp;
    }

    printf("Opened LevelDB successfully\n");
}

void CTxDB::Close()
{
    delete txdb;
    txdb = pdb = nullptr;
    delete options.filter_policy;
    options.filter_policy = nullptr;
    delete options.block_cache;
    options.block_cache = nullptr;
    delete activeBatch;
    activeBatch = nullptr;
}

bool CTxDB::TxnBegin()
{
    // Allow calling TxnBegin when a batch is already active (no-op).
    // This lets callers like SetBestChain share a batch that was opened
    // earlier by AddToBlockIndex, merging two commits into one.
    if (activeBatch)
        return true;
    activeBatch = new leveldb::WriteBatch();
    return true;
}

bool CTxDB::TxnCommit()
{
    assert(activeBatch);
    leveldb::Status status = pdb->Write(leveldb::WriteOptions(), activeBatch);
    delete activeBatch;
    activeBatch = nullptr;
    if (!status.ok()) {
        printf("ERROR: LevelDB batch commit failure: %s\n", status.ToString().c_str());
        printf("ERROR: This may indicate disk full, corruption, or permissions issue.\n");
        printf("ERROR: Chain state may be inconsistent - immediate investigation required!\n");
        return false;
    }
    return true;
}

namespace {

class CBatchScanner : public leveldb::WriteBatch::Handler {
public:
    std::string needle;
    bool *deleted;
    std::string *foundValue;
    bool foundEntry;

    CBatchScanner() : foundEntry(false) {}

    virtual void Put(const leveldb::Slice& key, const leveldb::Slice& value) {
        if (key.ToString() == needle) {
            foundEntry = true;
            *deleted = false;
            *foundValue = value.ToString();
        }
    }

    virtual void Delete(const leveldb::Slice& key) {
        if (key.ToString() == needle) {
            foundEntry = true;
            *deleted = true;
        }
    }
};

class CLevelDBIterator final : public CTxDBIteratorBase {
public:
    explicit CLevelDBIterator(leveldb::Iterator* pit) : pit(pit) {}
    ~CLevelDBIterator() override { delete pit; }

    void Seek(const std::string& key) override { pit->Seek(key); }
    bool Valid() const override { return pit->Valid(); }
    void Next() override { pit->Next(); }
    std::string KeyStr() const override   { return pit->key().ToString(); }
    std::string ValueStr() const override { return pit->value().ToString(); }

private:
    leveldb::Iterator* pit;
};

} // anonymous namespace

// When performing a read with an active batch, check the batch first. The
// rest of the codebase assumes that once a batch is open, reads are
// consistent with the pending writes inside it.
bool CTxDB::ScanBatch(const std::string& key, string* value, bool* deleted) const
{
    assert(activeBatch);
    *deleted = false;
    CBatchScanner scanner;
    scanner.needle = key;
    scanner.deleted = deleted;
    scanner.foundValue = value;
    leveldb::Status status = activeBatch->Iterate(&scanner);
    if (!status.ok()) {
        throw runtime_error(status.ToString());
    }
    return scanner.foundEntry;
}

bool CTxDB::ReadRaw(const std::string& key, std::string& value) const
{
    bool readFromDb = true;
    if (activeBatch) {
        bool deleted = false;
        readFromDb = ScanBatch(key, &value, &deleted) == false;
        if (deleted)
            return false;
    }
    if (readFromDb) {
        leveldb::Status status = pdb->Get(leveldb::ReadOptions(), key, &value);
        if (!status.ok()) {
            if (status.IsNotFound())
                return false;
            printf("LevelDB read failure: %s\n", status.ToString().c_str());
            return false;
        }
    }
    return true;
}

bool CTxDB::WriteRaw(const std::string& key, const std::string& value)
{
    if (activeBatch) {
        activeBatch->Put(key, value);
        return true;
    }
    leveldb::Status status = pdb->Put(leveldb::WriteOptions(), key, value);
    if (!status.ok()) {
        printf("LevelDB write failure: %s\n", status.ToString().c_str());
        return false;
    }
    return true;
}

bool CTxDB::EraseRaw(const std::string& key)
{
    if (!pdb)
        return false;
    if (activeBatch) {
        activeBatch->Delete(key);
        return true;
    }
    leveldb::Status status = pdb->Delete(leveldb::WriteOptions(), key);
    return (status.ok() || status.IsNotFound());
}

bool CTxDB::ExistsRaw(const std::string& key) const
{
    std::string unused;

    if (activeBatch) {
        bool deleted = false;
        if (ScanBatch(key, &unused, &deleted) && !deleted)
            return true;
    }

    leveldb::Status status = pdb->Get(leveldb::ReadOptions(), key, &unused);
    return status.IsNotFound() == false;
}

std::unique_ptr<CTxDBIteratorBase> CTxDB::NewIterator() const
{
    return std::unique_ptr<CTxDBIteratorBase>(
        new CLevelDBIterator(pdb->NewIterator(leveldb::ReadOptions())));
}

static CBlockIndex *InsertBlockIndex(uint256 hash)
{
    if (hash == 0)
        return nullptr;

    map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.find(hash);
    if (mi != mapBlockIndex.end())
        return (*mi).second;

    CBlockIndex* pindexNew = new CBlockIndex();
    if (!pindexNew)
        throw runtime_error("LoadBlockIndex() : new CBlockIndex failed");
    mi = mapBlockIndex.insert(make_pair(hash, pindexNew)).first;
    pindexNew->phashBlock = &((*mi).first);

    return pindexNew;
}

bool CTxDB::LoadBlockIndex()
{
    if (mapBlockIndex.size() > 0) {
        // Already loaded once in this session. Can happen during BDB migration.
        return true;
    }

    // Check DB format version to determine serialization features.
    int nDbFormat = 1;
    ReadDbFormat(nDbFormat);
    CDiskBlockIndex::fSerializeChainTrust = (nDbFormat >= 2);

    if (CDiskBlockIndex::fSerializeChainTrust)
        printf("LoadBlockIndex(): DB format v%d - nChainTrust persisted\n", nDbFormat);
    else
        printf("LoadBlockIndex(): DB format v%d - will recalculate nChainTrust (one-time upgrade)\n", nDbFormat);

    // Scan the block index out of the DB into mapBlockIndex.
    int64_t nPhaseStart = GetTimeMillis();
    int64_t nTotalStart = nPhaseStart;
    leveldb::Iterator *iterator = pdb->NewIterator(leveldb::ReadOptions());
    CDataStream ssStartKey(SER_DISK, CLIENT_VERSION);
    ssStartKey << make_pair(string("blockindex"), uint256(0));
    iterator->Seek(ssStartKey.str());
    int nBlocksLoaded = 0;
    while (iterator->Valid())
    {
        if (++nBlocksLoaded % 100000 == 0)
        {
            std::string strMsg = strprintf(_("Loading block index... (%d blocks)"), nBlocksLoaded);
            uiInterface.InitMessage(strMsg);
        }

        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.write(iterator->key().data(), iterator->key().size());
        CDataStream ssValue(SER_DISK, CLIENT_VERSION);
        ssValue.write(iterator->value().data(), iterator->value().size());
        string strType;
        ssKey >> strType;
        if (fRequestShutdown || strType != "blockindex")
            break;
        CDiskBlockIndex diskindex;
        ssValue >> diskindex;

        uint256 blockHash = diskindex.GetBlockHash();

        CBlockIndex* pindexNew    = InsertBlockIndex(blockHash);
        pindexNew->pprev          = InsertBlockIndex(diskindex.hashPrev);
        pindexNew->pnext          = InsertBlockIndex(diskindex.hashNext);
        pindexNew->nFile          = diskindex.nFile;
        pindexNew->nBlockPos      = diskindex.nBlockPos;
        pindexNew->nHeight        = diskindex.nHeight;
        pindexNew->nMint          = diskindex.nMint;
        pindexNew->nMoneySupply   = diskindex.nMoneySupply;
        pindexNew->nFlags         = diskindex.nFlags;
        pindexNew->nStakeModifier = diskindex.nStakeModifier;
        pindexNew->prevoutStake   = diskindex.prevoutStake;
        pindexNew->nStakeTime     = diskindex.nStakeTime;
        pindexNew->hashProofOfStake = diskindex.hashProofOfStake;
        pindexNew->nVersion       = diskindex.nVersion;
        pindexNew->hashMerkleRoot = diskindex.hashMerkleRoot;
        pindexNew->nTime          = diskindex.nTime;
        pindexNew->nBits          = diskindex.nBits;
        pindexNew->nNonce         = diskindex.nNonce;
        pindexNew->nChainTrust    = diskindex.nChainTrust;

        if (pindexGenesisBlock == nullptr && blockHash == (!fTestNet ? hashGenesisBlockOfficial : hashGenesisBlockTestNet))
            pindexGenesisBlock = pindexNew;

        if (!pindexNew->CheckIndex()) {
            delete iterator;
            return error("LoadBlockIndex() : CheckIndex failed at %d", pindexNew->nHeight);
        }

        iterator->Next();
    }
    delete iterator;
    printf("STARTUP-PERF: block_index_deserialize %" PRId64 "ms blocks=%d\n", GetTimeMillis() - nPhaseStart, nBlocksLoaded);

    if (fRequestShutdown)
        return true;

    // ---- nChainTrust: recalculate if not persisted, or verify stake modifiers ----
    nPhaseStart = GetTimeMillis();
    bool fNeedChainTrustRecalc = !CDiskBlockIndex::fSerializeChainTrust;

    if (fNeedChainTrustRecalc)
    {
        uiInterface.InitMessage(_("Calculating chain trust (one-time upgrade)..."));

        vector<pair<int, CBlockIndex*> > vSortedByHeight;
        vSortedByHeight.reserve(mapBlockIndex.size());
        for (const auto& item : mapBlockIndex)
            vSortedByHeight.push_back(make_pair(item.second->nHeight, item.second));
        sort(vSortedByHeight.begin(), vSortedByHeight.end());

        int nLastCheckpointHeight = Checkpoints::GetTotalBlocksEstimate();
        int nProgressInterval = std::max((int)vSortedByHeight.size() / 20, 1);
        int nCount = 0;

        for (const auto& item : vSortedByHeight)
        {
            CBlockIndex* pindex = item.second;
            pindex->nChainTrust = (pindex->pprev ? pindex->pprev->nChainTrust : 0) + pindex->GetBlockTrust();

            if (pindex->nHeight >= nLastCheckpointHeight)
            {
                pindex->nStakeModifierChecksum = GetStakeModifierChecksum(pindex);
                if (!CheckStakeModifierCheckpoints(pindex->nHeight, pindex->nStakeModifierChecksum))
                    return error("CTxDB::LoadBlockIndex() : Failed stake modifier checkpoint height=%d, modifier=0x%016"PRIx64, pindex->nHeight, pindex->nStakeModifier);
            }

            if (++nCount % nProgressInterval == 0)
            {
                std::string strMsg = strprintf(_("Calculating chain trust... (%d%%)"), nCount * 100 / vSortedByHeight.size());
                uiInterface.InitMessage(strMsg);
            }
        }

        // Upgrade: rewrite all block index entries with nChainTrust and bump format.
        printf("LoadBlockIndex(): upgrading DB to format v3 (persisting nChainTrust + UTXO model)...\n");
        uiInterface.InitMessage(_("Upgrading block index..."));
        CDiskBlockIndex::fSerializeChainTrust = true;

        leveldb::WriteBatch batch;
        nCount = 0;
        for (const auto& item : vSortedByHeight)
        {
            CBlockIndex* pindex = item.second;
            CDiskBlockIndex diskindex(pindex);

            CDataStream ssKey(SER_DISK, CLIENT_VERSION);
            ssKey << make_pair(string("blockindex"), *pindex->phashBlock);
            CDataStream ssValue(SER_DISK, CLIENT_VERSION);
            ssValue << diskindex;
            batch.Put(ssKey.str(), ssValue.str());

            if (++nCount % 100000 == 0)
            {
                pdb->Write(leveldb::WriteOptions(), &batch);
                batch.Clear();
                printf("LoadBlockIndex(): upgraded %d / %d block index entries\n", nCount, (int)vSortedByHeight.size());
            }
        }
        CDataStream ssFmtKey(SER_DISK, CLIENT_VERSION);
        ssFmtKey << string("dbformat");
        CDataStream ssFmtValue(SER_DISK, CLIENT_VERSION);
        ssFmtValue << (int)3;
        batch.Put(ssFmtKey.str(), ssFmtValue.str());

        leveldb::Status status = pdb->Write(leveldb::WriteOptions(), &batch);
        if (!status.ok())
            return error("LoadBlockIndex(): failed to write upgraded block index: %s", status.ToString().c_str());

        printf("LoadBlockIndex(): DB upgraded to format v3 (%d entries rewritten)\n", nCount);
    }
    else
    {
        int nLastCheckpointHeight = Checkpoints::GetTotalBlocksEstimate();
        bool fNeedModifierCheck = false;
        for (const auto& item : mapBlockIndex)
        {
            if (item.second->nHeight >= nLastCheckpointHeight)
            {
                fNeedModifierCheck = true;
                break;
            }
        }

        if (fNeedModifierCheck)
        {
            vector<pair<int, CBlockIndex*> > vAboveCheckpoint;
            for (const auto& item : mapBlockIndex)
                if (item.second->nHeight >= nLastCheckpointHeight)
                    vAboveCheckpoint.push_back(make_pair(item.second->nHeight, item.second));
            sort(vAboveCheckpoint.begin(), vAboveCheckpoint.end());

            for (const auto& item : vAboveCheckpoint)
            {
                CBlockIndex* pindex = item.second;
                pindex->nStakeModifierChecksum = GetStakeModifierChecksum(pindex);
                if (!CheckStakeModifierCheckpoints(pindex->nHeight, pindex->nStakeModifierChecksum))
                    return error("CTxDB::LoadBlockIndex() : Failed stake modifier checkpoint height=%d, modifier=0x%016"PRIx64, pindex->nHeight, pindex->nStakeModifier);
            }
        }
    }

    printf("STARTUP-PERF: chain_trust_and_modifiers %" PRId64 "ms\n", GetTimeMillis() - nPhaseStart);

    if (nDbFormat < 3)
    {
        WriteDbFormat(3);
        printf("LoadBlockIndex(): bumped dbformat to v3 (UTXO model with lazy fallback)\n");
    }

    nPhaseStart = GetTimeMillis();
    if (!ReadHashBestChain(hashBestChain))
    {
        if (pindexGenesisBlock == nullptr)
            return true;
        return error("CTxDB::LoadBlockIndex() : hashBestChain not loaded");
    }
    if (!mapBlockIndex.count(hashBestChain))
        return error("CTxDB::LoadBlockIndex() : hashBestChain not found in the block index");
    pindexBest = mapBlockIndex[hashBestChain];
    nBestHeight = pindexBest->nHeight;
    nBestChainTrust = pindexBest->nChainTrust;

    // Heal pnext pointers along the active chain.  Persisted hashNext can be
    // stale or zeroed by crash-interrupted reorgs, which breaks
    // GetKernelStakeModifier()'s forward walk and causes valid new
    // proof-of-stake blocks to be rejected with "check kernel failed".
    {
        int nHealed = 0;
        for (CBlockIndex* p = pindexBest; p && p->pprev; p = p->pprev)
        {
            if (p->pprev->pnext != p) { p->pprev->pnext = p; nHealed++; }
        }
        if (nHealed > 0)
            printf("LoadBlockIndex(): healed %d pnext links on active chain\n", nHealed);
    }

    printf("STARTUP-PERF: best_chain %" PRId64 "ms\n", GetTimeMillis() - nPhaseStart);

    nPhaseStart = GetTimeMillis();
    {
        int nStakeSeenDepth = 500;
        CBlockIndex* pindex = pindexBest;
        int nLoaded = 0;
        while (pindex && nLoaded < nStakeSeenDepth)
        {
            if (pindex->IsProofOfStake())
                setStakeSeen.insert(make_pair(pindex->prevoutStake, pindex->nStakeTime));
            pindex = pindex->pprev;
            nLoaded++;
        }
        printf("LoadBlockIndex(): populated setStakeSeen with %d entries (last %d blocks)\n",
               (int)setStakeSeen.size(), nLoaded);
    }
    printf("STARTUP-PERF: stake_seen %" PRId64 "ms\n", GetTimeMillis() - nPhaseStart);

    printf("LoadBlockIndex(): hashBestChain=%s  height=%d  trust=%s  date=%s\n",
      hashBestChain.ToString().substr(0,20).c_str(), nBestHeight, CBigNum(nBestChainTrust).ToString().c_str(),
      DateTimeStrFormat("%x %H:%M:%S", pindexBest->GetBlockTime()).c_str());

    // Re-evaluate best chain: scan for competing tips with equal or greater trust.
    {
        CBlockIndex* pindexBetter = nullptr;
        for (const auto& item : mapBlockIndex)
        {
            CBlockIndex* pindex = item.second;
            if (pindex == pindexBest)
                continue;
            if (pindex->nChainTrust > nBestChainTrust)
            {
                pindexBetter = pindex;
                break;
            }
            if (pindex->nChainTrust == nBestChainTrust &&
                pindex->GetBlockHash() < pindexBest->GetBlockHash())
            {
                if (!pindexBetter || pindex->GetBlockHash() < pindexBetter->GetBlockHash())
                    pindexBetter = pindex;
            }
        }
        if (pindexBetter)
        {
            printf("LoadBlockIndex(): found better chain tip %s at height %d (trust %s vs %s)\n",
                pindexBetter->GetBlockHash().ToString().substr(0,20).c_str(),
                pindexBetter->nHeight,
                CBigNum(pindexBetter->nChainTrust).ToString().c_str(),
                CBigNum(nBestChainTrust).ToString().c_str());
            CBlock block;
            if (block.ReadFromDisk(pindexBetter))
            {
                CTxDB txdb2;
                if (block.SetBestChain(txdb2, pindexBetter))
                {
                    hashBestChain = pindexBetter->GetBlockHash();
                    pindexBest = pindexBetter;
                    nBestHeight = pindexBetter->nHeight;
                    nBestChainTrust = pindexBetter->nChainTrust;
                    printf("LoadBlockIndex(): switched to better chain tip\n");
                }
            }
        }
    }

    if (!ReadSyncCheckpoint(Checkpoints::hashSyncCheckpoint))
        printf("LoadBlockIndex(): no sync checkpoint in DB, using default\n");
    else
        printf("LoadBlockIndex(): synchronized checkpoint %s\n", Checkpoints::hashSyncCheckpoint.ToString().c_str());
    if (!mapBlockIndex.count(Checkpoints::hashSyncCheckpoint))
    {
        printf("LoadBlockIndex(): sync checkpoint not in index, resetting to genesis\n");
        Checkpoints::hashSyncCheckpoint = (!fTestNet ? hashGenesisBlockOfficial : hashGenesisBlockTestNet);
    }

    CBigNum bnBestInvalidTrust;
    ReadBestInvalidTrust(bnBestInvalidTrust);
    nBestInvalidTrust = bnBestInvalidTrust.getuint256();

    nPhaseStart = GetTimeMillis();
    int nCheckLevel = GetArg("-checklevel", 1);
    int nCheckDepth = GetArg( "-checkblocks", 50);
    if (nCheckDepth == 0)
        nCheckDepth = 1000000000;
    if (nCheckDepth > nBestHeight)
        nCheckDepth = nBestHeight;
    printf("Verifying last %i blocks at level %i\n", nCheckDepth, nCheckLevel);
    CBlockIndex* pindexFork = nullptr;
    map<pair<unsigned int, unsigned int>, CBlockIndex*> mapBlockPos;
    for (CBlockIndex* pindex = pindexBest; pindex && pindex->pprev; pindex = pindex->pprev)
    {
        if (fRequestShutdown || pindex->nHeight < nBestHeight-nCheckDepth)
            break;
        CBlock block;
        if (!block.ReadFromDisk(pindex))
            return error("LoadBlockIndex() : block.ReadFromDisk failed");
        if (nCheckLevel>0 && !block.CheckBlock(true, true, (nCheckLevel>6)))
        {
            printf("LoadBlockIndex() : *** found bad block at %d, hash=%s\n", pindex->nHeight, pindex->GetBlockHash().ToString().c_str());
            pindexFork = pindex->pprev;
        }
        if (nCheckLevel>1)
        {
            pair<unsigned int, unsigned int> pos = make_pair(pindex->nFile, pindex->nBlockPos);
            mapBlockPos[pos] = pindex;
            for (const CTransaction &tx : block.vtx)
            {
                uint256 hashTx = tx.GetHash();
                CTxIndex txindex;
                if (ReadTxIndex(hashTx, txindex))
                {
                    if (nCheckLevel>2 || pindex->nFile != txindex.pos.nFile || pindex->nBlockPos != txindex.pos.nBlockPos)
                    {
                        CTransaction txFound;
                        if (!txFound.ReadFromDisk(txindex.pos))
                        {
                            printf("LoadBlockIndex() : *** cannot read mislocated transaction %s\n", hashTx.ToString().c_str());
                            pindexFork = pindex->pprev;
                        }
                        else
                            if (txFound.GetHash() != hashTx)
                            {
                                printf("LoadBlockIndex(): *** invalid tx position for %s\n", hashTx.ToString().c_str());
                                pindexFork = pindex->pprev;
                            }
                    }
                    if (nCheckLevel>3 && !tx.IsCoinBase())
                    {
                        for (const CTxIn &txin : tx.vin)
                        {
                            if (HaveUtxo(txin.prevout.hash, txin.prevout.n))
                            {
                                printf("LoadBlockIndex(): *** spent input still in UTXO set: %s:%i in %s\n",
                                       txin.prevout.hash.ToString().c_str(), txin.prevout.n, hashTx.ToString().c_str());
                                pindexFork = pindex->pprev;
                            }
                        }
                    }
                }
            }
        }
    }
    if (pindexFork && !fRequestShutdown)
    {
        printf("LoadBlockIndex() : *** moving best chain pointer back to block %d\n", pindexFork->nHeight);
        CBlock block;
        if (!block.ReadFromDisk(pindexFork))
            return error("LoadBlockIndex() : block.ReadFromDisk failed");
        CTxDB txdb;
        block.SetBestChain(txdb, pindexFork);
    }
    printf("STARTUP-PERF: verify_blocks %" PRId64 "ms depth=%d level=%d\n", GetTimeMillis() - nPhaseStart, nCheckDepth, nCheckLevel);
    printf("STARTUP-PERF: load_block_index_total %" PRId64 "ms\n", GetTimeMillis() - nTotalStart);

    return true;
}
