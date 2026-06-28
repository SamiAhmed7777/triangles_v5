// Copyright (c) 2026 The Triangles developers.
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "txdb-rocksdb.h"

#include <map>

#include <filesystem>

#include <boost/version.hpp>

#include <rocksdb/cache.h>
#include <rocksdb/filter_policy.h>
#include <rocksdb/iterator.h>
#include <rocksdb/slice.h>
#include <rocksdb/table.h>
#include <rocksdb/write_batch.h>

#include "kernel.h"
#include "checkpoints.h"
#include "txdb.h"
#include "util.h"
#include "ui_interface.h"
#include "addressindex.h"
#include "main.h"

using namespace std;
namespace fs = std::filesystem;

// Global pointer for the RocksDB instance, shared across CRocksTxDB instances
// the same way the LevelDB backend shares its txdb singleton.
static rocksdb::DB* g_rocksdb = nullptr;
static rocksdb::ColumnFamilyHandle* g_cf_handles[5] = {};  // indexed by CF_ enum
static bool g_cf_enabled = false;

// Non-batched writes bypass WAL fsync. The TxnCommit path handles durability;
// crash recovery replays from block files anyway. Default WriteOptions may
// vary across RocksDB versions, so we pin sync=false explicitly.
static const rocksdb::WriteOptions g_fastWriteOpts = []{
    rocksdb::WriteOptions wo;
    wo.sync = false;
    return wo;
}();

namespace {

// rocksdb::DB::Open shipped a raw DB** overload for years; newer releases
// (Homebrew's macOS rocksdb 10.x) replaced it with std::unique_ptr<DB>*.
// SFINAE picks whichever overload the linked rocksdb actually has —
// `int` is preferred over `long`, so when DB** exists, the first overload
// wins; otherwise the unique_ptr fallback runs.
template<typename T>
inline auto OpenRocksDBImpl(const rocksdb::Options& opts, const std::string& path,
                            T** dbptr, int)
    -> decltype(rocksdb::DB::Open(opts, path, dbptr))
{
    return rocksdb::DB::Open(opts, path, dbptr);
}

template<typename T>
inline rocksdb::Status OpenRocksDBImpl(const rocksdb::Options& opts, const std::string& path,
                                       T** dbptr, long)
{
    std::unique_ptr<T> tmp;
    auto s = rocksdb::DB::Open(opts, path, &tmp);
    if (s.ok()) *dbptr = tmp.release();
    return s;
}

inline rocksdb::Status OpenRocksDB(const rocksdb::Options& opts,
                                   const std::string& path,
                                   rocksdb::DB** dbptr)
{
    return OpenRocksDBImpl(opts, path, dbptr, 0);
}

// Same SFINAE pattern for the column-family Open overload.
// Some RocksDB versions (MSYS2 MinGW) ship only the unique_ptr signature.
template<typename T>
inline auto OpenRocksDBCFImpl(const rocksdb::Options& opts, const std::string& path,
                              const std::vector<rocksdb::ColumnFamilyDescriptor>& cfDescs,
                              std::vector<rocksdb::ColumnFamilyHandle*>* handles,
                              T** dbptr, int)
    -> decltype(rocksdb::DB::Open(opts, path, cfDescs, handles, dbptr))
{
    return rocksdb::DB::Open(opts, path, cfDescs, handles, dbptr);
}

template<typename T>
inline rocksdb::Status OpenRocksDBCFImpl(const rocksdb::Options& opts, const std::string& path,
                                         const std::vector<rocksdb::ColumnFamilyDescriptor>& cfDescs,
                                         std::vector<rocksdb::ColumnFamilyHandle*>* handles,
                                         T** dbptr, long)
{
    std::unique_ptr<T> tmp;
    auto s = rocksdb::DB::Open(opts, path, cfDescs, handles, &tmp);
    if (s.ok()) *dbptr = tmp.release();
    return s;
}

inline rocksdb::Status OpenRocksDBCF(const rocksdb::Options& opts,
                                     const std::string& path,
                                     const std::vector<rocksdb::ColumnFamilyDescriptor>& cfDescs,
                                     std::vector<rocksdb::ColumnFamilyHandle*>* handles,
                                     rocksdb::DB** dbptr)
{
    return OpenRocksDBCFImpl(opts, path, cfDescs, handles, dbptr, 0);
}

} // anonymous namespace

