// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#include <map>

#include <boost/version.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

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
using namespace boost;
namespace fs = boost::filesystem;

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
    // Allow more open files for better read performance on large chains
    options.max_open_files = 1000;
    return options;
}

void init_blockindex(leveldb::Options& options, bool fRemoveOld = false) {
    // First time init.
    fs::path directory = GetDataDir() / "txleveldb";

    if (fRemoveOld) {
        fs::remove_all(directory); // remove directory
        unsigned int nFile = 1;

        while (true)
        {
            fs::path strBlockFile = GetDataDir() / strprintf("blk%04u.dat", nFile);

            // Break if no such file
            if( !fs::exists( strBlockFile ) )
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

// CDB subclasses are created and destroyed VERY OFTEN. That's why
// we shouldn't treat this as a free operations.
CTxDB::CTxDB(const char* pszMode)
{
    assert(pszMode);
    activeBatch = NULL;
    fReadOnly = (!strchr(pszMode, '+') && !strchr(pszMode, 'w'));

    if (txdb) {
        pdb = txdb;
        return;
    }

    bool fCreate = strchr(pszMode, 'c');

    options = GetOptions();
    options.create_if_missing = fCreate;
    options.filter_policy = leveldb::NewBloomFilterPolicy(10);

    init_blockindex(options); // Init directory
    pdb = txdb;

    if (Exists(string("version")))
    {
        ReadVersion(nVersion);
        printf("Transaction index version is %d\n", nVersion);

        if (nVersion < DATABASE_VERSION)
        {
            printf("Required index version is %d, removing old database\n", DATABASE_VERSION);

            // Leveldb instance destruction
            delete txdb;
            txdb = pdb = NULL;
            delete activeBatch;
            activeBatch = NULL;

            init_blockindex(options, true); // Remove directory and create new database
            pdb = txdb;

            bool fTmp = fReadOnly;
            fReadOnly = false;
            WriteVersion(DATABASE_VERSION); // Save transaction index version
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
    txdb = pdb = NULL;
    delete options.filter_policy;
    options.filter_policy = NULL;
    delete options.block_cache;
    options.block_cache = NULL;
    delete activeBatch;
    activeBatch = NULL;
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
    activeBatch = NULL;
    if (!status.ok()) {
        printf("LevelDB batch commit failure: %s\n", status.ToString().c_str());
        return false;
    }
    return true;
}

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

// When performing a read, if we have an active batch we need to check it first
// before reading from the database, as the rest of the code assumes that once
// a database transaction begins reads are consistent with it. It would be good
// to change that assumption in future and avoid the performance hit, though in
// practice it does not appear to be large.
bool CTxDB::ScanBatch(const CDataStream &key, string *value, bool *deleted) const {
    assert(activeBatch);
    *deleted = false;
    CBatchScanner scanner;
    scanner.needle = key.str();
    scanner.deleted = deleted;
    scanner.foundValue = value;
    leveldb::Status status = activeBatch->Iterate(&scanner);
    if (!status.ok()) {
        throw runtime_error(status.ToString());
    }
    return scanner.foundEntry;
}

bool CTxDB::ReadTxIndex(uint256 hash, CTxIndex& txindex)
{
    assert(!fClient);
    txindex.SetNull();
    return Read(make_pair(string("tx"), hash), txindex);
}

bool CTxDB::UpdateTxIndex(uint256 hash, const CTxIndex& txindex)
{
    assert(!fClient);
    return Write(make_pair(string("tx"), hash), txindex);
}

bool CTxDB::AddTxIndex(const CTransaction& tx, const CDiskTxPos& pos, int nHeight)
{
    assert(!fClient);

    // Add to tx index
    uint256 hash = tx.GetHash();
    CTxIndex txindex(pos, tx.vout.size());
    return Write(make_pair(string("tx"), hash), txindex);
}

bool CTxDB::EraseTxIndex(const CTransaction& tx)
{
    assert(!fClient);
    uint256 hash = tx.GetHash();

    return Erase(make_pair(string("tx"), hash));
}

bool CTxDB::ContainsTx(uint256 hash)
{
    assert(!fClient);
    return Exists(make_pair(string("tx"), hash));
}

bool CTxDB::ReadDiskTx(uint256 hash, CTransaction& tx, CTxIndex& txindex)
{
    assert(!fClient);
    tx.SetNull();
    if (!ReadTxIndex(hash, txindex))
        return false;
    return (tx.ReadFromDisk(txindex.pos));
}

bool CTxDB::ReadDiskTx(uint256 hash, CTransaction& tx)
{
    CTxIndex txindex;
    return ReadDiskTx(hash, tx, txindex);
}

bool CTxDB::ReadDiskTx(COutPoint outpoint, CTransaction& tx, CTxIndex& txindex)
{
    return ReadDiskTx(outpoint.hash, tx, txindex);
}

bool CTxDB::ReadDiskTx(COutPoint outpoint, CTransaction& tx)
{
    CTxIndex txindex;
    return ReadDiskTx(outpoint.hash, tx, txindex);
}

bool CTxDB::WriteBlockIndex(const CDiskBlockIndex& blockindex)
{
    return Write(make_pair(string("blockindex"), blockindex.GetBlockHash()), blockindex);
}

bool CTxDB::ReadHashBestChain(uint256& hashBestChain)
{
    return Read(string("hashBestChain"), hashBestChain);
}

bool CTxDB::WriteHashBestChain(uint256 hashBestChain)
{
    return Write(string("hashBestChain"), hashBestChain);
}

bool CTxDB::ReadAddressIndexBestChain(uint256& hashBestChain)
{
    return Read(string("addressIndexBestChain"), hashBestChain);
}

bool CTxDB::WriteAddressIndexBestChain(uint256 hashBestChain)
{
    return Write(string("addressIndexBestChain"), hashBestChain);
}

bool CTxDB::ReadAddressIndexStartHeight(int& nHeight)
{
    return Read(string("addressIndexStartHeight"), nHeight);
}

bool CTxDB::WriteAddressIndexStartHeight(int nHeight)
{
    return Write(string("addressIndexStartHeight"), nHeight);
}

bool CTxDB::ReadBestInvalidTrust(CBigNum& bnBestInvalidTrust)
{
    return Read(string("bnBestInvalidTrust"), bnBestInvalidTrust);
}

bool CTxDB::WriteBestInvalidTrust(CBigNum bnBestInvalidTrust)
{
    return Write(string("bnBestInvalidTrust"), bnBestInvalidTrust);
}

bool CTxDB::ReadSyncCheckpoint(uint256& hashCheckpoint)
{
    return Read(string("hashSyncCheckpoint"), hashCheckpoint);
}

bool CTxDB::WriteSyncCheckpoint(uint256 hashCheckpoint)
{
    return Write(string("hashSyncCheckpoint"), hashCheckpoint);
}

bool CTxDB::ReadCheckpointPubKey(string& strPubKey)
{
    return Read(string("strCheckpointPubKey"), strPubKey);
}

bool CTxDB::WriteCheckpointPubKey(const string& strPubKey)
{
    return Write(string("strCheckpointPubKey"), strPubKey);
}

static CBlockIndex *InsertBlockIndex(uint256 hash)
{
    if (hash == 0)
        return NULL;

    // Return existing
    map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.find(hash);
    if (mi != mapBlockIndex.end())
        return (*mi).second;

    // Create new
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
        // Already loaded once in this session. It can happen during migration
        // from BDB.
        return true;
    }

    // Check DB format version to determine serialization features.
    int nDbFormat = 1;
    ReadDbFormat(nDbFormat);
    CDiskBlockIndex::fSerializeChainTrust = (nDbFormat >= 2);

    if (CDiskBlockIndex::fSerializeChainTrust)
        printf("LoadBlockIndex(): DB format v%d — nChainTrust persisted\n", nDbFormat);
    else
        printf("LoadBlockIndex(): DB format v%d — will recalculate nChainTrust (one-time upgrade)\n", nDbFormat);

    // The block index is an in-memory structure that maps hashes to on-disk
    // locations where the contents of the block can be found. Here, we scan it
    // out of the DB and into mapBlockIndex.
    int64_t nPhaseStart = GetTimeMillis();
    int64_t nTotalStart = nPhaseStart;
    leveldb::Iterator *iterator = pdb->NewIterator(leveldb::ReadOptions());
    // Seek to start key.
    CDataStream ssStartKey(SER_DISK, CLIENT_VERSION);
    ssStartKey << make_pair(string("blockindex"), uint256(0));
    iterator->Seek(ssStartKey.str());
    // Now read each entry.
    int nBlocksLoaded = 0;
    while (iterator->Valid())
    {
        // Report progress every 100k blocks
        if (++nBlocksLoaded % 100000 == 0)
        {
            std::string strMsg = strprintf(_("Loading block index... (%d blocks)"), nBlocksLoaded);
            uiInterface.InitMessage(strMsg);
        }

        // Unpack keys and values.
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.write(iterator->key().data(), iterator->key().size());
        CDataStream ssValue(SER_DISK, CLIENT_VERSION);
        ssValue.write(iterator->value().data(), iterator->value().size());
        string strType;
        ssKey >> strType;
        // Did we reach the end of the data to read?
        if (fRequestShutdown || strType != "blockindex")
            break;
        CDiskBlockIndex diskindex;
        ssValue >> diskindex;

        uint256 blockHash = diskindex.GetBlockHash();

        // Construct block index object
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
        // nChainTrust is populated from disk if fSerializeChainTrust, else stays 0
        pindexNew->nChainTrust    = diskindex.nChainTrust;

        // Watch for genesis block
        if (pindexGenesisBlock == NULL && blockHash == (!fTestNet ? hashGenesisBlockOfficial : hashGenesisBlockTestNet))
            pindexGenesisBlock = pindexNew;

        if (!pindexNew->CheckIndex()) {
            delete iterator;
            return error("LoadBlockIndex() : CheckIndex failed at %d", pindexNew->nHeight);
        }

        // setStakeSeen is populated below for recent blocks only (Change D)

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

            // Flush in chunks to limit memory usage
            if (++nCount % 100000 == 0)
            {
                pdb->Write(leveldb::WriteOptions(), &batch);
                batch.Clear();
                printf("LoadBlockIndex(): upgraded %d / %d block index entries\n", nCount, (int)vSortedByHeight.size());
            }
        }
        // Write remaining entries + format version
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
        // nChainTrust was loaded from disk. Only need stake modifier checksums
        // for blocks above the last checkpoint (typically very few or zero).
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

    // Bump dbformat to 3 if needed (databases that already had v2 nChainTrust upgrade).
    // UTXO entries are written by ConnectBlock during normal sync. For databases upgrading
    // from older versions, FetchInputs has a lazy fallback to the old CTxIndex path.
    if (nDbFormat < 3)
    {
        WriteDbFormat(3);
        printf("LoadBlockIndex(): bumped dbformat to v3 (UTXO model with lazy fallback)\n");
    }

    // Load hashBestChain pointer to end of best chain
    nPhaseStart = GetTimeMillis();
    if (!ReadHashBestChain(hashBestChain))
    {
        if (pindexGenesisBlock == NULL)
            return true;
        return error("CTxDB::LoadBlockIndex() : hashBestChain not loaded");
    }
    if (!mapBlockIndex.count(hashBestChain))
        return error("CTxDB::LoadBlockIndex() : hashBestChain not found in the block index");
    pindexBest = mapBlockIndex[hashBestChain];
    nBestHeight = pindexBest->nHeight;
    nBestChainTrust = pindexBest->nChainTrust;

    printf("STARTUP-PERF: best_chain %" PRId64 "ms\n", GetTimeMillis() - nPhaseStart);

    // ---- setStakeSeen: only populate for recent blocks (DoS protection) ----
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
    // This fixes nodes stuck on the wrong fork after consensus rule changes.
    {
        CBlockIndex* pindexBetter = NULL;
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

    // triangles: load hashSyncCheckpoint (best-effort, non-fatal)
    if (!ReadSyncCheckpoint(Checkpoints::hashSyncCheckpoint))
        printf("LoadBlockIndex(): no sync checkpoint in DB, using default\n");
    else
        printf("LoadBlockIndex(): synchronized checkpoint %s\n", Checkpoints::hashSyncCheckpoint.ToString().c_str());
    // If the stored checkpoint isn't in our index, reset to genesis so we don't assert-crash
    if (!mapBlockIndex.count(Checkpoints::hashSyncCheckpoint))
    {
        printf("LoadBlockIndex(): sync checkpoint not in index, resetting to genesis\n");
        Checkpoints::hashSyncCheckpoint = (!fTestNet ? hashGenesisBlockOfficial : hashGenesisBlockTestNet);
    }

    // Load bnBestInvalidTrust, OK if it doesn't exist
    CBigNum bnBestInvalidTrust;
    ReadBestInvalidTrust(bnBestInvalidTrust);
    nBestInvalidTrust = bnBestInvalidTrust.getuint256();

    // Verify blocks in the best chain
    nPhaseStart = GetTimeMillis();
    int nCheckLevel = GetArg("-checklevel", 1);
    int nCheckDepth = GetArg( "-checkblocks", 50);
    if (nCheckDepth == 0)
        nCheckDepth = 1000000000; // suffices until the year 19000
    if (nCheckDepth > nBestHeight)
        nCheckDepth = nBestHeight;
    printf("Verifying last %i blocks at level %i\n", nCheckDepth, nCheckLevel);
    CBlockIndex* pindexFork = NULL;
    map<pair<unsigned int, unsigned int>, CBlockIndex*> mapBlockPos;
    for (CBlockIndex* pindex = pindexBest; pindex && pindex->pprev; pindex = pindex->pprev)
    {
        if (fRequestShutdown || pindex->nHeight < nBestHeight-nCheckDepth)
            break;
        CBlock block;
        if (!block.ReadFromDisk(pindex))
            return error("LoadBlockIndex() : block.ReadFromDisk failed");
        // check level 1: verify block validity
        // check level 7: verify block signature too
        if (nCheckLevel>0 && !block.CheckBlock(true, true, (nCheckLevel>6)))
        {
            printf("LoadBlockIndex() : *** found bad block at %d, hash=%s\n", pindex->nHeight, pindex->GetBlockHash().ToString().c_str());
            pindexFork = pindex->pprev;
        }
        // check level 2: verify transaction index validity
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
                    // check level 3: checker transaction hashes
                    if (nCheckLevel>2 || pindex->nFile != txindex.pos.nFile || pindex->nBlockPos != txindex.pos.nBlockPos)
                    {
                        // either an error or a duplicate transaction
                        CTransaction txFound;
                        if (!txFound.ReadFromDisk(txindex.pos))
                        {
                            printf("LoadBlockIndex() : *** cannot read mislocated transaction %s\n", hashTx.ToString().c_str());
                            pindexFork = pindex->pprev;
                        }
                        else
                            if (txFound.GetHash() != hashTx) // not a duplicate tx
                            {
                                printf("LoadBlockIndex(): *** invalid tx position for %s\n", hashTx.ToString().c_str());
                                pindexFork = pindex->pprev;
                            }
                    }
                    // check level 4: verify spent inputs were removed from UTXO set
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
        // Reorg back to the fork
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

// ============================================================================
// Address index methods
// ============================================================================

bool CTxDB::ReadAddressBalance(int nType, const uint160& hashBytes, int64_t& nBalance)
{
    return Read(make_pair(string("addrbal"), CAddressBalanceKey(nType, hashBytes)), nBalance);
}

bool CTxDB::WriteAddressBalance(int nType, const uint160& hashBytes, int64_t nBalance)
{
    return Write(make_pair(string("addrbal"), CAddressBalanceKey(nType, hashBytes)), nBalance);
}

bool CTxDB::ReadAddressUtxo(int nType, const uint160& hashBytes, const uint256& txhash, int nIndex, int64_t& nValue, int& nHeight)
{
    CAddressUtxoValue val;
    if (!Read(make_pair(string("addrutxo"), CAddressUtxoKey(nType, hashBytes, txhash, nIndex)), val))
        return false;
    nValue = val.nValue;
    nHeight = val.nHeight;
    return true;
}

bool CTxDB::WriteAddressUtxo(int nType, const uint160& hashBytes, const uint256& txhash, int nIndex, int64_t nValue, int nHeight, const CScript& script)
{
    return Write(make_pair(string("addrutxo"), CAddressUtxoKey(nType, hashBytes, txhash, nIndex)),
                 CAddressUtxoValue(nValue, nHeight, script));
}

bool CTxDB::EraseAddressUtxo(int nType, const uint160& hashBytes, const uint256& txhash, int nIndex)
{
    return Erase(make_pair(string("addrutxo"), CAddressUtxoKey(nType, hashBytes, txhash, nIndex)));
}

bool CTxDB::WriteAddressTxId(int nType, const uint160& hashBytes, int nHeight, int nTxIndex, const uint256& txhash)
{
    return Write(make_pair(string("addrtxid"), CAddressTxIdKey(nType, hashBytes, nHeight, nTxIndex, txhash)), (char)0);
}

bool CTxDB::EraseAddressTxId(int nType, const uint160& hashBytes, int nHeight, int nTxIndex, const uint256& txhash)
{
    return Erase(make_pair(string("addrtxid"), CAddressTxIdKey(nType, hashBytes, nHeight, nTxIndex, txhash)));
}

bool CTxDB::GetAddressUtxos(int nType, const uint160& hashBytes, std::vector<std::pair<COutPoint, std::pair<int64_t, int> > >& vUtxos)
{
    vUtxos.clear();

    // Build the key prefix to seek to
    CDataStream ssKeyPrefix(SER_DISK, CLIENT_VERSION);
    ssKeyPrefix << make_pair(string("addrutxo"), CAddressUtxoKey(nType, hashBytes, uint256(0), 0));
    std::string strPrefixBegin = ssKeyPrefix.str();

    leveldb::Iterator* it = pdb->NewIterator(leveldb::ReadOptions());
    for (it->Seek(strPrefixBegin); it->Valid(); it->Next())
    {
        // Deserialize the key
        CDataStream ssKey(it->key().data(), it->key().data() + it->key().size(), SER_DISK, CLIENT_VERSION);
        std::string strKeyType;
        CAddressUtxoKey utxoKey;
        ssKey >> strKeyType;
        if (strKeyType != "addrutxo")
            break;
        ssKey >> utxoKey;
        if (utxoKey.nType != nType || utxoKey.hashBytes != hashBytes)
            break;

        // Deserialize the value
        CDataStream ssValue(it->value().data(), it->value().data() + it->value().size(), SER_DISK, CLIENT_VERSION);
        CAddressUtxoValue utxoValue;
        ssValue >> utxoValue;

        COutPoint outpoint(utxoKey.txhash, utxoKey.nIndex);
        vUtxos.push_back(make_pair(outpoint, make_pair(utxoValue.nValue, utxoValue.nHeight)));
    }
    delete it;
    return true;
}

bool CTxDB::GetAddressTxIds(int nType, const uint160& hashBytes, int nStartHeight, int nEndHeight, std::vector<uint256>& vTxIds)
{
    vTxIds.clear();

    // Build the key prefix to seek to
    CDataStream ssKeyPrefix(SER_DISK, CLIENT_VERSION);
    ssKeyPrefix << make_pair(string("addrtxid"), CAddressTxIdKey(nType, hashBytes, nStartHeight, 0, uint256(0)));
    std::string strPrefixBegin = ssKeyPrefix.str();

    leveldb::Iterator* it = pdb->NewIterator(leveldb::ReadOptions());
    for (it->Seek(strPrefixBegin); it->Valid(); it->Next())
    {
        CDataStream ssKey(it->key().data(), it->key().data() + it->key().size(), SER_DISK, CLIENT_VERSION);
        std::string strKeyType;
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
    delete it;
    return true;
}

// ---------- UTXO database methods ----------

bool CTxDB::ReadUtxo(const uint256& hash, unsigned int n, CUtxoEntry& entry)
{
    entry.SetNull();
    return Read(make_pair(string("u"), make_pair(hash, n)), entry);
}

bool CTxDB::WriteUtxo(const uint256& hash, unsigned int n, const CUtxoEntry& entry)
{
    return Write(make_pair(string("u"), make_pair(hash, n)), entry);
}

bool CTxDB::EraseUtxo(const uint256& hash, unsigned int n)
{
    return Erase(make_pair(string("u"), make_pair(hash, n)));
}

bool CTxDB::HaveUtxo(const uint256& hash, unsigned int n)
{
    if (Exists(make_pair(string("u"), make_pair(hash, n))))
        return true;

    // Lazy fallback: check old CTxIndex vSpent for databases upgrading from pre-UTXO format
    CTxIndex txindex;
    if (ReadTxIndex(hash, txindex))
    {
        if (n < txindex.vSpent.size() && txindex.vSpent[n].IsNull())
            return true; // vSpent[n] is null = output NOT spent = UTXO exists
    }

    return false;
}

int64_t CTxDB::SumUtxoValues(int& nCount)
{
    nCount = 0;
    int64_t nTotal = 0;

    // Seek to the start of UTXO entries (key prefix "u")
    CDataStream ssKeyPrefix(SER_DISK, CLIENT_VERSION);
    ssKeyPrefix << make_pair(string("u"), make_pair(uint256(0), (unsigned int)0));
    std::string strPrefixBegin = ssKeyPrefix.str();

    leveldb::Iterator* it = pdb->NewIterator(leveldb::ReadOptions());
    for (it->Seek(strPrefixBegin); it->Valid(); it->Next())
    {
        // Check key prefix is still "u"
        CDataStream ssKey(it->key().data(), it->key().data() + it->key().size(), SER_DISK, CLIENT_VERSION);
        std::string strKeyType;
        ssKey >> strKeyType;
        if (strKeyType != "u")
            break;

        // Deserialize the UTXO entry and sum the value
        CDataStream ssValue(it->value().data(), it->value().data() + it->value().size(), SER_DISK, CLIENT_VERSION);
        CUtxoEntry entry;
        ssValue >> entry;

        nTotal += entry.nValue;
        nCount++;
    }
    delete it;
    return nTotal;
}

