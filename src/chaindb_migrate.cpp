// Copyright (c) 2026 The Triangles developers.
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chaindb_migrate.h"

#include "txdb-leveldb.h"
#include "txdb-rocksdb.h"
#include "util.h"

#include <filesystem>
#include <fstream>
#include <memory>

namespace fs = std::filesystem;

namespace {

struct ChainDbStats
{
    int64_t nRecords = 0;
    int64_t nUtxos = 0;
    int64_t nUtxoValue = 0;
    uint256 hashBestChain = 0;
    int nDbFormat = 0;
};

bool CollectStats(CTxDBBase& db, ChainDbStats& stats, std::string& strError)
{
    stats = ChainDbStats();

    auto it = db.NewIterator();
    for (it->Seek(std::string()); it->Valid(); it->Next())
        stats.nRecords++;

    int nUtxos = 0;
    stats.nUtxoValue = db.SumUtxoValues(nUtxos);
    stats.nUtxos = nUtxos;
    db.ReadHashBestChain(stats.hashBestChain);
    db.ReadDbFormat(stats.nDbFormat);

    if (stats.nRecords <= 0) {
        strError = "source chain database contains no records";
        return false;
    }
    return true;
}

bool StatsMatch(const ChainDbStats& src, const ChainDbStats& dst, std::string& strError)
{
    if (src.nRecords != dst.nRecords) {
        strError = strprintf("record count mismatch after migration: source=%lld rocksdb=%lld",
                             (long long)src.nRecords, (long long)dst.nRecords);
        return false;
    }
    if (src.nUtxos != dst.nUtxos || src.nUtxoValue != dst.nUtxoValue) {
        strError = strprintf("UTXO mismatch after migration: source=(%lld,%lld) rocksdb=(%lld,%lld)",
                             (long long)src.nUtxos, (long long)src.nUtxoValue,
                             (long long)dst.nUtxos, (long long)dst.nUtxoValue);
        return false;
    }
    if (src.hashBestChain != dst.hashBestChain) {
        strError = strprintf("best-chain hash mismatch after migration: source=%s rocksdb=%s",
                             src.hashBestChain.ToString().c_str(),
                             dst.hashBestChain.ToString().c_str());
        return false;
    }
    if (src.nDbFormat != dst.nDbFormat) {
        strError = strprintf("dbformat mismatch after migration: source=%d rocksdb=%d",
                             src.nDbFormat, dst.nDbFormat);
        return false;
    }
    return true;
}

} // namespace

bool MaybeMigrateLevelDbToRocksDb(bool fForce, std::string& strError)
{
    strError.clear();

    const fs::path dataDir = GetDataDir();
    const fs::path levelPath = dataDir / "txleveldb";
    const fs::path rocksPath = dataDir / "rocksdb";
    const fs::path markerPath = rocksPath / "MIGRATION_INCOMPLETE";

    if (!fs::exists(levelPath))
        return true;

    if (fs::exists(rocksPath)) {
        if (fs::exists(markerPath)) {
            printf("ChainDB migration: removing incomplete previous RocksDB migration\n");
            fs::remove_all(rocksPath);
        }
        else if (!fForce)
            return true;
        else {
            printf("ChainDB migration: removing existing RocksDB directory due to -migratechaindbforce\n");
            fs::remove_all(rocksPath);
        }
    }

    printf("ChainDB migration: copying LevelDB chain state to RocksDB...\n");
    printf("ChainDB migration: source=%s destination=%s\n",
           levelPath.string().c_str(), rocksPath.string().c_str());

    try {
        fs::create_directories(rocksPath);
        {
            std::ofstream marker(markerPath);
            marker << "RocksDB migration in progress. Safe to delete this directory and retry.\n";
            marker.flush();
            if (!marker.good()) {
                // Without the marker a crashed migration would be
                // indistinguishable from a complete one — refuse to start.
                strError = "could not write migration marker " + markerPath.string();
                return false;
            }
        }

        CTxDB source("r");
        CRocksTxDB destination("c+");

        ChainDbStats srcStats;
        if (!CollectStats(source, srcStats, strError)) {
            source.Close();
            destination.Close();
            return false;
        }

        if (!destination.TxnBegin()) {
            strError = "failed to begin RocksDB migration batch";
            source.Close();
            destination.Close();
            return false;
        }

        int64_t nCopied = 0;
        auto it = source.NewIterator();
        for (it->Seek(std::string()); it->Valid(); it->Next())
        {
            if (!destination.WriteRawRecordForMigration(it->KeyStr(), it->ValueStr())) {
                destination.TxnAbort();
                strError = "failed to write migrated record to RocksDB";
                source.Close();
                destination.Close();
                return false;
            }

            if (++nCopied % 100000 == 0)
            {
                if (!destination.TxnCommit()) {
                    strError = "failed to commit RocksDB migration batch";
                    source.Close();
                    destination.Close();
                    return false;
                }
                printf("ChainDB migration: copied %lld / %lld records\n",
                       (long long)nCopied, (long long)srcStats.nRecords);
                if (!destination.TxnBegin()) {
                    strError = "failed to begin RocksDB migration batch";
                    source.Close();
                    destination.Close();
                    return false;
                }
            }
        }

        if (!destination.TxnCommit()) {
            strError = "failed to commit final RocksDB migration batch";
            source.Close();
            destination.Close();
            return false;
        }

        ChainDbStats dstStats;
        if (!CollectStats(destination, dstStats, strError)) {
            source.Close();
            destination.Close();
            return false;
        }
        if (!StatsMatch(srcStats, dstStats, strError)) {
            source.Close();
            destination.Close();
            return false;
        }

        printf("ChainDB migration: verified %lld records, %lld UTXOs, best=%s\n",
               (long long)dstStats.nRecords,
               (long long)dstStats.nUtxos,
               dstStats.hashBestChain.ToString().substr(0,20).c_str());

        source.Close();
        destination.Close();
        fs::remove(markerPath);
    }
    catch (std::exception& e) {
        strError = e.what();
        return false;
    }

    printf("ChainDB migration: complete. Legacy LevelDB was left untouched at %s\n",
           levelPath.string().c_str());
    return true;
}