static rocksdb::Options GetRocksOptions()
{
    rocksdb::Options opts;
    opts.create_if_missing = false;
    opts.compression = rocksdb::kSnappyCompression;
    opts.max_open_files = -1;
    opts.write_buffer_size = 256 * 1048576;
    opts.max_write_buffer_number = 4;
    opts.IncreaseParallelism();             // Multi-threaded compaction.
    opts.OptimizeLevelStyleCompaction();    // Sensible defaults for a LSM workload.

    rocksdb::BlockBasedTableOptions table_opts;
    int nCacheSizeMB = GetArg("-dbcache", 2048);
    table_opts.block_cache = rocksdb::NewLRUCache(static_cast<size_t>(nCacheSizeMB) * 1048576);
    table_opts.filter_policy.reset(rocksdb::NewBloomFilterPolicy(10, false));
    opts.table_factory.reset(rocksdb::NewBlockBasedTableFactory(table_opts));

    return opts;
}

// ─── Column family names ───────────────────────────────────────────────────
static const std::string CF_NAMES[] = {
    rocksdb::kDefaultColumnFamilyName,  // CF_DEFAULT (index 0)
    "blockindex",                       // CF_BLOCKINDEX (index 1)
    "txindex",                          // CF_TXINDEX (index 2)
    "utxo",                             // CF_UTXO (index 3)
    "addrindex",                        // CF_ADDRINDEX (index 4)
};
static constexpr int CF_COUNT = 5;

// Prefix-to-CF routing table. Keys starting with these prefixes go to
// the indicated CF index. Everything else stays in CF_DEFAULT (metadata).
struct CfPrefixEntry { const char* prefix; int len; int cf_index; };
static CfPrefixEntry prefixMap_[] = {
    {"b",     1, 1},  // CF_BLOCKINDEX
    {"t",     1, 2},  // CF_TXINDEX
    {"u",     1, 3},  // CF_UTXO
    {"addrbal",  7, 4},  // CF_ADDRINDEX
    {"addrutxo", 8, 4},  // CF_ADDRINDEX
    {"addrtxid", 8, 4},  // CF_ADDRINDEX
};

