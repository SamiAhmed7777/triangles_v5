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
        bool fCopyOK = true;
        {
            // W2 root cause: this iterator MUST be destroyed before
            // source.Close(). Live LevelDB iterators hold a reference to the
            // current Version; deleting the DB with one outstanding trips
            // `dummy_versions_.next_ == &dummy_versions_` in
            // leveldb::VersionSet::~VersionSet (version_set.cc:755) and
            // aborts the daemon AFTER verification but BEFORE the marker is
            // removed — which is what produced the original H4 symptom.
            // Scoping the iterator here guarantees every Close() below runs
            // with it already dead, on the success AND error paths.
            auto it = source.NewIterator();
            for (it->Seek(std::string()); it->Valid(); it->Next())
            {
                if (!destination.WriteRawRecordForMigration(it->KeyStr(), it->ValueStr())) {
                    strError = "failed to write migrated record to RocksDB";
                    fCopyOK = false;
                    break;
                }

                if (++nCopied % 100000 == 0)
                {
                    if (!destination.TxnCommit()) {
                        strError = "failed to commit RocksDB migration batch";
                        fCopyOK = false;
                        break;
                    }
                    printf("ChainDB migration: copied %lld / %lld records\n",
                           (long long)nCopied, (long long)srcStats.nRecords);
                    if (!destination.TxnBegin()) {
                        strError = "failed to begin RocksDB migration batch";
                        fCopyOK = false;
                        break;
                    }
                }
            }
        } // iterator destroyed here — before any Close()
        if (!fCopyOK) {
            destination.TxnAbort(); // safe no-op if the batch was already consumed
            source.Close();
            destination.Close();
            return false;
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

        // H4: Marker removal must be verified, not assumed. The previous
        // implementation called fs::remove() and ignored the return code, which
        // silently left the marker on disk after a successful migration. On
        // the next startup init.cpp's fCrashedMigration check would then
        // trigger a re-migration of the (already-good) RocksDB on every
        // restart, eventually destroying the chain state.
        //
        // Three defenses:
        //   1. Use the non-throwing error_code overload so a permission
        //      error doesn't propagate as an uncaught exception.
        //   2. After remove(), confirm the file is actually gone. fs::remove
        //      returns true if the file didn't exist, which is also success
        //      but worth distinguishing.
        //   3. Retry once with a short delay. On Windows, antivirus and
        //      indexer handles can transiently hold the marker file open
        //      even after our process closed it; a single retry usually
        //      wins. If the second attempt also leaves the file, treat the
        //      migration as FAILED — surface the error to the operator
        //      instead of letting init.cpp's fCrashedMigration logic
        //      destroy working data on the next startup.
        {
            std::error_code ec;
            fs::remove(markerPath, ec);
            if (ec) {
                strError = "could not remove migration marker " + markerPath.string() +
                           ": " + ec.message();
                return false;
            }
            if (fs::exists(markerPath)) {
                // Retry once — handles Windows AV/indexer transient locks.
                MilliSleep(100);
                std::error_code ec2;
                fs::remove(markerPath, ec2);
                if (ec2 || fs::exists(markerPath)) {
                    strError = "migration marker " + markerPath.string() +
                               " could not be removed after retry; refusing to leave it on disk " +
                               "(would trigger re-migration on next startup). " +
                               std::string(ec2 ? ec2.message().c_str() : "");
                    return false;
                }
            }
        }
    }
    catch (std::exception& e) {
        strError = e.what();
        return false;
    }

    printf("ChainDB migration: complete. Legacy LevelDB was left untouched at %s\n",
           levelPath.string().c_str());
    return true;
}
