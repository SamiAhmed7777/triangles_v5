// Copyright (c) 2024-2025 Triangles developers
// Distributed under the MIT/X11 software license

#include "utxosnapshot.h"

#include "main.h"
#include "txdb.h"
#include "checkpoints.h"
#include "util.h"
#include "ui_interface.h"
#include "addressindex.h"

#include <variant>

// defined in main.cpp
extern bool fAddressIndex;

#include <filesystem>

#include <openssl/sha.h>

#include <vector>
#include <algorithm>
#include <cstdio>

namespace fs = std::filesystem;

namespace UtxoSnapshot {

static constexpr unsigned int MAX_SNAPSHOT_STAKE_SEEN = 5000;

// ---------------------------------------------------------------------------
// DumpSnapshot - create a UTXO snapshot from the current chain state
// ---------------------------------------------------------------------------

bool DumpSnapshot(const fs::path& destPath,
                  std::string& strError)
{
    LOCK(cs_main);

    if (!pindexBest) {
        strError = "No best block - chain not loaded";
        return false;
    }

    // Collect every block index entry (genesis -> tip). Snapshot-loaded peers
    // need the complete index to answer getheaders/getblocks for fresh nodes;
    // a recent-only index strands those nodes at height zero.
    std::vector<std::pair<uint256, CDiskBlockIndex>> vHeaders;
    {
        CBlockIndex* pindex = pindexBest;
        while (pindex) {
            CDiskBlockIndex diskindex(pindex);
            vHeaders.push_back({*pindex->phashBlock, diskindex});
            pindex = pindex->pprev;
        }
        // Reverse to height ascending order (genesis first)
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
    // v2: size of raw blk0001.dat content embedded in this snapshot. v1
    // snapshots always write 0 here (no embedded blocks).
    unsigned int numBlocks = 0;
    {
        FILE* blkFile = fopen((GetDataDir() / "blk0001.dat").string().c_str(), "rb");
        if (blkFile) {
            fseek(blkFile, 0, SEEK_END);
            long blkSize = ftell(blkFile);
            fclose(blkFile);
            if (blkSize > 0) numBlocks = (unsigned int)blkSize;
        }
    }
    // v3: collect setStakeSeen entries (prevoutStake, nStakeTime) from the
    // last N PoS blocks. Required so a snapshot-loaded node has the recent
    // stake-collision set restored without walking blocks at startup.
    std::vector<std::pair<COutPoint, unsigned int> > vStakeSeen;
    {
        CBlockIndex* pindex = pindexBest;
        unsigned int nVisited = 0;
        while (pindex && nVisited < MAX_SNAPSHOT_STAKE_SEEN) {
            if (pindex->IsProofOfStake()) {
                vStakeSeen.push_back(std::make_pair(pindex->prevoutStake, pindex->nStakeTime));
            }
            pindex = pindex->pprev;
            nVisited++;
        }
    }
    unsigned int numStakeSeen = (unsigned int)vStakeSeen.size();
    uint256 contentHash; // placeholder, filled after writing data

    fwrite(&magic, sizeof(magic), 1, file);
    fwrite(&version, sizeof(version), 1, file);
    fwrite(&network, sizeof(network), 1, file);
    fwrite(&height, sizeof(height), 1, file);
    fwrite(&blockHash, sizeof(blockHash), 1, file);
    fwrite(&moneySupply, sizeof(moneySupply), 1, file);
    fwrite(&numHeaders, sizeof(numHeaders), 1, file);
    fwrite(&numUtxos, sizeof(numUtxos), 1, file);
    fwrite(&numBlocks, sizeof(numBlocks), 1, file); // v2+
    fwrite(&numStakeSeen, sizeof(numStakeSeen), 1, file); // v3+
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
            // Seek back and update numUtxos in header.
            // Header layout (v3):
            //   magic(4) + version(4) + network(4) + height(4) + blockHash(32)
            //   + moneySupply(8) + numHeaders(4) + numUtxos(4)
            //   + numBlocks(4) + numStakeSeen(4) + contentHash(32)
            // contentHashPos is the offset of contentHash. numUtxos is at
            // contentHashPos - sizeof(contentHash) - sizeof(numStakeSeen)
            //                  - sizeof(numBlocks) - sizeof(numUtxos).
            long currentPos = ftell(file);
            fseek(file, contentHashPos - sizeof(uint256) - sizeof(numStakeSeen)
                          - sizeof(numBlocks) - sizeof(numUtxos), SEEK_SET);
            fwrite(&numUtxos, sizeof(numUtxos), 1, file);
            fseek(file, currentPos, SEEK_SET);
        }
    }