static void open_rocksdb(rocksdb::Options& options, bool fRemoveOld = false)
{
    fs::path directory = GetDataDir() / "rocksdb";

    if (fRemoveOld) {
        fs::remove_all(directory);
    }

    fs::create_directory(directory);
    printf("Opening RocksDB in %s\n", directory.string().c_str());

    // Try opening with column families. First, list existing CFs.
    std::vector<std::string> existingCFs;
    rocksdb::Options listOpts = options;
    listOpts.create_if_missing = false;
    rocksdb::DB::ListColumnFamilies(listOpts, directory.string(), &existingCFs);

    bool needsCreate = (existingCFs.size() <= 1);  // Only "default" or empty

    std::vector<rocksdb::ColumnFamilyDescriptor> cfDescs;
    for (int i = 0; i < CF_COUNT; i++) {
        // Include this CF if it already exists OR if we're creating new
        bool exists = false;
        for (auto& name : existingCFs)
            if (name == CF_NAMES[i]) { exists = true; break; }
        if (exists || needsCreate) {
            rocksdb::ColumnFamilyOptions cfOpts = options;
            // Per-CF tuning:
            if (i == 3) {  // UTXO: optimize for point lookups
                cfOpts.OptimizeForPointLookup(static_cast<size_t>(GetArg("-dbcache", 2048)));
            } else if (i == 4) {  // addrindex: optimize for scans
                cfOpts.OptimizeLevelStyleCompaction(cfOpts.write_buffer_size);
            }
            cfDescs.push_back(rocksdb::ColumnFamilyDescriptor(CF_NAMES[i], cfOpts));
        }
    }

    std::vector<rocksdb::ColumnFamilyHandle*> handles;
    rocksdb::Status status = OpenRocksDBCF(options, directory.string(),
                                                cfDescs, &handles, &g_rocksdb);
    if (!status.ok()) {
        // Fallback: open without CFs (old-style single-CF database)
        printf("RocksDB CF open failed (%s), falling back to single-CF\n", status.ToString().c_str());
        status = OpenRocksDB(options, directory.string(), &g_rocksdb);
        if (!status.ok()) {
            throw runtime_error(strprintf("open_rocksdb(): error opening database: %s",
                                          status.ToString().c_str()));
        }
        return;
    }

    // Store handles in the global array (CF names map directly to indices)
    for (size_t i = 0; i < handles.size() && i < CF_COUNT; i++) {
        // Match handle to our index by name
        std::string hname = handles[i]->GetName();
        for (int j = 0; j < CF_COUNT; j++) {
            if (hname == CF_NAMES[j]) {
                g_cf_handles[j] = handles[i];
                break;
            }
        }
    }
    g_cf_enabled = true;
}

