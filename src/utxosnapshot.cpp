// Copyright (c) 2024-2025 Triangles developers
// Distributed under the MIT/X11 software license

#include "utxosnapshot.h"

#include "main.h"
#include "txdb.h"
#include "checkpoints.h"
#include "util.h"
#include "ui_interface.h"

#include <filesystem>

#include <openssl/sha.h>

#include <vector>
#include <algorithm>
#include <cstdio>

namespace fs = std::filesystem;

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
            vHeaders.push_back({*pindex->phashBlock, diskindex});
            pindex = pindex->pprev;
            nCollected++;
        }
        // Reverse to height ascending order
        std::reverse(vHeaders.begin(), vHeaders.end());
    }

    // Open the chain DB once and reuse for both the UTXO count and the
    // iteration below. Backend-agnostic via the CTxDBBase abstraction.
    auto txdbHolder = MakeChainDB("r");
    CTxDBBase& txdbRead = *txdbHolder;

    // Count UTXOs first
    int nUtxoCount = 0;
    txdbRead.SumUtxoValues(nUtxoCount);

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

    // Write UTXO section using the backend-agnostic iterator (same pattern as
    // SumUtxoValues). Iteration runs outside any active batch — the contract
    // documented on CTxDBIteratorBase guarantees a stable view of committed state.
    {
        CDataStream ssKeyPrefix(SER_DISK, CLIENT_VERSION);
        ssKeyPrefix << std::pair{std::string("u"), std::pair{uint256(0), (unsigned int)0}};
        std::string strPrefixBegin = ssKeyPrefix.str();

        auto it = txdbRead.NewIterator();
        unsigned int nWritten = 0;
        for (it->Seek(strPrefixBegin); it->Valid(); it->Next()) {
            std::string strKey = it->KeyStr();
            CDataStream ssKey(strKey.data(), strKey.data() + strKey.size(), SER_DISK, CLIENT_VERSION);
            std::string strKeyType;
            ssKey >> strKeyType;
            if (strKeyType != "u")
                break;

            uint256 txhash;
            unsigned int nIndex;
            ssKey >> txhash;
            ssKey >> nIndex;

            std::string strRawValue = it->ValueStr();
            CDataStream ssValue(strRawValue.data(), strRawValue.data() + strRawValue.size(), SER_DISK, CLIENT_VERSION);
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
                  const fs::path& /*dataDir — unused; resolved per-backend via GetChainDataDir()*/,
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

    // Wipe the chain DB directory for the configured backend, then open fresh
    // via the factory. Must run before any other code touches the chain DB
    // (the global handle is opened lazily on first MakeChainDB call).
    WipeChainDataDir();

    auto txdbHolder = MakeChainDB("c+");
    if (!txdbHolder) {
        fclose(file);
        strError = "Failed to open fresh chain DB";
        return false;
    }
    CTxDBBase& txdb = *txdbHolder;

    SHA256_CTX sha256;
    SHA256_Init(&sha256);

    bool success = true;
    unsigned int nBatchSize = 0;

    if (!txdb.TxnBegin()) {
        fclose(file);
        strError = "Failed to begin chain DB transaction";
        return false;
    }

    auto flushBatch = [&]() -> bool {
        if (nBatchSize == 0)
            return true;
        if (!txdb.TxnCommit()) {
            strError = "Chain DB batch commit failed";
            return false;
        }
        if (!txdb.TxnBegin()) {
            strError = "Chain DB batch restart failed";
            return false;
        }
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

        if (!txdb.WriteBlockIndex(diskindex)) {
            success = false;
            strError = "WriteBlockIndex failed at header " + std::to_string(i);
            break;
        }
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

            if (!txdb.WriteUtxo(txhash, nIndex, entry)) {
                success = false;
                strError = "WriteUtxo failed at index " + std::to_string(i);
                break;
            }
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

    // Write metadata via the abstraction's named operations. These produce
    // bit-identical key bytes across backends, so the new DB is in the same
    // canonical state as it would be after a normal IBD.
    if (success) {
        if (!txdb.TxnBegin()) {
            success = false;
            strError = "Failed to begin metadata transaction";
        }
    }
    if (success) {
        if (!txdb.WriteHashBestChain(blockHash) ||
            !txdb.WriteDbFormat(3) ||
            !txdb.WriteVersion(DATABASE_VERSION) ||
            !txdb.TxnCommit())
        {
            success = false;
            strError = "Failed to write snapshot metadata";
        }
    }

    fclose(file);

    if (!success) {
        // Roll back any pending batch and remove the partial DB so the next
        // startup begins from a clean slate.
        txdb.TxnAbort();
        printf("UtxoSnapshot: load failed: %s\n", strError.c_str());
        WipeChainDataDir();
        return false;
    }

    printf("UtxoSnapshot: successfully loaded %d headers + %d UTXOs at height %d\n",
           numHeaders, numUtxos, height);

    return true;
}

} // namespace UtxoSnapshot