    // v3: After UTXOs, write the setStakeSeen entries collected from the last
    // N PoS blocks. Format: a length-prefixed flat array of
    // (COutPoint prevout, unsigned int nStakeTime) records.
    if (version >= 3) {
        printf("UtxoSnapshot: writing %d setStakeSeen entries...\n", numStakeSeen);
        for (unsigned int i = 0; i < vStakeSeen.size(); i++) {
            CDataStream ssEntry(SER_DISK, CLIENT_VERSION);
            ssEntry << vStakeSeen[i].first;  // COutPoint (hash + index)
            ssEntry << vStakeSeen[i].second; // nStakeTime
            unsigned int entrySize = (unsigned int)ssEntry.size();
            std::string strEntry = ssEntry.str();
            fwrite(&entrySize, sizeof(entrySize), 1, file);
            fwrite(strEntry.data(), 1, entrySize, file);
            SHA256_Update(&sha256, &entrySize, sizeof(entrySize));
            SHA256_Update(&sha256, strEntry.data(), entrySize);
        }
    }

    // v2: After UTXOs, append raw blk0001.dat content. Streams in chunks;
    // SHA256 covers the bytes. A snapshot-loaded node has full block data
    // ready in datadir/blk0001.dat — no separate bootstrap needed.
    if (numBlocks > 0) {
        FILE* blkFile = fopen((GetDataDir() / "blk0001.dat").string().c_str(), "rb");
        if (!blkFile) {
            fclose(file);
            strError = "Cannot open blk0001.dat for snapshot embedding";
            return false;
        }
        printf("UtxoSnapshot: embedding blk0001.dat (%u bytes) into snapshot\n", numBlocks);
        unsigned char blkBuf[64 * 1024];
        size_t nLeft = numBlocks;
        while (nLeft > 0) {
            size_t nWant = nLeft > sizeof(blkBuf) ? sizeof(blkBuf) : nLeft;
            size_t nRead = fread(blkBuf, 1, nWant, blkFile);
            if (nRead != nWant) {
                fclose(blkFile);
                fclose(file);
                strError = "Short read on blk0001.dat during snapshot embed";
                return false;
            }
            fwrite(blkBuf, 1, nRead, file);
            SHA256_Update(&sha256, blkBuf, nRead);
            nLeft -= nRead;
        }
        fclose(blkFile);
    }

    // Finalize content hash and write it to the header
    SHA256_Final((unsigned char*)&contentHash, &sha256);
    fseek(file, contentHashPos, SEEK_SET);
    fwrite(&contentHash, sizeof(contentHash), 1, file);

    fclose(file);

    printf("UtxoSnapshot: wrote %s (%d headers, %d UTXOs, %u block bytes, hash=%s)\n",
           destPath.string().c_str(), numHeaders, numUtxos, numBlocks,
           contentHash.ToString().c_str());

    return true;
}