CRocksTxDB::CRocksTxDB(const char* pszMode)
    : pdb(nullptr), activeBatch(nullptr), nVersion(0)
{
    assert(pszMode);
    fReadOnly = (!strchr(pszMode, '+') && !strchr(pszMode, 'w'));

    if (g_rocksdb) {
        pdb = g_rocksdb;
        return;
    }

    bool fCreate = strchr(pszMode, 'c');
    options = GetRocksOptions();
    options.create_if_missing = fCreate;

    open_rocksdb(options);
    pdb = g_rocksdb;

    if (Exists(string("version")))
    {
        ReadVersion(nVersion);
        printf("RocksDB transaction index version is %d\n", nVersion);

        if (nVersion < DATABASE_VERSION)
        {
            printf("Required index version is %d, removing old RocksDB database\n",
                   DATABASE_VERSION);

            delete g_rocksdb;
            g_rocksdb = pdb = nullptr;
            delete activeBatch;
            activeBatch = nullptr;

            open_rocksdb(options, true);
            pdb = g_rocksdb;

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

    printf("Opened RocksDB successfully\n");
}

CRocksTxDB::~CRocksTxDB()
{
    delete activeBatch;
}

void CRocksTxDB::Close()
{
    delete g_rocksdb;
    g_rocksdb = pdb = nullptr;
    delete activeBatch;
    activeBatch = nullptr;
}

bool CRocksTxDB::TxnBegin()
{
    if (activeBatch)
        return true;
    activeBatch = new rocksdb::WriteBatch();
    pendingBatch.clear();
    return true;
}

bool CRocksTxDB::TxnCommit()
{
    assert(activeBatch);
    rocksdb::Status status = pdb->Write(rocksdb::WriteOptions(), activeBatch);
    delete activeBatch;
    activeBatch = nullptr;
    pendingBatch.clear();
    if (!status.ok()) {
        printf("ERROR: RocksDB batch commit failure: %s\n", status.ToString().c_str());
        printf("ERROR: This may indicate disk full, corruption, or permissions issue.\n");
        printf("ERROR: Chain state may be inconsistent - immediate investigation required!\n");
        return false;
    }
    return true;
}

bool CRocksTxDB::TxnAbort()
{
    delete activeBatch;
    activeBatch = nullptr;
    pendingBatch.clear();
    return true;
}

namespace {

class CRocksDBIterator final : public CTxDBIteratorBase {
public:
    explicit CRocksDBIterator(rocksdb::Iterator* pit) : pit(pit) {}
    ~CRocksDBIterator() override { delete pit; }

    void Seek(const std::string& key) override { pit->Seek(key); }
    bool Valid() const override { return pit->Valid(); }
    void Next() override { pit->Next(); }
    std::string KeyStr() const override   { return pit->key().ToString(); }
    std::string ValueStr() const override { return pit->value().ToString(); }

private:
    rocksdb::Iterator* pit;
};

} // anonymous namespace

bool CRocksTxDB::ScanBatch(const std::string& key, std::string* value, bool* deleted) const
{
    assert(activeBatch);
    *deleted = false;
    auto it = pendingBatch.find(key);
    if (it == pendingBatch.end())
        return false;
    if (!it->second.has_value()) {
        *deleted = true;
        return true;
    }
    *value = *it->second;
    return true;
}

// ─── CF routing helper ──────────────────────────────────────────────────────
rocksdb::ColumnFamilyHandle* CRocksTxDB::GetCF(const std::string& key) const
{
    if (!g_cf_enabled)
        return nullptr;  // nullptr = default CF
    for (auto& entry : prefixMap_) {
        if ((int)key.size() >= entry.len && key.compare(0, entry.len, entry.prefix) == 0)
            return g_cf_handles[entry.cf_index];
    }
    return nullptr;  // default CF for metadata keys
}

bool CRocksTxDB::ReadRaw(const std::string& key, std::string& value) const
{
    bool readFromDb = true;
    if (activeBatch) {
        bool deleted = false;
        readFromDb = ScanBatch(key, &value, &deleted) == false;
        if (deleted)
            return false;
    }
    if (readFromDb) {
        rocksdb::ReadOptions ro;
        auto* cf = GetCF(key);
        rocksdb::Status status = cf ? pdb->Get(ro, cf, key, &value)
                                     : pdb->Get(ro, key, &value);
        if (!status.ok()) {
            if (status.IsNotFound()) {
                // If CFs are enabled and key wasn't in the target CF, also
                // check the default CF (handles data written before CF migration)
                if (g_cf_enabled && cf) {
                    rocksdb::Status status2 = pdb->Get(ro, key, &value);
                    if (!status2.ok()) return false;
                    return true;
                }
                return false;
            }
            printf("RocksDB read failure: %s\n", status.ToString().c_str());
            return false;
        }
    }
    return true;
}

bool CRocksTxDB::WriteRaw(const std::string& key, const std::string& value)
{
    auto* cf = GetCF(key);
    if (activeBatch) {
        if (cf)
            activeBatch->Put(cf, key, value);
        else
            activeBatch->Put(key, value);
        pendingBatch[key] = value;
        return true;
    }
    rocksdb::Status status = cf ? pdb->Put(g_fastWriteOpts, cf, key, value)
                                  : pdb->Put(g_fastWriteOpts, key, value);
    if (!status.ok()) {
        printf("RocksDB write failure: %s\n", status.ToString().c_str());
        return false;
    }
    return true;
}

bool CRocksTxDB::EraseRaw(const std::string& key)
{
    if (!pdb)
        return false;
    auto* cf = GetCF(key);
    if (activeBatch) {
        if (cf)
            activeBatch->Delete(cf, key);
        else
            activeBatch->Delete(key);
        pendingBatch[key] = std::nullopt;
        return true;
    }
    rocksdb::Status status = cf ? pdb->Delete(rocksdb::WriteOptions(), cf, key)
                                  : pdb->Delete(rocksdb::WriteOptions(), key);
    return (status.ok() || status.IsNotFound());
}

bool CRocksTxDB::ExistsRaw(const std::string& key) const
{
    std::string unused;

    if (activeBatch) {
        bool deleted = false;
        bool inBatch = ScanBatch(key, &unused, &deleted);
        if (inBatch) {
            return !deleted;
        }
    }

    auto* cf = GetCF(key);
    rocksdb::ReadOptions ro;
    rocksdb::Status status = cf ? pdb->Get(ro, cf, key, &unused)
                                  : pdb->Get(ro, key, &unused);
    if (status.IsNotFound() && g_cf_enabled && cf) {
        // Fallback to default CF for pre-migration data
        status = pdb->Get(ro, key, &unused);
    }
    return status.IsNotFound() == false;
}

std::unique_ptr<CTxDBIteratorBase> CRocksTxDB::NewIterator() const
{
    return std::unique_ptr<CTxDBIteratorBase>(
        new CRocksDBIterator(pdb->NewIterator(rocksdb::ReadOptions())));
}

// ─── LoadBlockIndex ─────────────────────────────────────────────────────────
// Mirrors CTxDB::LoadBlockIndex with rocksdb:: substitutions. The dbformat
// upgrade path is preserved verbatim because a freshly-imported RocksDB may
// have been migrated from a v1 LevelDB and still need the chain-trust pass.
//
// This duplication is acknowledged debt — CTxDBBase will absorb LoadBlockIndex
// into the base class in a later phase once the iterator/batch abstractions
// have proven stable across both backends.
// ────────────────────────────────────────────────────────────────────────────
static CBlockIndex *InsertBlockIndexRocks(uint256 hash)
{
    if (hash == 0)
        return nullptr;

    auto mi = mapBlockIndex.find(hash);
    if (mi != mapBlockIndex.end())
        return mi->second;

    CBlockIndex* pindexNew = new CBlockIndex();
    if (!pindexNew)
        throw runtime_error("LoadBlockIndex(): new CBlockIndex failed");
    mi = mapBlockIndex.insert(make_pair(hash, pindexNew)).first;
    pindexNew->phashBlock = &mi->first;

    return pindexNew;
}

bool CRocksTxDB::LoadBlockIndex()
{
    if (mapBlockIndex.size() > 0) {
        return true;
    }

    int nDbFormat = 1;
    ReadDbFormat(nDbFormat);
    CDiskBlockIndex::fSerializeChainTrust = (nDbFormat >= 2);

    if (CDiskBlockIndex::fSerializeChainTrust)
        printf("LoadBlockIndex(): RocksDB format v%d - nChainTrust persisted\n", nDbFormat);
    else
        printf("LoadBlockIndex(): RocksDB format v%d - will recalculate nChainTrust\n", nDbFormat);

    int64_t nPhaseStart = GetTimeMillis();
    int64_t nTotalStart = nPhaseStart;
    rocksdb::Iterator* iterator = pdb->NewIterator(rocksdb::ReadOptions());
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

        CBlockIndex* pindexNew    = InsertBlockIndexRocks(blockHash);
        pindexNew->pprev          = InsertBlockIndexRocks(diskindex.hashPrev);
        pindexNew->pnext          = InsertBlockIndexRocks(diskindex.hashNext);
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
            return error("LoadBlockIndex(): CheckIndex failed at %d", pindexNew->nHeight);
        }

        iterator->Next();
    }
    delete iterator;
    printf("STARTUP-PERF: block_index_deserialize %" PRId64 "ms blocks=%d\n",
           GetTimeMillis() - nPhaseStart, nBlocksLoaded);

    if (fRequestShutdown)
        return true;

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
            pindex->nChainTrust = (pindex->pprev ? pindex->pprev->nChainTrust : 0)
                                + pindex->GetBlockTrust();

            if (pindex->nHeight >= nLastCheckpointHeight)
            {
                pindex->nStakeModifierChecksum = GetStakeModifierChecksum(pindex);
                if (!CheckStakeModifierCheckpoints(pindex->nHeight, pindex->nStakeModifierChecksum))
                    return error("LoadBlockIndex(): Failed stake modifier checkpoint h=%d, mod=0x%016"PRIx64,
                                 pindex->nHeight, pindex->nStakeModifier);
            }

            if (++nCount % nProgressInterval == 0)
            {
                std::string strMsg = strprintf(_("Calculating chain trust... (%d%%)"),
                                               nCount * 100 / vSortedByHeight.size());
                uiInterface.InitMessage(strMsg);
            }
        }

        printf("LoadBlockIndex(): upgrading RocksDB to format v3...\n");
        uiInterface.InitMessage(_("Upgrading block index..."));
        CDiskBlockIndex::fSerializeChainTrust = true;

        rocksdb::WriteBatch batch;
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
                pdb->Write(rocksdb::WriteOptions(), &batch);
                batch.Clear();
                printf("LoadBlockIndex(): upgraded %d / %d entries\n",
                       nCount, (int)vSortedByHeight.size());
            }
        }
        CDataStream ssFmtKey(SER_DISK, CLIENT_VERSION);
        ssFmtKey << string("dbformat");
        CDataStream ssFmtValue(SER_DISK, CLIENT_VERSION);
        ssFmtValue << (int)3;
        batch.Put(ssFmtKey.str(), ssFmtValue.str());

        rocksdb::Status status = pdb->Write(rocksdb::WriteOptions(), &batch);
        if (!status.ok())
            return error("LoadBlockIndex(): failed to write upgraded block index: %s",
                         status.ToString().c_str());

        printf("LoadBlockIndex(): RocksDB upgraded to format v3 (%d entries)\n", nCount);
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
                    return error("LoadBlockIndex(): Failed stake modifier checkpoint h=%d, mod=0x%016"PRIx64,
                                 pindex->nHeight, pindex->nStakeModifier);
            }
        }
    }

    printf("STARTUP-PERF: chain_trust_and_modifiers %" PRId64 "ms\n",
           GetTimeMillis() - nPhaseStart);

    if (nDbFormat < 3)
    {
        WriteDbFormat(3);
        printf("LoadBlockIndex(): bumped RocksDB dbformat to v3\n");
    }

    nPhaseStart = GetTimeMillis();
    if (!ReadHashBestChain(hashBestChain))
    {
        if (pindexGenesisBlock == nullptr)
            return true;
        return error("LoadBlockIndex(): hashBestChain not loaded");
    }
    if (!mapBlockIndex.count(hashBestChain))
        return error("LoadBlockIndex(): hashBestChain not found in the block index");
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

    printf("LoadBlockIndex(): hashBestChain=%s height=%d trust=%s date=%s\n",
        hashBestChain.ToString().substr(0,20).c_str(), nBestHeight,
        CBigNum(nBestChainTrust).ToString().c_str(),
        DateTimeStrFormat("%x %H:%M:%S", pindexBest->GetBlockTime()).c_str());

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
            printf("LoadBlockIndex(): better chain tip %s at %d (trust %s vs %s)\n",
                pindexBetter->GetBlockHash().ToString().substr(0,20).c_str(),
                pindexBetter->nHeight,
                CBigNum(pindexBetter->nChainTrust).ToString().c_str(),
                CBigNum(nBestChainTrust).ToString().c_str());
            CBlock block;
            if (block.ReadFromDisk(pindexBetter))
            {
                CRocksTxDB txdb2;
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
        printf("LoadBlockIndex(): synchronized checkpoint %s\n",
               Checkpoints::hashSyncCheckpoint.ToString().c_str());
    if (!mapBlockIndex.count(Checkpoints::hashSyncCheckpoint))
    {
        printf("LoadBlockIndex(): sync checkpoint not in index, resetting to genesis\n");
        Checkpoints::hashSyncCheckpoint = (!fTestNet ? hashGenesisBlockOfficial
                                                    : hashGenesisBlockTestNet);
    }

    CBigNum bnBestInvalidTrust;
    ReadBestInvalidTrust(bnBestInvalidTrust);
    nBestInvalidTrust = bnBestInvalidTrust.getuint256();

    nPhaseStart = GetTimeMillis();
    int nCheckLevel = GetArg("-checklevel", 1);
    int nCheckDepth = GetArg("-checkblocks", 50);
    if (nCheckDepth == 0)
        nCheckDepth = 1000000000;
    if (nCheckDepth > nBestHeight)
        nCheckDepth = nBestHeight;
    printf("Verifying last %i blocks at level %i\n", nCheckDepth, nCheckLevel);
    CBlockIndex* pindexFork = nullptr;
    map<pair<unsigned int, unsigned int>, CBlockIndex*> mapBlockPos;
    for (CBlockIndex* pindex = pindexBest; pindex && pindex->pprev; pindex = pindex->pprev)
    {
        if (fRequestShutdown || pindex->nHeight < nBestHeight - nCheckDepth)
            break;
        CBlock block;
        if (!block.ReadFromDisk(pindex))
        {
            if (fLoadedFromSnapshot) {
                printf("LoadBlockIndex(): block %d not on disk (snapshot-sourced), skipping verification\n",
                       pindex->nHeight);
                continue;
            }
            return error("LoadBlockIndex(): block.ReadFromDisk failed");
        }
        if (nCheckLevel > 0 && !block.CheckBlock(true, true, (nCheckLevel > 6)))
        {
            printf("LoadBlockIndex(): bad block at %d, hash=%s\n",
                   pindex->nHeight, pindex->GetBlockHash().ToString().c_str());
            pindexFork = pindex->pprev;
        }
        if (nCheckLevel > 1)
        {
            pair<unsigned int, unsigned int> pos = make_pair(pindex->nFile, pindex->nBlockPos);
            mapBlockPos[pos] = pindex;
            for (const CTransaction &tx : block.vtx)
            {
                uint256 hashTx = tx.GetHash();
                CTxIndex txindex;
                if (ReadTxIndex(hashTx, txindex))
                {
                    if (nCheckLevel > 2 || pindex->nFile != txindex.pos.nFile
                        || pindex->nBlockPos != txindex.pos.nBlockPos)
                    {
                        CTransaction txFound;
                        if (!txFound.ReadFromDisk(txindex.pos))
                        {
                            printf("LoadBlockIndex(): cannot read mislocated transaction %s\n",
                                   hashTx.ToString().c_str());
                            pindexFork = pindex->pprev;
                        }
                        else if (txFound.GetHash() != hashTx)
                        {
                            printf("LoadBlockIndex(): invalid tx position for %s\n",
                                   hashTx.ToString().c_str());
                            pindexFork = pindex->pprev;
                        }
                    }
                    if (nCheckLevel > 3 && !tx.IsCoinBase())
                    {
                        for (const CTxIn &txin : tx.vin)
                        {
                            if (HaveUtxo(txin.prevout.hash, txin.prevout.n))
                            {
                                printf("LoadBlockIndex(): spent input still in UTXO set: %s:%i in %s\n",
                                       txin.prevout.hash.ToString().c_str(), txin.prevout.n,
                                       hashTx.ToString().c_str());
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
        printf("LoadBlockIndex(): moving best chain pointer back to block %d\n",
               pindexFork->nHeight);
        CBlock block;
        if (!block.ReadFromDisk(pindexFork))
            return error("LoadBlockIndex(): block.ReadFromDisk failed");
        CRocksTxDB txdb;
        block.SetBestChain(txdb, pindexFork);
    }
    printf("STARTUP-PERF: verify_blocks %" PRId64 "ms depth=%d level=%d\n",
           GetTimeMillis() - nPhaseStart, nCheckDepth, nCheckLevel);
    printf("STARTUP-PERF: load_block_index_total %" PRId64 "ms\n",
           GetTimeMillis() - nTotalStart);

    return true;
}
