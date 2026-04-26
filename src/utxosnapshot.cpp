// Copyright (c) 2024-2025 Triangles developers
// Distributed under the MIT/X11 software license

#include "utxosnapshot.h"

#include "main.h"
#include "txdb.h"
#include "checkpoints.h"
#include "util.h"
#include "ui_interface.h"

#include <filesystem>

#include <leveldb/db.h>
#include <leveldb/write_batch.h>
#include <leveldb/cache.h>
#include <leveldb/filter_policy.h>

#include <openssl/sha.h>

#include <vector>
#include <algorithm>
#include <cstdio>

namespace fs = std::filesystem;

// Global LevelDB pointer (defined in txdb-leveldb.cpp)
extern leveldb::DB *txdb;

namespace UtxoSnapshot {

// ---------------------------------------------------------------------------
// DumpSnapshot - create a UTXO snapshot from the current chain state
// ---------------------------------------------------------------------------

bool DumpSnapshot(const fs::path& destPath,
                  unsigned int nHeaders,
                  std::string& strError)
{
    LOCK(cs_main);

    if (!pindexBest) {
        strError = "No best block - chain not loaded";
        return false;
    }

    // Collect block index entries (last nHeaders blocks, height ascending)
    std::vector<std::pair<uint256, CDiskBlockIndex>> vHeaders;
    vHeaders.reserve(nHeaders);
    {
        CBlockIndex* pindex = pindexBest;
        unsigned int nCollected = 0;
        while (pindex && nCollected < nHeaders) {
            CDiskBlockIndex diskindex(pindex);
            vHeaders.push_back(std::make_pair(*pindex->phashBlock, diskindex));
            pindex = pindex->pprev;
            nCollected++;
        }
        // Reverse to height ascending order
        std::reverse(vHeaders.begin(), vHeaders.end());
    }

    // Count UTXOs first
    int nUtxoCount = 0;
    {
        auto txdbRead_holder = MakeChainDB("r"); CTxDBBase& txdbRead = *txdbRead_holder;
        txdbRead.SumUtxoValues(nUtxoCount);
    }

    if (nUtxoCount == 0) {
        strError = "No UTXOs found in database";
        return false;
    }

    printf("UtxoSnapshot: dumping %d headers + %d UTXOs at height %d\n",
           (int)vHeaders.size(), nUtxoCount, nBestHeight);

    // Open output file
    FILE* file = fopen(destPath.string().c_str(), "wb");
    if (!file) {
        strError = "Cannot create file: " + destPath.string();
        return false;
    }

    // Write header (we'll seek back to fill in content_hash later)
    unsigned int magic = UTXO_SNAPSHOT_MAGIC;
    unsigned int version = UTXO_SNAPSHOT_VERSION;
    unsigned int network = fTestNet ? 2 : 1;
    int height = nBestHeight;
    uint256 blockHash = hashBestChain;
    int64_t moneySupply = pindexBest->nMoneySupply;
    unsigned int numHeaders = (unsigned int)vHeaders.size();
    unsigned int numUtxos = (unsigned int)nUtxoCount;
    uint256 contentHash; // placeholder, filled after writing data

    fwrite(&magic, sizeof(magic), 1, file);
    fwrite(&version, sizeof(version), 1, file);
    fwrite(&network, sizeof(network), 1, file);
    fwrite(&height, sizeof(height), 1, file);
    fwrite(&blockHash, sizeof(blockHash), 1, file);
    fwrite(&moneySupply, sizeof(moneySupply), 1, file);
    fwrite(&numHeaders, sizeof(numHeaders), 1, file);
    fwrite(&numUtxos, sizeof(numUtxos), 1, file);
    long contentHashPos = ftell(file);
    fwrite(&contentHash, sizeof(contentHash), 1, file); // placeholder

    // Start SHA256 for content hash
    SHA256_CTX sha256;
    SHA256_Init(&sha256);

    // Write block headers section
    for (const auto& item : vHeaders) {
        CDataStream ssEntry(SER_DISK, CLIENT_VERSION);
        ssEntry << item.first;   // block hash
        ssEntry << item.second;  // CDiskBlockIndex

        // Write length-prefixed entry
        unsigned int entrySize = (unsigned int)ssEntry.size();
        std::string strEntry = ssEntry.str();
        fwrite(&entrySize, sizeof(entrySize), 1, file);
        fwrite(strEntry.data(), 1, entrySize, file);

        SHA256_Update(&sha256, &entrySize, sizeof(entrySize));
        SHA256_Update(&sha256, strEntry.data(), entrySize);
    }

    // Write UTXO section using LevelDB iterator (same pattern as SumUtxoValues)
    {

        CDataStream ssKeyPrefix(SER_DISK, CLIENT_VERSION);
        ssKeyPrefix << std::make_pair(std::string("u"), std::make_pair(uint256(0), (unsigned int)0));
        std::string strPrefixBegin = ssKeyPrefix.str();

        leveldb::Iterator* it = txdb->NewIterator(leveldb::ReadOptions());
        unsigned int nWritten = 0;
        for (it->Seek(strPrefixBegin); it->Valid(); it->Next()) {
            // Check key prefix is still "u"
            CDataStream ssKey(it->key().data(), it->key().data() + it->key().size(), SER_DISK, CLIENT_VERSION);
            std::string strKeyType;
            ssKey >> strKeyType;
            if (strKeyType != "u")
                break;

            // Extract outpoint from key
            uint256 txhash;
            unsigned int nIndex;
            ssKey >> txhash;
            ssKey >> nIndex;

            // Extract UTXO entry from value
            CDataStream ssValue(it->value().data(), it->value().data() + it->value().size(), SER_DISK, CLIENT_VERSION);
            CUtxoEntry entry;
            ssValue >> entry;

            // Serialize the UTXO record
            CDataStream ssRecord(SER_DISK, CLIENT_VERSION);
            ssRecord << txhash;
            ssRecord << nIndex;
            ssRecord << entry;

            unsigned int recordSize = (unsigned int)ssRecord.size();
            std::string strRecord = ssRecord.str();
            fwrite(&recordSize, sizeof(recordSize), 1, file);
            fwrite(strRecord.data(), 1, recordSize, file);

            SHA256_Update(&sha256, &recordSize, sizeof(recordSize));
            SHA256_Update(&sha256, strRecord.data(), recordSize);

            nWritten++;
            if (nWritten % 10000 == 0)
                printf("UtxoSnapshot: wrote %d / %d UTXOs\n", nWritten, nUtxoCount);
        }
        delete it;

        // Update actual count (in case it changed during iteration)
        if (nWritten != numUtxos) {
            numUtxos = nWritten;
            // Seek back and update numUtxos in header
            long currentPos = ftell(file);
            fseek(file, contentHashPos - sizeof(numUtxos), SEEK_SET);
            fwrite(&numUtxos, sizeof(numUtxos), 1, file);
            fseek(file, currentPos, SEEK_SET);
        }
    }

    // Finalize content hash and write it to the header
    SHA256_Final((unsigned char*)&contentHash, &sha256);
    fseek(file, contentHashPos, SEEK_SET);
    fwrite(&contentHash, sizeof(contentHash), 1, file);

    fclose(file);

    printf("UtxoSnapshot: wrote %s (%d headers, %d UTXOs, hash=%s)\n",
           destPath.string().c_str(), numHeaders, numUtxos,
           contentHash.ToString().c_str());

    return true;
}

// ---------------------------------------------------------------------------
// LoadSnapshot - load a UTXO snapshot into a fresh LevelDB
// ---------------------------------------------------------------------------

bool LoadSnapshot(const fs::path& snapshotPath,
                  const fs::path& dataDir,
                  std::string& strError)
{
    FILE* file = fopen(snapshotPath.string().c_str(), "rb");
    if (!file) {
        strError = "Cannot open snapshot file: " + snapshotPath.string();
        return false;
    }

    // Read header
    unsigned int magic, version, network;
    int height;
    uint256 blockHash;
    int64_t moneySupply;
    unsigned int numHeaders, numUtxos;
    uint256 expectedContentHash;

    if (fread(&magic, sizeof(magic), 1, file) != 1 ||
        fread(&version, sizeof(version), 1, file) != 1 ||
        fread(&network, sizeof(network), 1, file) != 1 ||
        fread(&height, sizeof(height), 1, file) != 1 ||
        fread(&blockHash, sizeof(blockHash), 1, file) != 1 ||
        fread(&moneySupply, sizeof(moneySupply), 1, file) != 1 ||
        fread(&numHeaders, sizeof(numHeaders), 1, file) != 1 ||
        fread(&numUtxos, sizeof(numUtxos), 1, file) != 1 ||
        fread(&expectedContentHash, sizeof(expectedContentHash), 1, file) != 1) {
        fclose(file);
        strError = "Truncated snapshot header";
        return false;
    }

    // Validate header
    if (magic != UTXO_SNAPSHOT_MAGIC) {
        fclose(file);
        strError = "Invalid snapshot magic (not a UTXO snapshot file)";
        return false;
    }

    if (version != UTXO_SNAPSHOT_VERSION) {
        fclose(file);
        strError = "Unsupported snapshot version: " + std::to_string(version);
        return false;
    }

    unsigned int expectedNetwork = fTestNet ? 2 : 1;
    if (network != expectedNetwork) {
        fclose(file);
        strError = "Network mismatch: snapshot is " + std::string(network == 1 ? "mainnet" : "testnet");
        return false;
    }

    if (numHeaders == 0 || numUtxos == 0) {
        fclose(file);
        strError = "Snapshot contains no data";
        return false;
    }

    // Verify snapshot block is a known checkpoint
    if (!Checkpoints::IsKnownCheckpoint(height, blockHash)) {
        fclose(file);
        strError = "Snapshot block " + blockHash.ToString() + " at height "
                 + std::to_string(height) + " is not a known checkpoint";
        return false;
    }

    printf("UtxoSnapshot: loading snapshot at height %d (%d headers, %d UTXOs)\n",
           height, numHeaders, numUtxos);

    // Create fresh LevelDB directory
    fs::path txleveldbPath = dataDir / "txleveldb";
    if (fs::exists(txleveldbPath))
        fs::remove_all(txleveldbPath);
    fs::create_directories(txleveldbPath);

    // Open LevelDB directly (not via CTxDB - it's not initialized yet)
    leveldb::Options options;
    int nCacheSizeMB = GetArg("-dbcache", 2048);
    options.block_cache = leveldb::NewLRUCache(nCacheSizeMB * 1048576);
    options.filter_policy = leveldb::NewBloomFilterPolicy(10);
    options.write_buffer_size = 64 * 1048576;
    options.max_open_files = 1000;
    options.create_if_missing = true;

    leveldb::DB* pdb = NULL;
    leveldb::Status status = leveldb::DB::Open(options, txleveldbPath.string(), &pdb);
    if (!status.ok()) {
        fclose(file);
        delete options.filter_policy;
        delete options.block_cache;
        strError = "Cannot create LevelDB: " + status.ToString();
        return false;
    }

    SHA256_CTX sha256;
    SHA256_Init(&sha256);

    leveldb::WriteBatch batch;
    bool success = true;
    unsigned int nBatchSize = 0;

    auto flushBatch = [&]() -> bool {
        if (nBatchSize == 0)
            return true;
        leveldb::Status s = pdb->Write(leveldb::WriteOptions(), &batch);
        if (!s.ok()) {
            strError = "LevelDB write failed: " + s.ToString();
            return false;
        }
        batch.Clear();
        nBatchSize = 0;
        return true;
    };

    // Read and write block headers
    printf("UtxoSnapshot: loading %d block headers...\n", numHeaders);
    uiInterface.InitMessage(_("Loading UTXO snapshot (headers)..."));

    for (unsigned int i = 0; i < numHeaders; i++) {
        unsigned int entrySize;
        if (fread(&entrySize, sizeof(entrySize), 1, file) != 1 || entrySize > 10000) {
            success = false;
            strError = "Invalid header entry size at index " + std::to_string(i);
            break;
        }

        std::vector<char> buf(entrySize);
        if (fread(buf.data(), 1, entrySize, file) != entrySize) {
            success = false;
            strError = "Truncated header entry at index " + std::to_string(i);
            break;
        }

        SHA256_Update(&sha256, &entrySize, sizeof(entrySize));
        SHA256_Update(&sha256, buf.data(), entrySize);

        // Parse: block_hash + CDiskBlockIndex
        CDataStream ssEntry(buf.data(), buf.data() + buf.size(), SER_DISK, CLIENT_VERSION);
        uint256 entryHash;
        CDiskBlockIndex diskindex;
        ssEntry >> entryHash;
        ssEntry >> diskindex;

        // Write to LevelDB as "blockindex" key
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey << std::make_pair(std::string("blockindex"), entryHash);

        CDataStream ssValue(SER_DISK, CLIENT_VERSION);
        ssValue << diskindex;

        batch.Put(ssKey.str(), ssValue.str());
        nBatchSize++;

        if (nBatchSize >= 1000) {
            if (!flushBatch()) { success = false; break; }
        }
    }

    if (success && !flushBatch())
        success = false;

    // Read and write UTXOs
    if (success) {
        printf("UtxoSnapshot: loading %d UTXOs...\n", numUtxos);

        for (unsigned int i = 0; i < numUtxos; i++) {
            unsigned int recordSize;
            if (fread(&recordSize, sizeof(recordSize), 1, file) != 1 || recordSize > 100000) {
                success = false;
                strError = "Invalid UTXO record size at index " + std::to_string(i);
                break;
            }

            std::vector<char> buf(recordSize);
            if (fread(buf.data(), 1, recordSize, file) != recordSize) {
                success = false;
                strError = "Truncated UTXO record at index " + std::to_string(i);
                break;
            }

            SHA256_Update(&sha256, &recordSize, sizeof(recordSize));
            SHA256_Update(&sha256, buf.data(), recordSize);

            // Parse: txid + output_index + CUtxoEntry
            CDataStream ssRecord(buf.data(), buf.data() + buf.size(), SER_DISK, CLIENT_VERSION);
            uint256 txhash;
            unsigned int nIndex;
            CUtxoEntry entry;
            ssRecord >> txhash;
            ssRecord >> nIndex;
            ssRecord >> entry;

            // Write to LevelDB with "u" prefix key
            CDataStream ssKey(SER_DISK, CLIENT_VERSION);
            ssKey << std::make_pair(std::string("u"), std::make_pair(txhash, nIndex));

            CDataStream ssValue(SER_DISK, CLIENT_VERSION);
            ssValue << entry;

            batch.Put(ssKey.str(), ssValue.str());
            nBatchSize++;

            if (nBatchSize >= 50000) {
                if (!flushBatch()) { success = false; break; }

                if (i % 50000 == 0) {
                    std::string strMsg = strprintf(_("Loading UTXO snapshot (%d%%)..."),
                                                   i * 100 / numUtxos);
                    uiInterface.InitMessage(strMsg);
                    printf("UtxoSnapshot: loaded %d / %d UTXOs\n", i, numUtxos);
                }
            }
        }

        if (success && !flushBatch())
            success = false;
    }

    // Verify content hash
    if (success) {
        uint256 actualHash;
        SHA256_Final((unsigned char*)&actualHash, &sha256);

        if (actualHash != expectedContentHash) {
            success = false;
            strError = "Content hash mismatch - snapshot may be corrupted";
        }
    }

    // Write metadata
    if (success) {
        leveldb::WriteBatch metaBatch;

        // hashBestChain
        CDataStream ssKey1(SER_DISK, CLIENT_VERSION);
        ssKey1 << std::string("hashBestChain");
        CDataStream ssVal1(SER_DISK, CLIENT_VERSION);
        ssVal1 << blockHash;
        metaBatch.Put(ssKey1.str(), ssVal1.str());

        // dbformat = 3
        CDataStream ssKey2(SER_DISK, CLIENT_VERSION);
        ssKey2 << std::string("dbformat");
        CDataStream ssVal2(SER_DISK, CLIENT_VERSION);
        ssVal2 << (int)3;
        metaBatch.Put(ssKey2.str(), ssVal2.str());

        // version
        CDataStream ssKey3(SER_DISK, CLIENT_VERSION);
        ssKey3 << std::string("version");
        CDataStream ssVal3(SER_DISK, CLIENT_VERSION);
        ssVal3 << DATABASE_VERSION;
        metaBatch.Put(ssKey3.str(), ssVal3.str());

        leveldb::Status s = pdb->Write(leveldb::WriteOptions(), &metaBatch);
        if (!s.ok()) {
            success = false;
            strError = "Failed to write metadata: " + s.ToString();
        }
    }

    // Clean up LevelDB
    delete pdb;
    delete options.filter_policy;
    delete options.block_cache;

    fclose(file);

    if (!success) {
        // Remove corrupted/incomplete database
        printf("UtxoSnapshot: load failed: %s\n", strError.c_str());
        if (fs::exists(txleveldbPath))
            fs::remove_all(txleveldbPath);
        return false;
    }

    printf("UtxoSnapshot: successfully loaded %d headers + %d UTXOs at height %d\n",
           numHeaders, numUtxos, height);

    return true;
}

} // namespace UtxoSnapshot