// Extract (type, hash160) from a scriptPubKey for the address index.
// Mirrors GetAddressFromScript() in main.cpp (which is file-static there).
static bool SnapAddressFromScript(const CScript& script, int& nType, uint160& hashBytes)
{
    CTxDestination dest;
    if (!ExtractDestination(script, dest))
        return false;
    if (const CKeyID* keyId = std::get_if<CKeyID>(&dest)) {
        nType = ADDR_TYPE_P2PKH; hashBytes = *keyId; return true;
    }
    if (const CScriptID* scriptId = std::get_if<CScriptID>(&dest)) {
        nType = ADDR_TYPE_P2SH; hashBytes = *scriptId; return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// LoadSnapshot - load a UTXO snapshot into a fresh LevelDB
//
// `requireCheckpoint` controls whether the snapshot's tip block must be a
// known checkpoint. This gate exists to prevent malicious P2P peers from
// tricking the daemon into accepting a fake UTXO set at an arbitrary
// height on an alternate chain. Local file loads (operator already has
// filesystem access, so the trust model is the same as editing the chain
// state directly) skip the gate via requireCheckpoint=false. P2P-delivered
// snapshots (SnapshotNet) keep the gate on.
// ---------------------------------------------------------------------------

bool LoadSnapshot(const fs::path& snapshotPath,
                  const fs::path& /*dataDir — unused; resolved per-backend via GetChainDataDir()*/,
                  std::string& strError,
                  bool requireCheckpoint)
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
    unsigned int numHeaders = 0, numUtxos = 0, numBlocks = 0, numStakeSeen = 0;
    uint256 expectedContentHash;

    if (fread(&magic, sizeof(magic), 1, file) != 1 ||
        fread(&version, sizeof(version), 1, file) != 1 ||
        fread(&network, sizeof(network), 1, file) != 1 ||
        fread(&height, sizeof(height), 1, file) != 1 ||
        fread(&blockHash, sizeof(blockHash), 1, file) != 1 ||
        fread(&moneySupply, sizeof(moneySupply), 1, file) != 1 ||
        fread(&numHeaders, sizeof(numHeaders), 1, file) != 1 ||
        fread(&numUtxos, sizeof(numUtxos), 1, file) != 1) {
        fclose(file);
        strError = "Truncated snapshot header (common fields)";
        return false;
    }
    // v2+ has numBlocks between numUtxos and (numStakeSeen|contentHash).
    if (version >= 2) {
        if (fread(&numBlocks, sizeof(numBlocks), 1, file) != 1) {
            fclose(file);
            strError = "Truncated snapshot header (numBlocks)";
            return false;
        }
    }
    // v3+ has numStakeSeen before contentHash.
    if (version >= 3) {
        if (fread(&numStakeSeen, sizeof(numStakeSeen), 1, file) != 1) {
            fclose(file);
            strError = "Truncated snapshot header (numStakeSeen)";
            return false;
        }
    }
    if (fread(&expectedContentHash, sizeof(expectedContentHash), 1, file) != 1) {
        fclose(file);
        strError = "Truncated snapshot header (contentHash)";
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

    if (height < 0 || static_cast<uint64_t>(numHeaders) >
            static_cast<uint64_t>(height) + 1) {
        fclose(file);
        strError = "Snapshot header count is inconsistent with its tip height";
        return false;
    }

    if (numStakeSeen > MAX_SNAPSHOT_STAKE_SEEN) {
        fclose(file);
        strError = "Snapshot contains too many setStakeSeen entries";
        return false;
    }

    std::error_code sizeError;
    const uint64_t snapshotSize = fs::file_size(snapshotPath, sizeError);
    const uint64_t minimumSize = 104ULL +
        static_cast<uint64_t>(numHeaders) * 5ULL +
        static_cast<uint64_t>(numUtxos) * 5ULL +
        static_cast<uint64_t>(numStakeSeen) * 5ULL +
        static_cast<uint64_t>(numBlocks);
    if (sizeError || minimumSize > snapshotSize) {
        fclose(file);
        strError = "Snapshot section counts exceed the file size";
        return false;
    }

    // Verify snapshot block is a known checkpoint (only for P2P-delivered
    // snapshots — local files are operator-trusted and can be at any height)
    if (requireCheckpoint && !Checkpoints::IsKnownCheckpoint(height, blockHash)) {
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

    // CRITICAL: Set fSerializeChainTrust=true before writing CDiskBlockIndex records.
    // LoadBlockIndex later reads with fSerializeChainTrust=true (derived from
    // dbformat >= 2), so writes must include nChainTrust to match. Without this,
    // every LoadSnapshot is followed by an "end of data: iostream error" in
    // LoadBlockIndex because the reader expects a field the writer omitted.
    //
    // The default value is false; nothing else in the daemon sets it to true
    // BEFORE LoadSnapshot runs (only the in-place upgrade path inside
    // LoadBlockIndex sets it true, which is too late). The snapshot writer
    // (an external daemon or our own DumpSnapshot) may have set it differently;
    // but for a fresh LevelDB created by LoadSnapshot, we want the resulting
    // DB to be self-consistent, so we always write with the field included.
    CDiskBlockIndex::fSerializeChainTrust = true;

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

    const int expectedFirstHeight = height - static_cast<int>(numHeaders) + 1;
    uint256 previousHeaderHash;
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

        CBlock blockHeader;
        blockHeader.nVersion = diskindex.nVersion;
        blockHeader.hashPrevBlock = diskindex.hashPrev;
        blockHeader.hashMerkleRoot = diskindex.hashMerkleRoot;
        blockHeader.nTime = diskindex.nTime;
        blockHeader.nBits = diskindex.nBits;
        blockHeader.nNonce = diskindex.nNonce;
        const uint256 calculatedHash = blockHeader.GetHash();

        const int expectedHeight = expectedFirstHeight + static_cast<int>(i);
        if (diskindex.nHeight != expectedHeight) {
            success = false;
            strError = "Non-contiguous snapshot height at header " +
                       std::to_string(i);
            break;
        }
        if (entryHash != calculatedHash) {
            success = false;
            strError = "Snapshot header hash mismatch at height " +
                       std::to_string(diskindex.nHeight);
            break;
        }
        if (i > 0 && diskindex.hashPrev != previousHeaderHash) {
            success = false;
            strError = "Broken snapshot header chain at height " +
                       std::to_string(diskindex.nHeight);
            break;
        }
        if (!Checkpoints::CheckHardened(diskindex.nHeight, entryHash)) {
            success = false;
            strError = "Snapshot conflicts with hardened checkpoint at height " +
                       std::to_string(diskindex.nHeight);
            break;
        }
        if (i + 1 == numHeaders && entryHash != blockHash) {
            success = false;
            strError = "Snapshot tip does not match its final block-index entry";
            break;
        }
        previousHeaderHash = entryHash;

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

            // Address index: snapshot UTXOs are all unspent -> credit balance + record UTXO.
            if (::fAddressIndex && !entry.scriptPubKey.empty() && entry.nValue != 0) {
                int nAType; uint160 aHash;
                if (SnapAddressFromScript(entry.scriptPubKey, nAType, aHash)) {
                    txdb.WriteAddressUtxo(nAType, aHash, txhash, nIndex,
                                          entry.nValue, entry.nHeight, entry.scriptPubKey);
                    int64_t nABal = 0;
                    txdb.ReadAddressBalance(nAType, aHash, nABal);
                    nABal += entry.nValue;
                    txdb.WriteAddressBalance(nAType, aHash, nABal);
                }
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

    // v3: After UTXOs (before the embedded blocks), read the setStakeSeen
    // entries collected from the last N PoS blocks of the source chain.
    // Required so a snapshot-loaded node has the recent stake-collision set
    // restored immediately, without having to walk blocks at startup. This
    // is what lets the anti-spam "too little proof-of-stake" check in
    // ProcessBlock function correctly right after a snapshot bootstrap.
    if (success && version >= 3 && numStakeSeen > 0) {
        printf("UtxoSnapshot: loading %d setStakeSeen entries...\n", numStakeSeen);
        // setStakeSeen is declared in main.cpp — we reference it via the
        // header declaration. Clear first so the snapshot's view is authoritative.
        setStakeSeen.clear();
        unsigned int nLoadedStakeSeen = 0;
        for (unsigned int i = 0; i < numStakeSeen; i++) {
            unsigned int entrySize;
            if (fread(&entrySize, sizeof(entrySize), 1, file) != 1 || entrySize > 1000) {
                success = false;
                strError = "Invalid setStakeSeen entry size at index " + std::to_string(i);
                break;
            }
            std::vector<char> buf(entrySize);
            if (fread(buf.data(), 1, entrySize, file) != entrySize) {
                success = false;
                strError = "Truncated setStakeSeen entry at index " + std::to_string(i);
                break;
            }
            SHA256_Update(&sha256, &entrySize, sizeof(entrySize));
            SHA256_Update(&sha256, buf.data(), entrySize);

            CDataStream ssEntry(buf.data(), buf.data() + buf.size(), SER_DISK, CLIENT_VERSION);
            COutPoint prevout;
            unsigned int nStakeTime;
            ssEntry >> prevout;
            ssEntry >> nStakeTime;
            setStakeSeen.insert(std::make_pair(prevout, nStakeTime));
            nLoadedStakeSeen++;
        }
        if (success)
            printf("UtxoSnapshot: loaded %d setStakeSeen entries\n", nLoadedStakeSeen);
    }

    // v2: After UTXOs, extract the raw blk0001.dat content. This makes the
    // loaded node fully self-contained — no separate bootstrap needed.
    if (success && version >= 2 && numBlocks > 0) {
        printf("UtxoSnapshot: extracting %u block bytes to blk0001.dat...\n", numBlocks);
        fs::path blkOut = GetDataDir() / "blk0001.dat";
        FILE* blkOutFile = fopen(blkOut.string().c_str(), "wb");
        if (!blkOutFile) {
            success = false;
            strError = "Cannot create blk0001.dat for snapshot extract: " + blkOut.string();
        } else {
            unsigned char blkBuf[64 * 1024];
            size_t nLeft = numBlocks;
            while (nLeft > 0 && success) {
                size_t nWant = nLeft > sizeof(blkBuf) ? sizeof(blkBuf) : nLeft;
                size_t nRead = fread(blkBuf, 1, nWant, file);
                if (nRead != nWant) {
                    success = false;
                    strError = "Short read on snapshot blocks section";
                    break;
                }
                fwrite(blkBuf, 1, nRead, blkOutFile);
                SHA256_Update(&sha256, blkBuf, nRead);
                nLeft -= nRead;
            }
            fclose(blkOutFile);
            if (success)
                printf("UtxoSnapshot: wrote blk0001.dat (%u bytes)\n", numBlocks);
        }
    }

    // Build the transaction index (txindex) from the freshly-extracted blk0001.dat.
    // The snapshot loads the UTXO set and blk0001.dat but does NOT rebuild the
    // per-tx index that CTransaction::ReadFromDisk requires for stake-input
    // signature verification. Without this, a new PoS block referencing any
    // pre-snapshot tx would fail CheckProofOfStake with "read txPrev failed"
    // and be rejected with DoS=100, stalling the node at the snapshot height.
    //
    // Walk every block in blk0001.dat and record CDiskTxPos for each tx, so
    // the loaded chain is fully self-contained. The walk is O(N) over the
    // historical block range but uses the already-cached blocks on disk and
    // batches the writes (every 5000 txs).
    if (success) {
        printf("UtxoSnapshot: building transaction index from blk0001.dat...\n");
        fs::path blkPath = GetDataDir() / "blk0001.dat";
        FILE* blkFile = fopen(blkPath.string().c_str(), "rb");
        if (!blkFile) {
            success = false;
            strError = "Cannot open blk0001.dat for txindex build: " + blkPath.string();
        } else {
            CAutoFile blkdat(blkFile, SER_DISK, CLIENT_VERSION);
            if (!txdb.TxnBegin()) {
                success = false;
                strError = "Failed to begin txindex build transaction";
            } else {
                unsigned int nPos = 0;
                unsigned int nBlocksIndexed = 0;
                unsigned int nTxsIndexed = 0;
                unsigned int nBatchTxs = 0;
                int64_t nLastReport = GetTimeMillis();
                while (success && blkdat.good()) {
                    fseek(blkdat, nPos, SEEK_SET);
                    // Locate block magic
                    unsigned char pchData[65536];
                    int nRead = fread(pchData, 1, sizeof(pchData), blkdat);
                    if (nRead <= 8) break;
                    void* nFind = memchr(pchData, pchMessageStart[0], nRead + 1 - sizeof(pchMessageStart));
                    if (!nFind) {
                        // Reached the tail of the file
                        break;
                    }
                    if (memcmp(nFind, pchMessageStart, sizeof(pchMessageStart)) != 0) {
                        nPos += ((unsigned char*)nFind - pchData) + 1;
                        continue;
                    }
                    unsigned int nBlockStart = nPos + ((unsigned char*)nFind - pchData);
                    fseek(blkdat, nBlockStart + sizeof(pchMessageStart), SEEK_SET);
                    unsigned int nSize;
                    blkdat >> nSize;
                    if (nSize == 0 || nSize > MAX_BLOCK_SIZE) {
                        nPos = nBlockStart + sizeof(pchMessageStart) + 4;
                        continue;
                    }
                    CBlock block;
                    blkdat >> block;
                    // CDiskTxPos::nTxPos is an absolute file offset. Match
                    // ConnectBlock's layout: block start, 80-byte header,
                    // CompactSize transaction count, then transaction bytes.
                    const unsigned int nBlockPos = nBlockStart +
                        sizeof(pchMessageStart) + sizeof(unsigned int);
                    unsigned int nTxPos = nBlockPos +
                        ::GetSerializeSize(CBlock(), SER_DISK, CLIENT_VERSION) -
                        (2 * GetSizeOfCompactSize(0)) +
                        GetSizeOfCompactSize(block.vtx.size());
                    for (const CTransaction& tx : block.vtx) {
                        CDiskTxPos posThisTx(1, nBlockPos, nTxPos);
                        txdb.UpdateTxIndex(tx.GetHash(), CTxIndex(posThisTx, tx.vout.size()));
                        nTxPos += ::GetSerializeSize(tx, SER_DISK, CLIENT_VERSION);
                        nTxsIndexed++;
                        nBatchTxs++;
                    }
                    nBlocksIndexed++;
                    // Advance past this block to scan the next one
                    nPos = nBlockStart + sizeof(pchMessageStart) + sizeof(unsigned int) + nSize;
                    // Commit batch periodically to avoid unbounded memory
                    if (nBatchTxs >= 5000) {
                        if (!txdb.TxnCommit()) {
                            success = false;
                            strError = "txindex batch commit failed";
                            break;
                        }
                        if (!txdb.TxnBegin()) {
                            success = false;
                            strError = "txindex batch restart failed";
                            break;
                        }
                        nBatchTxs = 0;
                        if (GetTimeMillis() - nLastReport > 5000) {
                            printf("UtxoSnapshot: indexed %u blocks / %u txs (pos=%u)\n",
                                   nBlocksIndexed, nTxsIndexed, nPos);
                            nLastReport = GetTimeMillis();
                        }
                    }
                }
                if (success && !txdb.TxnCommit()) {
                    success = false;
                    strError = "Final txindex commit failed";
                }
                if (success) {
                    printf("UtxoSnapshot: built txindex for %u blocks / %u transactions\n",
                           nBlocksIndexed, nTxsIndexed);
                }
            }
        }
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

    fLoadedFromSnapshot = true;

    return true;
}

} // namespace UtxoSnapshot
