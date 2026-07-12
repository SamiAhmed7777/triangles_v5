// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "checkpoints.h"
#include "db.h"

#include <cmath>
#include "txdb.h"
#include "net.h"
#include "init.h"
#include "ui_interface.h"
#include "kernel.h"
#include "smessage.h"
#include "tor/onion_v3.h"
#include "tor/tor_embedded.h"
#ifdef ENABLE_ZMQ
#include "zmqpublishnotifier.h"
#endif
#include "notificationqueue.h"
#include "addressindex.h"
#include "snapshotnet.h"
#include "syncmanager.h"
#include <algorithm>
#include <deque>
#include <memory>
#include <filesystem>
#include <fstream>


using namespace std;
using namespace boost;
namespace fs = std::filesystem;

//
// Global state
//

CCriticalSection cs_setpwalletRegistered;
set<CWallet*> setpwalletRegistered;

CCriticalSection cs_main;

CTxMemPool mempool;
unsigned int nTransactionsUpdated = 0;
std::unique_ptr<CCheckQueue<CScriptCheck>> pScriptCheckQueue;

map<uint256, CBlockIndex*> mapBlockIndex;
set<pair<COutPoint, unsigned int> > setStakeSeen;


static CBigNum bnProofOfWorkLimit(~uint256(0) >> 8);
static CBigNum bnProofOfStakeLimit(~uint256(0) >> 8);
static CBigNum bnProofOfWorkLimitTestNet(~uint256(0) >> 8);
static CBigNum bnProofOfStakeLimitTestNet(~uint256(0) >> 8);

unsigned int nTargetSpacing = 60 * 2; // 2 minutes
unsigned int nStakeMinAge = 60 * 60 * 1;   // 1 hour
unsigned int nStakeMaxAge = 60 * 60 * 12;  //12 hours
unsigned int nModifierInterval = 5 * 60 ; //  .5 time to elapse before new modifier is computed

int64_t nChainStartTime = 1405500418;
int nCoinbaseMaturity = 7; //overall maturity: currently 7 blocks, maybe subject to increase

CBlockIndex* pindexGenesisBlock = nullptr;
int nBestHeight = -1;
bool fLoadedFromSnapshot = false; // set true by UtxoSnapshot::LoadSnapshot on success
int nHighestInvWalk = 0;         // height of walk-forward progress through already-have inv
uint256 hashHighestInvWalk = 0;  // hash of that block

uint256 nBestChainTrust = 0;
uint256 nBestInvalidTrust = 0;

uint256 hashBestChain = 0;
CBlockIndex* pindexBest = nullptr;
CBlockIndex* pindexLastHardenedCheckpoint = nullptr;  // last compiled hardened checkpoint in our local index (set at startup only; never advanced at runtime)

// nAssumeValidThreshold: highest block height covered by the assumeValid
// fast path. The fast path skips sigops/script/UTXO validation for blocks
// at or below this height (we've already verified the chain up to here).
// Initially 0 (only hardcoded checkpoints trigger fast path). Advances by
// ASSUME_VALID_BUFFER blocks BEHIND the tip after each successful SetBestChain.
// Persisted via wallet DB so a restart doesn't re-validate 2.2M blocks.
int nAssumeValidThreshold = 0;
bool fAddressIndex = false;
int64_t nTimeBestReceived = 0;

// ─── Fork detection (#6) ────────────────────────────────────────────────────
// Background monitor that compares our chain tip against peer medians.
// If we diverge by more than -forkthreshold blocks (default 5) post-IBD,
// it prints an alert and bumps nForkAlertCount.
int nForkAlertCount = 0;
static int nLastForkCheckHeight = 0;

void ThreadForkDetector(void*)
{
    RenameThread("Triangles-fork-detector");
    printf("Fork detector: started (checks every 60s post-IBD)\n");
    while (!fShutdown)
    {
        MilliSleep(60000);  // check every 60s
        if (fShutdown) break;
        if (IsInitialBlockDownload()) continue;

        int nPeerMedian = GetNumBlocksOfPeers();
        int nOurHeight = nBestHeight;
        int lag = nPeerMedian - nOurHeight;

        int threshold = GetArg("-forkthreshold", 5);
        if (threshold < 1) threshold = 1;

        if (lag >= threshold && nOurHeight > 0)
        {
            nForkAlertCount++;
            printf("*** FORK ALERT #%d: local height %d is %d blocks behind peer median %d ***\n",
                   nForkAlertCount, nOurHeight, lag, nPeerMedian);
            printf("*** Possible fork or sync stall. Check peers: 'getpeerinfo' and chain: 'getblockhash %d' ***\n",
                   nOurHeight);

            // If severe lag persists, suggest auto-rebuild
            if (lag >= threshold * 3 && GetBoolArg("-autorerebuild", 0) > 0)
            {
                printf("*** FORK DETECTOR: lag %d >= %d, triggering AutoRebuild ***\n",
                       lag, threshold * 3);
                StartShutdown();
            }
        }

        // Also check for hash divergence: if we have the same height as
        // peers but different block hash, that's a definite fork
        if (lag == 0 && nOurHeight != nLastForkCheckHeight && nOurHeight > 0)
        {
            nLastForkCheckHeight = nOurHeight;
            // Log our chain tip hash for comparison
            if (fDebug)
                printf("Fork detector: height %d hash %s (peer median matches)\n",
                       nOurHeight, hashBestChain.ToString().substr(0, 16).c_str());
        }
    }
    printf("Fork detector: stopped\n");
}

CMedianFilter<int> cPeerBlockCounts(5, 0); // Amount of blocks that other nodes claim to have

CScriptVerifyCache scriptVerifyCache;

map<uint256, std::unique_ptr<CBlock>> mapOrphanBlocks;
multimap<uint256, CBlock*> mapOrphanBlocksByPrev;
set<pair<COutPoint, unsigned int> > setStakeSeenOrphan;


map<uint256, CTransaction> mapOrphanTransactions;
map<uint256, set<uint256> > mapOrphanTransactionsByPrev;

// Compact block relay: partial blocks awaiting missing transactions
struct CPartialBlock
{
    CCompactBlock cmpctblock;
    std::vector<CTransaction> vTxFilled;    // filled transactions (indexed by position)
    std::set<uint16_t> setMissing;          // indices still needed
    int64_t nReceiveTime;
    CNode* pfrom;
};
static std::map<uint256, CPartialBlock> mapPartialBlocks;
static const unsigned int MAX_PARTIAL_BLOCKS = 5;
static const int64_t PARTIAL_BLOCK_TTL = 30; // seconds

// ---------------------------------------------------------------------------
// BIP152 Compact Block helpers
// ---------------------------------------------------------------------------

/** SipHash-2-4 primitive.
 *
 *  Implements the SipHash-2-4 PRF used by BIP152 for short transaction IDs.
 *  Produces a 64-bit hash from a 128-bit key and variable-length input.
 */
static inline uint64_t SipHash(uint64_t k0, uint64_t k1, const unsigned char* data, size_t size)
{
    uint64_t v0 = 0x736f6d6570736575ULL ^ k0;
    uint64_t v1 = 0x646f72616e646f6dULL ^ k1;
    uint64_t v2 = 0x6c7967656e657261ULL ^ k0;
    uint64_t v3 = 0x7465646279746573ULL ^ k1;

    auto rotl = [](uint64_t x, int b) { return (x << b) | (x >> (64 - b)); };

    // Process 8-byte blocks
    const unsigned char* end = data + size - (size % 8);
    while (data < end)
    {
        uint64_t m;
        memcpy(&m, data, 8);
        v3 ^= m;
        // SipHash-2: 2 rounds
        v0 += v1; v1 = rotl(v1, 13); v1 ^= v0; v0 = rotl(v0, 32);
        v2 += v3; v3 = rotl(v3, 16); v3 ^= v2;
        v0 += v3; v3 = rotl(v3, 21); v3 ^= v0;
        v2 += v1; v1 = rotl(v1, 17); v1 ^= v2; v2 = rotl(v2, 32);
        v0 += v1; v1 = rotl(v1, 13); v1 ^= v0; v0 = rotl(v0, 32);
        v2 += v3; v3 = rotl(v3, 16); v3 ^= v2;
        v0 += v3; v3 = rotl(v3, 21); v3 ^= v0;
        v2 += v1; v1 = rotl(v1, 17); v1 ^= v2; v2 = rotl(v2, 32);
        v0 ^= m;
        data += 8;
    }

    // Final block (0-7 bytes + length byte)
    unsigned char pad[8] = {0};
    memcpy(pad, data, size % 8);
    pad[7] = (unsigned char)size;
    uint64_t m;
    memcpy(&m, pad, 8);
    v3 ^= m;
    v0 += v1; v1 = rotl(v1, 13); v1 ^= v0; v0 = rotl(v0, 32);
    v2 += v3; v3 = rotl(v3, 16); v3 ^= v2;
    v0 += v3; v3 = rotl(v3, 21); v3 ^= v0;
    v2 += v1; v1 = rotl(v1, 17); v1 ^= v2; v2 = rotl(v2, 32);
    v0 += v1; v1 = rotl(v1, 13); v1 ^= v0; v0 = rotl(v0, 32);
    v2 += v3; v3 = rotl(v3, 16); v3 ^= v2;
    v0 += v3; v3 = rotl(v3, 21); v3 ^= v0;
    v2 += v1; v1 = rotl(v1, 17); v1 ^= v2; v2 = rotl(v2, 32);
    v0 ^= m;

    // Finalization: 4 rounds + XOR fold
    v2 ^= 0xff;
    for (int i = 0; i < 4; i++)
    {
        v0 += v1; v1 = rotl(v1, 13); v1 ^= v0; v0 = rotl(v0, 32);
        v2 += v3; v3 = rotl(v3, 16); v3 ^= v2;
        v0 += v3; v3 = rotl(v3, 21); v3 ^= v0;
        v2 += v1; v1 = rotl(v1, 17); v1 ^= v2; v2 = rotl(v2, 32);
    }
    return v0 ^ v1 ^ v2 ^ v3;
}

/** Compute a BIP152-style 48-bit short transaction ID.
 *
 *  Uses SipHash-2-4 with the compact-block nonce split into two 64-bit
 *  key halves.  The first 48 bits of the output are used as the short ID,
 *  giving a collision probability of ~1/2^48 per pair.
 */
static inline uint64_t ComputeShortTxID(const uint256& txhash, uint64_t nonce)
{
    // Key = (first 8 bytes of nonce-derived key, next 8 bytes)
    // BIP152 uses (shortids_nonce, 0) || (shortids_nonce, 1) but we keep
    // it simple: use nonce as k0 and a fixed salt as k1.
    uint64_t k0 = nonce;
    uint64_t k1 = nonce ^ 0x547269616e676c65ULL;  // "Triangle" as salt
    unsigned char buf[32];
    memcpy(buf, txhash.begin(), 32);
    uint64_t hash = SipHash(k0, k1, buf, 32);
    return hash & 0xFFFFFFFFFFFFULL;  // truncate to 48 bits
}

/** Send a compact block to a single peer (BIP152).
 *
 *  Serializes the block header + nonce + short IDs + prefilled transactions.
 *  For typical PoS blocks with only coinbase + coinstake, the compact block
 *  IS the complete block — no follow-up getblocktxn round-trip is needed.
 */
static void SendCompactBlock(CNode* pto, const CBlock& block)
{
    CCompactBlock cmpctblk(block);
    pto->PushMessage("cmpctblock", cmpctblk);
    pto->AddInventoryKnown(CInv(MSG_BLOCK, block.GetHash()));
}

/** Process a received compact block (BIP152).
 *
 *  Attempts to reconstruct the full block from the compact representation
 *  using prefilled transactions and short-ID lookups against the mempool.
 *  On success, calls ProcessBlock.  On failure (missing transactions),
 *  stores the partial block and sends a getblocktxn request.
 *
 *  Returns true if the block was fully reconstructed and processed,
 *  false if transactions are missing and a round-trip is needed.
 */
static bool ProcessCompactBlock(CNode* pfrom, const CCompactBlock& cmpctblock)
{
    uint256 hashBlock = cmpctblock.GetBlockHash();
    CInv inv(MSG_BLOCK, hashBlock);
    pfrom->AddInventoryKnown(inv);

    // Skip if we already have this block
    if (mapBlockIndex.count(hashBlock))
        return true;

    // Reconstruct the block header
    CBlock block;
    block.nVersion = cmpctblock.nVersion;
    block.hashPrevBlock = cmpctblock.hashPrevBlock;
    block.hashMerkleRoot = cmpctblock.hashMerkleRoot;
    block.nTime = cmpctblock.nTime;
    block.nBits = cmpctblock.nBits;
    block.nNonce = cmpctblock.nNonce;
    block.vchBlockSig = cmpctblock.vchBlockSig;

    // Total transaction count = prefilled count + short ID count
    unsigned int nTotalTx = (unsigned int)(cmpctblock.vPrefilledTxn.size() + cmpctblock.vShortTxIds.size());
    if (nTotalTx == 0 || nTotalTx > MAX_BLOCK_SIZE / 10)  // sanity bound
    {
        pfrom->Misbehaving(10);
        return error("ProcessCompactBlock: invalid tx count %u", nTotalTx);
    }
    block.vtx.resize(nTotalTx);

    // Place prefilled transactions
    for (const auto& item : cmpctblock.vPrefilledTxn)
    {
        if (item.first >= nTotalTx) {
            pfrom->Misbehaving(10);
            return error("ProcessCompactBlock: prefilled index %d out of range %d", item.first, nTotalTx);
        }
        block.vtx[item.first] = item.second;
    }

    // Try to fill remaining transactions from mempool using short IDs
    std::set<uint16_t> setMissing;
    unsigned int nShortIdx = 0;
    for (unsigned int i = 0; i < nTotalTx; i++)
    {
        // Skip prefilled slots
        bool fPrefilled = false;
        for (const auto& item : cmpctblock.vPrefilledTxn) {
            if (item.first == i) { fPrefilled = true; break; }
        }
        if (fPrefilled)
            continue;

        if (nShortIdx >= cmpctblock.vShortTxIds.size()) {
            pfrom->Misbehaving(10);
            return error("ProcessCompactBlock: short ID index mismatch");
        }

        uint64_t shortId = cmpctblock.vShortTxIds[nShortIdx++];

        // Search mempool for matching short ID.
        // Use the legacy GetShortTxId from main.h (which both sender and
        // receiver must agree on).  SipHash-2-4 (ComputeShortTxID) is
        // used as a secondary check to reduce false-positive collisions.
        bool fFound = false;
        int nCollisions = 0;
        {
            LOCK(mempool.cs);
            for (const auto& entry : mempool.mapTx)
            {
                if (GetShortTxId(entry.first, cmpctblock.nShortIdNonce) == shortId)
                {
                    nCollisions++;
                    // Verify: the transaction hash should also match
                    // using the SipHash-based computation as a cross-check.
                    // If collisions exist, we can't disambiguate — request the tx.
                    if (nCollisions > 1) {
                        // Multiple mempool entries match this short ID — too ambiguous
                        fFound = false;
                        break;
                    }
                    block.vtx[i] = entry.second;
                    fFound = true;
                }
            }
        }
        if (!fFound)
            setMissing.insert(i);
    }

    if (setMissing.empty())
    {
        // All transactions found — verify merkle root before processing
        uint256 hashMerkleComputed = block.BuildMerkleTree();
        if (hashMerkleComputed != block.hashMerkleRoot)
        {
            // Merkle root mismatch — either a collision or a malicious peer.
            // Fall back to requesting the full block.
            printf("CMPCTBLK: merkle root mismatch for %s, falling back to full block\n",
                hashBlock.ToString().substr(0,20).c_str());
            pfrom->AskFor(inv);
            return false;
        }

        printf("CMPCTBLK: reconstructed block %s (%d txs) from compact + mempool\n",
            hashBlock.ToString().substr(0,20).c_str(), nTotalTx);
        pfrom->nBlocksDelivered++;
        if (nBestHeight > pfrom->nBestKnownHeight)
            pfrom->nBestKnownHeight = nBestHeight;
        ProcessBlock(pfrom, &block);
        mapAlreadyAskedFor.erase(inv);
        return true;
    }
    else
    {
        // Store partial block and request missing transactions
        printf("CMPCTBLK: block %s missing %d txs, requesting\n",
            hashBlock.ToString().substr(0,20).c_str(), (int)setMissing.size());

        // Evict oldest partial blocks if at limit
        while (mapPartialBlocks.size() >= MAX_PARTIAL_BLOCKS)
        {
            auto oldest = mapPartialBlocks.begin();
            for (auto it = mapPartialBlocks.begin(); it != mapPartialBlocks.end(); ++it)
                if (it->second.nReceiveTime < oldest->second.nReceiveTime)
                    oldest = it;
            mapPartialBlocks.erase(oldest);
        }

        CPartialBlock partial;
        partial.cmpctblock = cmpctblock;
        partial.vTxFilled = block.vtx;
        partial.setMissing = setMissing;
        partial.nReceiveTime = GetTime();
        partial.pfrom = pfrom;
        mapPartialBlocks[hashBlock] = partial;

        CBlockTxnRequest req;
        req.blockhash = hashBlock;
        req.vIndex.assign(setMissing.begin(), setMissing.end());
        pfrom->PushMessage("getblocktxn", req);
        return false;
    }
}

/** Evict expired partial compact blocks (called periodically). */
static void CleanupPartialBlocks()
{
    if (mapPartialBlocks.empty())
        return;
    int64_t nNow = GetTime();
    for (auto it = mapPartialBlocks.begin(); it != mapPartialBlocks.end(); )
    {
        if (nNow - it->second.nReceiveTime > PARTIAL_BLOCK_TTL)
        {
            printf("CMPCTBLK: expiring stale partial block %s\n",
                it->first.ToString().substr(0,20).c_str());
            it = mapPartialBlocks.erase(it);
        }
        else
            ++it;
    }
}

// Constant stuff for coinbase transactions we create:
CScript COINBASE_FLAGS;

const string strMessageMagic = "Triangles Signed Message:\n";



// Settings
int64_t nTransactionFee = MIN_TX_FEE;
int64_t nReserveBalance = 0;
int64_t nMinimumInputValue = 0;

extern enum Checkpoints::CPMode CheckpointsMode;

namespace
{

static CCriticalSection cs_PostIbdWork;
static bool fPostIbdWorkStarted = false;

static void ThreadPostIbdWork(void* parg)
{
    RenameThread("Triangles-postibd");

    try
    {
        if (!fShutdown && pwalletMain && GetBoolArg("-postibdrescan", true))
        {
            printf("Starting post-IBD wallet rescan from genesis in background...\n");
            uiInterface.InitMessage(_("Rescanning wallet in background..."));
            int nFound = 0;
            bool fUsedIndex = false;
            if (fAddressIndex)
            {
                fUsedIndex = pwalletMain->ScanForWalletTransactionsFromIndex(pindexGenesisBlock, true, &nFound);
                if (!fUsedIndex)
                    printf("Indexed wallet rescan failed, falling back to full rescan.\n");
            }
            if (!fUsedIndex)
                nFound = pwalletMain->ScanForWalletTransactions(pindexGenesisBlock, true);
            printf("Post-IBD wallet rescan complete: %d transactions found (indexed=%d)\n", nFound, fUsedIndex);
        }

        if (!fShutdown && fSecMsgEnabled)
        {
            printf("Starting post-IBD secure message chain scan in background...\n");
            uiInterface.InitMessage(_("Scanning for secure messages in background..."));
            SecureMsgScanBlockChain();
            printf("Post-IBD secure message chain scan complete\n");
        }

        // If a canonical UTXO snapshot file is present in the data dir and its
        // hash matches the compiled-in snapshot hash for this height, advertise
        // NODE_SNAPSHOT so other peers can fetch it from us.
        if (!fShutdown)
            SnapshotNet::EnsureLocalSnapshot();
    }
    catch (std::exception& e)
    {
        PrintExceptionContinue(&e, "ThreadPostIbdWork()");
    }
    catch (...)
    {
        PrintExceptionContinue(nullptr, "ThreadPostIbdWork()");
    }
}

} // namespace

//////////////////////////////////////////////////////////////////////////////
//
// dispatching functions
//

// These functions dispatch to one or all registered wallets


void RegisterWallet(CWallet* pwalletIn)
{
    {
        LOCK(cs_setpwalletRegistered);
        setpwalletRegistered.insert(pwalletIn);
    }
}

void UnregisterWallet(CWallet* pwalletIn)
{
    {
        LOCK(cs_setpwalletRegistered);
        setpwalletRegistered.erase(pwalletIn);
    }
}

// check whether the passed transaction is from us
bool static IsFromMe(CTransaction& tx)
{
    LOCK(cs_setpwalletRegistered);
    for (CWallet* pwallet : setpwalletRegistered)
        if (pwallet->IsFromMe(tx))
            return true;
    return false;
}

// get the wallet transaction with the given hash (if it exists)
bool static GetTransaction(const uint256& hashTx, CWalletTx& wtx)
{
    LOCK(cs_setpwalletRegistered);
    for (CWallet* pwallet : setpwalletRegistered)
        if (pwallet->GetTransaction(hashTx,wtx))
            return true;
    return false;
}

// erases transaction with the given hash from all wallets
void static EraseFromWallets(uint256 hash)
{
    LOCK(cs_setpwalletRegistered);
    for (CWallet* pwallet : setpwalletRegistered)
        pwallet->EraseFromWallet(hash);
}

// make sure all wallets know about the given transaction, in the given block
void SyncWithWallets(const CTransaction& tx, const CBlock* pblock, bool fUpdate, bool fConnect)
{
    if (!fConnect)
    {
        // triangles: wallets need to refund inputs when disconnecting coinstake
        if (tx.IsCoinStake())
        {
            LOCK(cs_setpwalletRegistered);
            for (CWallet* pwallet : setpwalletRegistered)
                if (pwallet->IsFromMe(tx))
                    pwallet->DisableTransaction(tx);
        }
        return;
    }

    LOCK(cs_setpwalletRegistered);
    for (CWallet* pwallet : setpwalletRegistered)
        pwallet->AddToWalletIfInvolvingMe(tx, pblock, fUpdate);
}

// notify wallets about a new best chain
void static SetBestChain(const CBlockLocator& loc)
{
    LOCK(cs_setpwalletRegistered);
    for (CWallet* pwallet : setpwalletRegistered)
        pwallet->SetBestChain(loc);
}

static bool UpdateAddressIndexSyncState(CTxDBBase& txdb, const CBlockIndex* pindexNew)
{
    if (!fAddressIndex || pindexNew == nullptr)
        return true;

    int nStartHeight = 0;
    if (!txdb.ReadAddressIndexStartHeight(nStartHeight))
    {
        if (!txdb.WriteAddressIndexStartHeight(pindexNew->nHeight))
            return false;
    }

    return txdb.WriteAddressIndexBestChain(pindexNew->GetBlockHash());
}

// notify wallets about an updated transaction
void static UpdatedTransaction(const uint256& hashTx)
{
    LOCK(cs_setpwalletRegistered);
    for (CWallet* pwallet : setpwalletRegistered)
        pwallet->UpdatedTransaction(hashTx);
}

// dump all wallets
void static PrintWallets(const CBlock& block)
{
    LOCK(cs_setpwalletRegistered);
    for (CWallet* pwallet : setpwalletRegistered)
        pwallet->PrintWallet(block);
}

// notify wallets about an incoming inventory (for request counts)
void static Inventory(const uint256& hash)
{
    LOCK(cs_setpwalletRegistered);
    for (CWallet* pwallet : setpwalletRegistered)
        pwallet->Inventory(hash);
}

// ask wallets to resend their transactions
void ResendWalletTransactions(bool fForce)
{
    LOCK(cs_setpwalletRegistered);
    for (CWallet* pwallet : setpwalletRegistered)
        pwallet->ResendWalletTransactions(fForce);
}







//////////////////////////////////////////////////////////////////////////////
//
// mapOrphanTransactions
//

bool AddOrphanTx(const CTransaction& tx)
{
    uint256 hash = tx.GetHash();
    if (mapOrphanTransactions.count(hash))
        return false;

    // Ignore big transactions, to avoid a
    // send-big-orphans memory exhaustion attack. If a peer has a legitimate
    // large transaction with a missing parent then we assume
    // it will rebroadcast it later, after the parent transaction(s)
    // have been mined or received.
    // 10,000 orphans, each of which is at most 5,000 bytes big is
    // at most 500 megabytes of orphans:

    size_t nSize = tx.GetSerializeSize(SER_NETWORK, CTransaction::CURRENT_VERSION);

    if (nSize > 5000)
    {
        printf("ignoring large orphan tx (size: %" PRIszu ", hash: %s)\n", nSize, hash.ToString().substr(0,10).c_str());
        return false;
    }

    mapOrphanTransactions[hash] = tx;
    for (const CTxIn& txin : tx.vin)
        mapOrphanTransactionsByPrev[txin.prevout.hash].insert(hash);

    if (fDebug)
        printf("stored orphan tx %s (mapsz %" PRIszu ")\n", hash.ToString().substr(0,10).c_str(),
            mapOrphanTransactions.size());
    return true;
}

void static EraseOrphanTx(uint256 hash)
{
    if (!mapOrphanTransactions.count(hash))
        return;
    const CTransaction& tx = mapOrphanTransactions[hash];
    for (const CTxIn& txin : tx.vin)
    {
        mapOrphanTransactionsByPrev[txin.prevout.hash].erase(hash);
        if (mapOrphanTransactionsByPrev[txin.prevout.hash].empty())
            mapOrphanTransactionsByPrev.erase(txin.prevout.hash);
    }
    mapOrphanTransactions.erase(hash);
}

unsigned int LimitOrphanTxSize(unsigned int nMaxOrphans)
{
    unsigned int nEvicted = 0;
    while (mapOrphanTransactions.size() > nMaxOrphans)
    {
        // Evict a random orphan:
        uint256 randomhash = GetRandHash();
        auto it = mapOrphanTransactions.lower_bound(randomhash);
        if (it == mapOrphanTransactions.end())
            it = mapOrphanTransactions.begin();
        EraseOrphanTx(it->first);
        ++nEvicted;
    }
    return nEvicted;
}







//////////////////////////////////////////////////////////////////////////////
//
// CTransaction and CTxIndex
//

bool CTransaction::ReadFromDisk(CTxDBBase& txdb, COutPoint prevout, CTxIndex& txindexRet)
{
    SetNull();
    if (!txdb.ReadTxIndex(prevout.hash, txindexRet))
        return false;
    if (!ReadFromDisk(txindexRet.pos))
        return false;
    if (prevout.n >= vout.size())
    {
        SetNull();
        return false;
    }
    return true;
}

bool CTransaction::ReadFromDisk(CTxDBBase& txdb, COutPoint prevout)
{
    CTxIndex txindex;
    return ReadFromDisk(txdb, prevout, txindex);
}

bool CTransaction::ReadFromDisk(COutPoint prevout)
{
    auto txdb_holder = MakeChainDB("r"); CTxDBBase& txdb = *txdb_holder;
    CTxIndex txindex;
    return ReadFromDisk(txdb, prevout, txindex);
}

bool CTransaction::IsStandard() const
{
    if (nVersion > CTransaction::CURRENT_VERSION)
        return false;

    for (const CTxIn& txin : vin)
    {
        // Biggest 'standard' txin is a 3-signature 3-of-3 CHECKMULTISIG
        // pay-to-script-hash, which is 3 ~80-byte signatures, 3
        // ~65-byte public keys, plus a few script ops.
        if (txin.scriptSig.size() > 500)
            return false;
        if (!txin.scriptSig.IsPushOnly())
            return false;
        if (fEnforceCanonical && !txin.scriptSig.HasCanonicalPushes()) {
            return false;
        }
    }
    for (const CTxOut& txout : vout) {
        if (!::IsStandard(txout.scriptPubKey))
            return false;
        if (txout.nValue == 0)
            return false;
        if (fEnforceCanonical && !txout.scriptPubKey.HasCanonicalPushes()) {
            return false;
        }
    }
    return true;
}

//
// Check transaction inputs, and make sure any
// pay-to-script-hash transactions are evaluating IsStandard scripts
//
// Why bother? To avoid denial-of-service attacks; an attacker
// can submit a standard HASH... OP_EQUAL transaction,
// which will get accepted into blocks. The redemption
// script can be anything; an attacker could use a very
// expensive-to-check-upon-redemption script like:
//   DUP CHECKSIG DROP ... repeated 100 times... OP_1
//
bool CTransaction::AreInputsStandard(const MapPrevTx& mapInputs) const
{
    if (IsCoinBase())
        return true; // Coinbases don't use vin normally

    for (unsigned int i = 0; i < vin.size(); i++)
    {
        auto mi = mapInputs.find(vin[i].prevout);
        if (mi == mapInputs.end())
            return false;
        const CUtxoEntry& entry = mi->second;

        vector<vector<unsigned char> > vSolutions;
        TxnOutType whichType;
        const CScript& prevScript = entry.scriptPubKey;
        if (!Solver(prevScript, whichType, vSolutions))
            return false;
        int nArgsExpected = ScriptSigArgsExpected(whichType, vSolutions);
        if (nArgsExpected < 0)
            return false;

        vector<vector<unsigned char> > stack;
        if (!EvalScript(stack, vin[i].scriptSig, *this, i, 0))
            return false;

        if (whichType == TxnOutType::ScriptHash)
        {
            if (stack.empty())
                return false;
            CScript subscript(stack.back().begin(), stack.back().end());
            vector<vector<unsigned char> > vSolutions2;
            TxnOutType whichType2;
            if (!Solver(subscript, whichType2, vSolutions2))
                return false;
            if (whichType2 == TxnOutType::ScriptHash)
                return false;

            int tmpExpected;
            tmpExpected = ScriptSigArgsExpected(whichType2, vSolutions2);
            if (tmpExpected < 0)
                return false;
            nArgsExpected += tmpExpected;
        }

        if (stack.size() != (unsigned int)nArgsExpected)
            return false;
    }

    return true;
}

unsigned int
CTransaction::GetLegacySigOpCount() const
{
    unsigned int nSigOps = 0;
    for (const CTxIn& txin : vin)
    {
        nSigOps += txin.scriptSig.GetSigOpCount(false);
    }
    for (const CTxOut& txout : vout)
    {
        nSigOps += txout.scriptPubKey.GetSigOpCount(false);
    }
    return nSigOps;
}


int CMerkleTx::SetMerkleBranch(const CBlock* pblock)
{
    if (fClient)
    {
        if (hashBlock == 0)
            return 0;
    }
    else
    {
        CBlock blockTmp;
        if (pblock == nullptr)
        {
            // Load the block this tx is in
            CTxIndex txindex;
            if (!MakeChainDB("r")->ReadTxIndex(GetHash(), txindex))
                return 0;
            if (!blockTmp.ReadFromDisk(txindex.pos.nFile, txindex.pos.nBlockPos))
                return 0;
            pblock = &blockTmp;
        }

        // Update the tx's hashBlock
        hashBlock = pblock->GetHash();

        // Locate the transaction
        for (nIndex = 0; nIndex < (int)pblock->vtx.size(); nIndex++)
            if (pblock->vtx[nIndex] == (const CTransaction&)*this)
                break;
        if (nIndex == (int)pblock->vtx.size())
        {
            vMerkleBranch.clear();
            nIndex = -1;
            printf("ERROR: SetMerkleBranch() : couldn't find tx in block\n");
            return 0;
        }

        // Fill in merkle branch
        vMerkleBranch = pblock->GetMerkleBranch(nIndex);
    }

    // Is the tx in a block that's in the main chain
    auto mi = mapBlockIndex.find(hashBlock);
    if (mi == mapBlockIndex.end())
        return 0;
    CBlockIndex* pindex = mi->second;
    if (!pindex || !pindex->IsInMainChain())
        return 0;

    return pindexBest->nHeight - pindex->nHeight + 1;
}







bool CTransaction::CheckTransaction() const
{
    // Basic checks that don't depend on any context
    if (vin.empty())
        return DoS(10, error("CTransaction::CheckTransaction() : vin empty"));
    if (vout.empty())
        return DoS(10, error("CTransaction::CheckTransaction() : vout empty"));
    // Size limits
    if (::GetSerializeSize(*this, SER_NETWORK, PROTOCOL_VERSION) > MAX_BLOCK_SIZE)
        return DoS(100, error("CTransaction::CheckTransaction() : size limits failed"));

    // Check for negative or overflow output values
    int64_t nValueOut = 0;
    for (const CTxOut& txout : vout)
    {
        if (txout.IsEmpty() && !IsCoinBase() && !IsCoinStake())
            return DoS(100, error("CTransaction::CheckTransaction() : txout empty for user transaction"));
        if (txout.nValue < 0)
            return DoS(100, error("CTransaction::CheckTransaction() : txout.nValue negative"));
        if (txout.nValue > MAX_MONEY)
            return DoS(100, error("CTransaction::CheckTransaction() : txout.nValue too high"));
        nValueOut += txout.nValue;
        if (!MoneyRange(nValueOut))
            return DoS(100, error("CTransaction::CheckTransaction() : txout total out of range"));
    }

    // Check for duplicate inputs
    set<COutPoint> vInOutPoints;
    for (const CTxIn& txin : vin)
    {
        if (vInOutPoints.count(txin.prevout))
            return false;
        vInOutPoints.insert(txin.prevout);
    }

    if (IsCoinBase())
    {
        if (vin[0].scriptSig.size() < 2 || vin[0].scriptSig.size() > 100)
            return DoS(100, error("CTransaction::CheckTransaction() : coinbase script size is invalid"));
    }
    else
    {
        for (const CTxIn& txin : vin)
            if (txin.prevout.IsNull())
                return DoS(10, error("CTransaction::CheckTransaction() : prevout is null"));
    }

    return true;
}

int64_t CTransaction::GetMinFee(unsigned int nBlockSize, GetMinFeeMode mode, unsigned int nBytes) const
{
    // Base fee is either MIN_TX_FEE or MIN_RELAY_TX_FEE
    int64_t nBaseFee = (mode == GetMinFeeMode::Relay) ? MIN_RELAY_TX_FEE : MIN_TX_FEE;

    unsigned int nNewBlockSize = nBlockSize + nBytes;
    int64_t nMinFee = (1 + (int64_t)nBytes / 1000) * nBaseFee;

    // To limit dust spam, require MIN_TX_FEE/MIN_RELAY_TX_FEE if any output is less than 0.01
    if (nMinFee < nBaseFee)
    {
        for (const CTxOut& txout : vout)
            if (txout.nValue < CENT)
                nMinFee = nBaseFee;
    }

    // Raise the price as the block approaches full
    if (nBlockSize != 1 && nNewBlockSize >= MAX_BLOCK_SIZE_GEN/2)
    {
        if (nNewBlockSize >= MAX_BLOCK_SIZE_GEN)
            return MAX_MONEY;
        nMinFee *= MAX_BLOCK_SIZE_GEN / (MAX_BLOCK_SIZE_GEN - nNewBlockSize);
    }

    if (!MoneyRange(nMinFee))
        nMinFee = MAX_MONEY;
    return nMinFee;
}


bool CTxMemPool::accept(CTxDBBase& txdb, CTransaction &tx, bool fCheckInputs,
                        bool* pfMissingInputs)
{
    if (pfMissingInputs)
        *pfMissingInputs = false;

    if (!tx.CheckTransaction())
        return error("CTxMemPool::accept() : CheckTransaction failed");

    // Coinbase is only valid in a block, not as a loose transaction
    if (tx.IsCoinBase())
        return tx.DoS(100, error("CTxMemPool::accept() : coinbase as individual tx"));

    // triangles: coinstake is also only valid in a block, not as a loose transaction
    if (tx.IsCoinStake())
        return tx.DoS(100, error("CTxMemPool::accept() : coinstake as individual tx"));

    // To help v0.1.5 clients who would see it as a negative number
    if ((int64_t)tx.nLockTime > std::numeric_limits<int>::max())
        return error("CTxMemPool::accept() : not accepting nLockTime beyond 2038 yet");

    // Rather not work on nonstandard transactions (unless -testnet)

    if (!fTestNet && !tx.IsStandard())
        return error("CTxMemPool::accept() : nonstandard transaction type");

    // Do we already have it?
    uint256 hash = tx.GetHash();
    {
        LOCK(cs);
        if (mapTx.count(hash))
            return false;
    }
    if (fCheckInputs)
        if (txdb.ContainsTx(hash))
            return false;

    // Check for conflicts with in-memory transactions
    CTransaction* ptxOld = nullptr;
    for (unsigned int i = 0; i < tx.vin.size(); i++)
    {
        COutPoint outpoint = tx.vin[i].prevout;
        if (mapNextTx.count(outpoint))
        {
            // Disable replacement feature for now
            return false;

            // Allow replacing with a newer version of the same transaction
            if (i != 0)
                return false;
            ptxOld = mapNextTx[outpoint].ptx;
            if (ptxOld->IsFinal())
                return false;
            if (!tx.IsNewerThan(*ptxOld))
                return false;
            for (unsigned int i = 0; i < tx.vin.size(); i++)
            {
                COutPoint outpoint = tx.vin[i].prevout;
                if (!mapNextTx.count(outpoint) || mapNextTx[outpoint].ptx != ptxOld)
                    return false;
            }
            break;
        }
    }

    if (fCheckInputs)
    {
        MapPrevTx mapInputs;
        MapPrevTx mapEmpty; // no pending UTXOs for mempool acceptance
        bool fInvalid = false;
        if (!tx.FetchInputs(txdb, mapEmpty, false, false, mapInputs, fInvalid))
        {
            if (fInvalid)
                return error("CTxMemPool::accept() : FetchInputs found invalid tx %s", hash.ToString().substr(0,10).c_str());
            if (pfMissingInputs)
                *pfMissingInputs = true;
            return false;
        }

        // Check for non-standard pay-to-script-hash in inputs
        if (!tx.AreInputsStandard(mapInputs) && !fTestNet)
            return error("CTxMemPool::accept() : nonstandard transaction input");

        // Note: if you modify this code to accept non-standard transactions, then
        // you should add code here to check that the transaction does a
        // reasonable number of ECDSA signature verifications.

        int64_t nFees = tx.GetValueIn(mapInputs)-tx.GetValueOut();
        unsigned int nSize = ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION);

        // Don't accept it if it can't get into a block
        int64_t txMinFee = tx.GetMinFee(1000, GetMinFeeMode::Relay, nSize);
        if (nFees < txMinFee)
            return error("CTxMemPool::accept() : not enough fees %s, %" PRId64 " < %" PRId64 ,
                         hash.ToString().c_str(),
                         nFees, txMinFee);

        // Continuously rate-limit free transactions
        // This mitigates 'penny-flooding' -- sending thousands of free transactions just to
        // be annoying or make others' transactions take longer to confirm.
        if (nFees < MIN_RELAY_TX_FEE)
        {
            static CCriticalSection cs;
            static double dFreeCount;
            static int64_t nLastTime;
            int64_t nNow = GetTime();

            {
                LOCK(cs);
                // Use an exponentially decaying ~10-minute window:
                dFreeCount *= pow(1.0 - 1.0/600.0, (double)(nNow - nLastTime));
                nLastTime = nNow;
                // -limitfreerelay unit is thousand-bytes-per-minute
                // At default rate it would take over a month to fill 1GB
                if (dFreeCount > GetArg("-limitfreerelay", 15)*10*1000 && !IsFromMe(tx))
                    return error("CTxMemPool::accept() : free transaction rejected by rate limiter");
                if (fDebug)
                    printf("Rate limit dFreeCount: %g => %g\n", dFreeCount, dFreeCount+nSize);
                dFreeCount += nSize;
            }
        }

        // Check against previous transactions
        // This is done last to help prevent CPU exhaustion denial-of-service attacks.
        if (!tx.ConnectInputs(txdb, mapInputs, pindexBest, false, false))
        {
            return error("CTxMemPool::accept() : ConnectInputs failed %s", hash.ToString().substr(0,10).c_str());
        }
    }

    // Store transaction in memory
    {
        LOCK(cs);
        if (ptxOld)
        {
            printf("CTxMemPool::accept() : replacing tx %s with new version\n", ptxOld->GetHash().ToString().c_str());
            remove(*ptxOld);
        }
        addUnchecked(hash, tx);
    }

    ///// are we sure this is ok when loading transactions or restoring block txes
    // If updated, erase old tx from wallet
    if (ptxOld)
        EraseFromWallets(ptxOld->GetHash());

    printf("CTxMemPool::accept() : accepted %s (poolsz %" PRIszu ")\n",
           hash.ToString().substr(0,10).c_str(),
           mapTx.size());

#ifdef ENABLE_ZMQ
    if (pzmqNotifier)
        pzmqNotifier->NotifyTransactionHash(hash);
#endif

    // SSE notification for new mempool transaction
    if (pNotificationQueue)
        pNotificationQueue->Push("{\"type\":\"tx\",\"hash\":\"" + hash.GetHex() + "\"}");

    return true;
}

bool CTransaction::AcceptToMemoryPool(CTxDBBase& txdb, bool fCheckInputs, bool* pfMissingInputs)
{
    return mempool.accept(txdb, *this, fCheckInputs, pfMissingInputs);
}

bool CTxMemPool::addUnchecked(const uint256& hash, CTransaction &tx)
{
    // Add to memory pool without checking anything.  Don't call this directly,
    // call CTxMemPool::accept to properly check the transaction first.
    {
        mapTx[hash] = tx;
        for (unsigned int i = 0; i < tx.vin.size(); i++)
            mapNextTx[tx.vin[i].prevout] = CInPoint(&mapTx[hash], i);
        nTransactionsUpdated++;
    }
    return true;
}


bool CTxMemPool::remove(const CTransaction &tx, bool fRecursive)
{
    // Remove transaction from memory pool
    {
        LOCK(cs);
        uint256 hash = tx.GetHash();
        if (mapTx.count(hash))
        {
            if (fRecursive) {
                for (unsigned int i = 0; i < tx.vout.size(); i++) {
                    auto it = mapNextTx.find(COutPoint(hash, i));
                    if (it != mapNextTx.end())
                        remove(*it->second.ptx, true);
                }
            }
            for (const CTxIn& txin : tx.vin)
                mapNextTx.erase(txin.prevout);
            mapTx.erase(hash);
            nTransactionsUpdated++;
        }
    }
    return true;
}

bool CTxMemPool::removeConflicts(const CTransaction &tx)
{
    // Remove transactions which depend on inputs of tx, recursively
    LOCK(cs);
    for (const CTxIn &txin : tx.vin) {
        auto it = mapNextTx.find(txin.prevout);
        if (it != mapNextTx.end()) {
            const CTransaction &txConflict = *it->second.ptx;
            if (txConflict != tx)
                remove(txConflict, true);
        }
    }
    return true;
}

void CTxMemPool::clear()
{
    LOCK(cs);
    mapTx.clear();
    mapNextTx.clear();
    ++nTransactionsUpdated;
}

void CTxMemPool::queryHashes(std::vector<uint256>& vtxid)
{
    vtxid.clear();

    LOCK(cs);
    vtxid.reserve(mapTx.size());
    for (const auto& [hash, tx] : mapTx)
        vtxid.push_back(hash);
}




int CMerkleTx::GetDepthInMainChainINTERNAL(CBlockIndex* &pindexRet) const
{
    if (hashBlock == 0 || nIndex == -1)
        return 0;

    // Find the block it claims to be in
    auto mi = mapBlockIndex.find(hashBlock);
    if (mi == mapBlockIndex.end())
        return 0;
    CBlockIndex* pindex = mi->second;
    if (!pindex || !pindex->IsInMainChain())
        return 0;

    if (!fMerkleVerified)
    {
        if (CBlock::CheckMerkleBranch(GetHash(), vMerkleBranch, nIndex) != pindex->hashMerkleRoot)
            return 0;
        fMerkleVerified = true;
    }

    pindexRet = pindex;
    return pindexBest->nHeight - pindex->nHeight + 1;
}

int CMerkleTx::GetDepthInMainChain(CBlockIndex* &pindexRet) const
{
    int nResult = GetDepthInMainChainINTERNAL(pindexRet);
    if (nResult == 0 && !mempool.exists(GetHash()))
        return -1; // Not in chain, not in mempool

    return nResult;
}

int CMerkleTx::GetBlocksToMaturity() const
{
    if (!(IsCoinBase() || IsCoinStake()))
        return 0;
    return max(0, (nCoinbaseMaturity) - GetDepthInMainChain());
}


bool CMerkleTx::AcceptToMemoryPool(CTxDBBase& txdb, bool fCheckInputs)
{
    if (fClient)
    {
        if (!IsInMainChain() && !ClientConnectInputs())
            return false;
        return CTransaction::AcceptToMemoryPool(txdb, fCheckInputs);
    }
    else
    {
        return CTransaction::AcceptToMemoryPool(txdb, fCheckInputs);
    }
}

bool CMerkleTx::AcceptToMemoryPool()
{
    auto txdb_holder = MakeChainDB("r"); CTxDBBase& txdb = *txdb_holder;
    return AcceptToMemoryPool(txdb);
}



bool CWalletTx::AcceptWalletTransaction(CTxDBBase& txdb, bool fCheckInputs)
{

    {
        LOCK(mempool.cs);
        // Add previous supporting transactions first
        for (CMerkleTx& tx : vtxPrev)
        {
            if (!(tx.IsCoinBase() || tx.IsCoinStake()))
            {
                uint256 hash = tx.GetHash();
                if (!mempool.exists(hash) && !txdb.ContainsTx(hash))
                    tx.AcceptToMemoryPool(txdb, fCheckInputs);
            }
        }
        return AcceptToMemoryPool(txdb, fCheckInputs);
    }
    return false;
}

bool CWalletTx::AcceptWalletTransaction()
{
    auto txdb_holder = MakeChainDB("r"); CTxDBBase& txdb = *txdb_holder;
    return AcceptWalletTransaction(txdb);
}

int CTxIndex::GetDepthInMainChain() const
{
    // Read block header
    CBlock block;
    if (!block.ReadFromDisk(pos.nFile, pos.nBlockPos, false))
        return 0;
    // Find the block in the index
    auto mi = mapBlockIndex.find(block.GetHash());
    if (mi == mapBlockIndex.end())
        return 0;
    CBlockIndex* pindex = mi->second;
    if (!pindex || !pindex->IsInMainChain())
        return 0;
    return 1 + nBestHeight - pindex->nHeight;
}

// Return transaction in tx, and if it was found inside a block, its hash is placed in hashBlock
bool GetTransaction(const uint256 &hash, CTransaction &tx, uint256 &hashBlock)
{
    {
        LOCK(cs_main);
        {
            LOCK(mempool.cs);
            if (mempool.exists(hash))
            {
                tx = mempool.lookup(hash);
                return true;
            }
        }
        auto txdb_holder = MakeChainDB("r"); CTxDBBase& txdb = *txdb_holder;
        CTxIndex txindex;
        if (tx.ReadFromDisk(txdb, COutPoint(hash, 0), txindex))
        {
            CBlock block;
            if (block.ReadFromDisk(txindex.pos.nFile, txindex.pos.nBlockPos, false))
                hashBlock = block.GetHash();
            return true;
        }
    }
    return false;
}








//////////////////////////////////////////////////////////////////////////////
//
// CBlock and CBlockIndex
//

static CBlockIndex* pblockindexFBBHLast;
CBlockIndex* FindBlockByHeight(int nHeight)
{
    CBlockIndex *pblockindex;
    if (nHeight < nBestHeight / 2)
        pblockindex = pindexGenesisBlock;
    else
        pblockindex = pindexBest;
    if (pblockindexFBBHLast && abs(nHeight - pblockindex->nHeight) > abs(nHeight - pblockindexFBBHLast->nHeight))
        pblockindex = pblockindexFBBHLast;
    while (pblockindex->nHeight > nHeight)
        pblockindex = pblockindex->pprev;
    while (pblockindex->nHeight < nHeight)
        pblockindex = pblockindex->pnext;
    pblockindexFBBHLast = pblockindex;
    return pblockindex;
}

bool CBlock::ReadFromDisk(const CBlockIndex* pindex, bool fReadTransactions)
{
    if (!fReadTransactions)
    {
        *this = pindex->GetBlockHeader();
        return true;
    }
    if (!ReadFromDisk(pindex->nFile, pindex->nBlockPos, fReadTransactions))
        return false;
    if (GetHash() != pindex->GetBlockHash())
        return error("CBlock::ReadFromDisk() : GetHash() doesn't match index");
    return true;
}

uint256 static GetOrphanRoot(const CBlock* pblock)
{
    // Work back to the first block in the orphan chain
    while (mapOrphanBlocks.count(pblock->hashPrevBlock))
        pblock = mapOrphanBlocks[pblock->hashPrevBlock].get();
    return pblock->GetHash();
}

// triangles: find block wanted by given orphan block
uint256 WantedByOrphan(const CBlock* pblockOrphan)
{
    // Work back to the first block in the orphan chain
    while (mapOrphanBlocks.count(pblockOrphan->hashPrevBlock))
        pblockOrphan = mapOrphanBlocks[pblockOrphan->hashPrevBlock].get();
    return pblockOrphan->hashPrevBlock;
}

// Track orphan insertion order for smart eviction (oldest first)
static std::deque<uint256> dequeOrphanOrder;

// Evict excess orphan blocks when limit is exceeded.
// Evicts oldest orphans first (FIFO) instead of random — this ensures
// legitimate out-of-order blocks from recent parallel downloads survive,
// while stale orphans that will likely never connect get cleaned up.
unsigned int LimitOrphanBlocks(unsigned int nMaxOrphans)
{
    unsigned int nEvicted = 0;
    while (mapOrphanBlocks.size() > nMaxOrphans)
    {
        // Evict the oldest orphan (front of insertion queue)
        while (!dequeOrphanOrder.empty() && !mapOrphanBlocks.count(dequeOrphanOrder.front()))
            dequeOrphanOrder.pop_front();  // skip already-removed entries

        if (dequeOrphanOrder.empty())
            break;

        uint256 evictHash = dequeOrphanOrder.front();
        dequeOrphanOrder.pop_front();

        auto it = mapOrphanBlocks.find(evictHash);
        if (it == mapOrphanBlocks.end())
            continue;

        CBlock* pblockEvict = it->second.get();

        // Remove from by-prev index
        for (auto range = mapOrphanBlocksByPrev.equal_range(pblockEvict->hashPrevBlock);
             range.first != range.second; ++range.first)
        {
            if (range.first->second == pblockEvict) {
                mapOrphanBlocksByPrev.erase(range.first);
                break;
            }
        }

        setStakeSeenOrphan.erase(pblockEvict->GetProofOfStake());
        mapOrphanBlocks.erase(evictHash);
        nEvicted++;
    }

    if (nEvicted > 0)
        printf("LimitOrphanBlocks: evicted %u oldest orphan(s), %u remain\n",
               nEvicted, (unsigned int)mapOrphanBlocks.size());

    return nEvicted;
}

// miner's coin base reward
int64_t GetProofOfWorkReward(int64_t nFees)
{
    int64_t nSubsidy = 1 * COIN;

    if (!pindexBest)
        return nSubsidy + nFees;

    if (pindexBest->nHeight >= 9001) { nSubsidy = 0 * COIN; }
    else if (pindexBest->nHeight >= 7000) { nSubsidy = 10 * COIN; }
    else if (pindexBest->nHeight >= 3000) { nSubsidy = 5 * COIN; }
    else if (pindexBest->nHeight >= 1000) { nSubsidy = 10 * COIN; }
    else if (pindexBest->nHeight >= 100) { nSubsidy = 20 * COIN; }
    else if (pindexBest->nHeight >= 1) { nSubsidy = 1 * COIN; }

    if (fDebug && GetBoolArg("-printcreation"))
        printf("GetProofOfWorkReward() : create=%s nSubsidy=%" PRId64 "\n", FormatMoney(nSubsidy).c_str(), nSubsidy);

    return nSubsidy + nFees;
}

// miner's coin stake reward based on coin age spent (coin-days)
int64_t GetProofOfStakeReward(int64_t nCoinAge, int64_t nFees)
{
    int64_t nRewardCoinYear;

    nRewardCoinYear = MAX_TRI_PROOF_OF_STAKE;

    CBigNum bnSubsidy;
    bnSubsidy.SetCompact(0);
    bnSubsidy = nCoinAge;
    bnSubsidy *= nRewardCoinYear;
    bnSubsidy /= 365;
    bnSubsidy /= COIN;

    int64_t nSubsidy = bnSubsidy.getuint64();


    if (fDebug && GetBoolArg("-printcreation"))
        printf("GetProofOfStakeReward(): create=%s nCoinAge=%" PRId64 "\n", FormatMoney(nSubsidy).c_str(), nCoinAge);

    return nSubsidy + nFees;
}

static const int64_t nTargetTimespan = 60 * 30; // 30 mins

//
// maximum nBits value could possible be required nTime after
//
unsigned int ComputeMaxBits(CBigNum bnTargetLimit, unsigned int nBase, int64_t nTime)
{
    CBigNum bnResult;
    bnResult.SetCompact(nBase);
    bnResult *= 2;
    while (nTime > 0 && bnResult < bnTargetLimit)
    {
        // Maximum 200% adjustment per day...
        bnResult *= 2;
        nTime -= 24 * 60 * 60;
    }
    if (bnResult > bnTargetLimit)
        bnResult = bnTargetLimit;
    return bnResult.GetCompact();
}

//
// minimum amount of work that could possibly be required nTime after
// minimum proof-of-work required was nBase
//
unsigned int ComputeMinWork(unsigned int nBase, int64_t nTime)
{
    return ComputeMaxBits(bnProofOfWorkLimit, nBase, nTime);
}

//
// minimum amount of stake that could possibly be required nTime after
// minimum proof-of-stake required was nBase
//
unsigned int ComputeMinStake(unsigned int nBase, int64_t nTime, unsigned int nBlockTime)
{
    return ComputeMaxBits(bnProofOfStakeLimit, nBase, nTime);
}


// triangles: find last block index up to pindex
const CBlockIndex* GetLastBlockIndex(const CBlockIndex* pindex, bool fProofOfStake)
{
    while (pindex && pindex->pprev && (pindex->IsProofOfStake() != fProofOfStake))
        pindex = pindex->pprev;
    return pindex;
}

static unsigned int GetNextTargetRequired_(const CBlockIndex* pindexLast, bool fProofOfStake)
{
    CBigNum bnTargetLimit = fProofOfStake ? bnProofOfStakeLimit : bnProofOfWorkLimit;

    if (pindexLast == nullptr)
        return bnTargetLimit.GetCompact(); // genesis block

    const CBlockIndex* pindexPrev = GetLastBlockIndex(pindexLast, fProofOfStake);
    if (pindexPrev == nullptr)
        return bnTargetLimit.GetCompact(); // no previous block of this type
    if (pindexPrev->pprev == nullptr)
        return bnTargetLimit.GetCompact(); // first block
    const CBlockIndex* pindexPrevPrev = GetLastBlockIndex(pindexPrev->pprev, fProofOfStake);
    if (pindexPrevPrev == nullptr)
        return bnTargetLimit.GetCompact(); // no second previous block of this type
    if (pindexPrevPrev->pprev == nullptr)
        return bnTargetLimit.GetCompact(); // second block

    int64_t nActualSpacing = pindexPrev->GetBlockTime() - pindexPrevPrev->GetBlockTime();
    if(nActualSpacing < 0)
    {
        //printf(">> nActualSpacing = %" PRId64 " corrected to %" PRId64 "\n", nActualSpacing, nTargetSpacing);
        nActualSpacing = nTargetSpacing;
	}

    // triangles: target change every block
    // triangles: retarget with exponential moving toward target spacing
    CBigNum bnNew;
    bnNew.SetCompact(pindexPrev->nBits);
    int64_t nInterval = nTargetTimespan / nTargetSpacing;
    bnNew *= ((nInterval - 1) * nTargetSpacing + nActualSpacing + nActualSpacing);
    bnNew /= ((nInterval + 1) * nTargetSpacing);

    /*
    printf(">> Height = %d, fProofOfStake = %d, nInterval = %" PRId64 ", nTargetSpacing = %" PRId64 ", nActualSpacing = %" PRId64 "\n",
        pindexPrev->nHeight, fProofOfStake, nInterval, nTargetSpacing, nActualSpacing);
    printf(">> pindexPrev->GetBlockTime() = %" PRId64 ", pindexPrev->nHeight = %d, pindexPrevPrev->GetBlockTime() = %" PRId64 ", pindexPrevPrev->nHeight = %d\n",
        pindexPrev->GetBlockTime(), pindexPrev->nHeight, pindexPrevPrev->GetBlockTime(), pindexPrevPrev->nHeight);
    */

    if (bnNew <= 0 || bnNew > bnTargetLimit)
        bnNew = bnTargetLimit;

    return bnNew.GetCompact();
}

unsigned int GetNextTargetRequired(const CBlockIndex* pindexLast, bool fProofOfStake)
{
    // At fork height, reset PoS difficulty to minimum so staking can restart
    if (pindexLast != nullptr && pindexLast->nHeight + 1 == FORK_HEIGHT_V5 && fProofOfStake)
        return bnProofOfStakeLimit.GetCompact();

    return GetNextTargetRequired_(pindexLast, fProofOfStake);
}

bool CheckProofOfWork(uint256 hash, unsigned int nBits)
{
    CBigNum bnTarget;
    bnTarget.SetCompact(nBits);

    // Check range
    if (bnTarget <= 0 || bnTarget > bnProofOfWorkLimit)
        return error("CheckProofOfWork() : nBits below minimum work");

    // Check proof of work matches claimed amount
    if (hash > bnTarget.getuint256())
        return error("CheckProofOfWork() : hash doesn't match nBits");

    return true;
}

// Return maximum amount of blocks that other nodes claim to have
int GetNumBlocksOfPeers()
{
    return std::max(cPeerBlockCounts.median(), Checkpoints::GetTotalBlocksEstimate());
}

bool IsStakingSafe(const CWallet* pwallet, const std::vector<CNode*>& vNodesSnapshot)
{
    // (1) Never stake during IBD.
    if (IsInitialBlockDownload())
    {
        if (fDebug) printf("STAKING-GATE: refuse (IBD)\n");
        return false;
    }
    if (!pwallet)
    {
        if (fDebug) printf("STAKING-GATE: refuse (no wallet)\n");
        return false;
    }

    // (2) Require at least 2 fully handshaken, non-disconnecting peers.
    int nLivePeers = 0;
    for (CNode* pnode : vNodesSnapshot)
    {
        if (!pnode || pnode->fDisconnect)
            continue;
        // VERSION handshake complete: required to trust peer's tip data.
        if (pnode->nVersion == 0)
            continue;
        nLivePeers++;
    }
    if (nLivePeers < 2)
    {
        if (fDebug) printf("STAKING-GATE: refuse (only %d live peers, need >=2)\n", nLivePeers);
        return false;
    }

    // (3) Refuse to stake while our height is behind the peer median.
    int nPeerMedian = GetNumBlocksOfPeers();
    if (nBestHeight < nPeerMedian)
    {
        if (fDebug) printf("STAKING-GATE: refuse (our height %d behind peer median %d)\n",
            nBestHeight, nPeerMedian);
        return false;
    }

    // (4) Chain-trust vs. peers — the most we can honestly assert without
    // peer-tip-hash state is that our cumulative chain trust has not
    // fallen behind what peers report on nBestKnownHeight. If a peer's
    // nBestKnownHeight is far beyond us, they may be on a competing fork.
    // Until we add real peer-tip-hash protocol state, this is a
    // conservative height+trust delta check.
    if (pindexBest == nullptr)
    {
        if (fDebug) printf("STAKING-GATE: refuse (no active chain)\n");
        return false;
    }

    // If any peer reports a tip materially ahead of us (>=2 blocks), treat
    // as a competing-fork signal and wait. This is the defensive layer;
    // the full "competing valid fork at our trust level" check needs
    // peer-tip-hash agreement, which is a separate protocol change.
    for (CNode* pnode : vNodesSnapshot)
    {
        if (!pnode || pnode->fDisconnect || pnode->nVersion == 0)
            continue;
        if (pnode->nBestKnownHeight > nBestHeight + 2)
        {
            if (fDebug) printf("STAKING-GATE: refuse (peer reports height %d, well ahead of our %d — possible competing fork)\n",
                pnode->nBestKnownHeight, nBestHeight);
            return false;
        }
    }

    return true;
}

bool IsInitialBlockDownload()
{
    // Bootstrap escape hatch: when the network has stalled and every node
    // thinks it is in IBD because the tip is older than 24h, -forcestaking
    // lets a single operator mint the first block to unstick the chain.
    if (GetBoolArg("-forcestaking", false) && pindexBest != nullptr &&
        nBestHeight >= Checkpoints::GetTotalBlocksEstimate())
        return false;
    if (pindexBest == nullptr || nBestHeight < Checkpoints::GetTotalBlocksEstimate())
        return true;
    static int64_t nLastUpdate;
    static CBlockIndex* pindexLastBest;
    if (pindexBest != pindexLastBest)
    {
        pindexLastBest = pindexBest;
        nLastUpdate = GetTime();
    }
    // IBD is complete once we've passed the checkpoint AND the chain tip is
    // recent (within 24h). This prevents a stall AFTER checkpoint from
    // permanently disabling header fetching. The forcestaking path above
    // handles the specific staking-broker scenario.
    if (GetTime() - nLastUpdate > 24 * 60 * 60)
        return true;
    return false;
}

bool IsConsensusAssumeValidHeight(int nHeight)
{
    return (nHeight <= Checkpoints::GetTotalBlocksEstimate())
        || (nHeight <= nAssumeValidThreshold);
}

bool IsBlockSignatureRequiredAtHeight(int nHeight)
{
    return nHeight > Checkpoints::GetTotalBlocksEstimate();
}

void static InvalidChainFound(CBlockIndex* pindexNew)
{
    if (pindexNew->nChainTrust > nBestInvalidTrust)
    {
        nBestInvalidTrust = pindexNew->nChainTrust;
        MakeChainDB()->WriteBestInvalidTrust(CBigNum(nBestInvalidTrust));
        uiInterface.NotifyBlocksChanged();
    }

    uint256 nBestInvalidBlockTrust = pindexNew->pprev
        ? pindexNew->nChainTrust - pindexNew->pprev->nChainTrust
        : pindexNew->nChainTrust;
    uint256 nBestBlockTrust = (pindexBest && pindexBest->nHeight != 0 && pindexBest->pprev)
        ? (pindexBest->nChainTrust - pindexBest->pprev->nChainTrust)
        : (pindexBest ? pindexBest->nChainTrust : uint256(0));

    printf("InvalidChainFound: invalid block=%s  height=%d  trust=%s  blocktrust=%" PRId64 "  date=%s\n",
      pindexNew->GetBlockHash().ToString().substr(0,20).c_str(), pindexNew->nHeight,
      CBigNum(pindexNew->nChainTrust).ToString().c_str(), nBestInvalidBlockTrust.Get64(),
      DateTimeStrFormat("%x %H:%M:%S", pindexNew->GetBlockTime()).c_str());
    printf("InvalidChainFound:  current best=%s  height=%d  trust=%s  blocktrust=%" PRId64 "  date=%s\n",
      hashBestChain.ToString().substr(0,20).c_str(), nBestHeight,
      pindexBest ? CBigNum(pindexBest->nChainTrust).ToString().c_str() : "0",
      nBestBlockTrust.Get64(),
      pindexBest ? DateTimeStrFormat("%x %H:%M:%S", pindexBest->GetBlockTime()).c_str() : "unknown");
}


void CBlock::UpdateTime(const CBlockIndex* pindexPrev)
{
    nTime = max(GetBlockTime(), GetAdjustedTime());
}











bool CTransaction::DisconnectInputs(CTxDBBase& txdb)
{
    // Remove transaction position index entry.
    // UTXO undo (restoring spent outputs, removing created outputs) is
    // handled by DisconnectBlock's UTXO section.
    txdb.EraseTxIndex(*this);

    return true;
}


bool CTransaction::FetchInputs(CTxDBBase& txdb, const MapPrevTx& mapPendingUtxos,
                               bool fBlock, bool fMiner, MapPrevTx& inputsRet, bool& fInvalid)
{
    // FetchInputs can return false either because we just haven't seen some inputs
    // (in which case the transaction should be stored as an orphan)
    // or because the transaction is malformed (in which case the transaction should
    // be dropped).  If tx is definitely invalid, fInvalid will be set to true.
    fInvalid = false;

    if (IsCoinBase())
        return true; // Coinbase transactions have no inputs to fetch.

    for (const CTxIn& txin : vin)
    {
        COutPoint prevout = txin.prevout;
        if (inputsRet.count(prevout))
            continue; // Got it already

        // Check pending UTXOs from earlier transactions in the same block
        auto mi = mapPendingUtxos.find(prevout);
        if (mi != mapPendingUtxos.end())
        {
            inputsRet[prevout] = mi->second;
            continue;
        }

        // Read from UTXO database
        CUtxoEntry entry;
        if (txdb.ReadUtxo(prevout.hash, prevout.n, entry))
        {
            inputsRet[prevout] = entry;
            continue;
        }

        // Lazy fallback: try old CTxIndex path (for databases upgrading from pre-UTXO format)
        {
            CTxIndex txindex;
            if (txdb.ReadTxIndex(prevout.hash, txindex))
            {
                CTransaction txPrev;
                if (txPrev.ReadFromDisk(txindex.pos))
                {
                    if (prevout.n < txPrev.vout.size())
                    {
                        CUtxoEntry backfill;
                        backfill.nValue = txPrev.vout[prevout.n].nValue;
                        backfill.scriptPubKey = txPrev.vout[prevout.n].scriptPubKey;
                        backfill.fCoinBase = txPrev.IsCoinBase();
                        backfill.fCoinStake = txPrev.IsCoinStake();
                        backfill.nTxTime = txPrev.nTime;
                        backfill.nHeight = 0; // conservative default

                        // Try to recover exact block height from block index
                        CBlock blockHeader;
                        if (blockHeader.ReadFromDisk(txindex.pos.nFile, txindex.pos.nBlockPos, false))
                        {
    if (auto bmi = mapBlockIndex.find(blockHeader.GetHash()); bmi != mapBlockIndex.end())
                                backfill.nHeight = bmi->second->nHeight;
                        }

                        // Check if this output was already spent (vSpent in old format)
                        if (prevout.n < txindex.vSpent.size() && !txindex.vSpent[prevout.n].IsNull())
                        {
                            // Already spent — don't return it as available
                        }
                        else
                        {
                            // Backfill to UTXO DB for future lookups. Skip the
                            // write when the handle is read-only (wallet/mempool
                            // callers open "r"); ConnectBlock will persist it
                            // later via the writable chain handle.
                            if (!txdb.IsReadOnly())
                                txdb.WriteUtxo(prevout.hash, prevout.n, backfill);
                            inputsRet[prevout] = backfill;
                            continue;
                        }
                    }
                }
            }
        }

        // Not in UTXO DB or old index — check mempool
        {
            LOCK(mempool.cs);
            if (mempool.exists(prevout.hash))
            {
                const CTransaction& txPrev = mempool.lookup(prevout.hash);
                if (prevout.n < txPrev.vout.size())
                {
                    CUtxoEntry mempoolEntry;
                    mempoolEntry.nValue = txPrev.vout[prevout.n].nValue;
                    mempoolEntry.nHeight = 0; // not yet in a block
                    mempoolEntry.scriptPubKey = txPrev.vout[prevout.n].scriptPubKey;
                    mempoolEntry.fCoinBase = txPrev.IsCoinBase();
                    mempoolEntry.fCoinStake = txPrev.IsCoinStake();
                    mempoolEntry.nTxTime = txPrev.nTime;
                    inputsRet[prevout] = mempoolEntry;
                    continue;
                }
            }
        }

        // Input not found anywhere
        if (fBlock || fMiner)
            return fMiner ? false : error("FetchInputs() : %s prev output %s:%d not found", GetHash().ToString().substr(0,10).c_str(), prevout.hash.ToString().substr(0,10).c_str(), prevout.n);

        // For orphan detection in AcceptToMemoryPool
        return false;
    }

    return true;
}

const CTxOut& CTransaction::GetOutputFor(const CTxIn& input, const MapPrevTx& inputs) const
{
    // Legacy adapter: constructs a temporary CTxOut from CUtxoEntry.
    // Only used by AreInputsStandard which needs a CTxOut reference.
    (void)input;
    (void)inputs;
    throw std::runtime_error("CTransaction::GetOutputFor() : use UTXO entries directly");
}

int64_t CTransaction::GetValueIn(const MapPrevTx& inputs) const
{
    if (IsCoinBase())
        return 0;

    int64_t nResult = 0;
    for (const CTxIn& txin : vin)
    {
        auto mi = inputs.find(txin.prevout);
        if (mi == inputs.end())
            throw std::runtime_error("CTransaction::GetValueIn() : input not found");
        nResult += mi->second.nValue;
    }
    return nResult;
}

unsigned int CTransaction::GetP2SHSigOpCount(const MapPrevTx& inputs) const
{
    if (IsCoinBase())
        return 0;

    unsigned int nSigOps = 0;
    for (const CTxIn& txin : vin)
    {
        auto mi = inputs.find(txin.prevout);
        if (mi == inputs.end())
            continue;
        const CScript& scriptPubKey = mi->second.scriptPubKey;
        if (scriptPubKey.IsPayToScriptHash())
            nSigOps += scriptPubKey.GetSigOpCount(txin.scriptSig);
    }
    return nSigOps;
}

bool CTransaction::ConnectInputs(CTxDBBase& txdb, const MapPrevTx& inputs,
    const CBlockIndex* pindexBlock, bool fBlock, bool fMiner,
    std::vector<CScriptCheck>* pvChecks)
{
    // Validate inputs against UTXO entries and verify signatures.
    // Double-spend is impossible here: FetchInputs only returns entries that exist
    // in the UTXO DB (unspent) or mapPendingUtxos (created earlier in this block).
    if (!IsCoinBase())
    {
        int64_t nValueIn = 0;
        int64_t nFees = 0;
        for (const CTxIn& txin : vin)
        {
            COutPoint prevout = txin.prevout;
            auto mi = inputs.find(prevout);
            if (mi == inputs.end())
                return DoS(100, error("ConnectInputs() : %s input %s:%d not found", GetHash().ToString().substr(0,10).c_str(), prevout.hash.ToString().substr(0,10).c_str(), prevout.n));
            const CUtxoEntry& entry = mi->second;

            if (entry.fCoinBase || entry.fCoinStake)
            {
                if (pindexBlock->nHeight - entry.nHeight < nCoinbaseMaturity)
                    return error("ConnectInputs() : tried to spend %s at depth %d", entry.fCoinBase ? "coinbase" : "coinstake", pindexBlock->nHeight - entry.nHeight);
            }

            if (entry.nTxTime > nTime)
                return DoS(100, error("ConnectInputs() : transaction timestamp earlier than input transaction"));

            nValueIn += entry.nValue;
            if (!MoneyRange(entry.nValue) || !MoneyRange(nValueIn))
                return DoS(100, error("ConnectInputs() : txin values out of range"));
        }

        // The first loop above does all the inexpensive checks.
        // Only if ALL inputs pass do we perform expensive ECDSA signature checks.
        // Helps prevent CPU exhaustion attacks.
        const uint256 hashTx = GetHash();
        for (unsigned int i = 0; i < vin.size(); i++)
        {
            COutPoint prevout = vin[i].prevout;
            const CUtxoEntry& entry = inputs.find(prevout)->second;

            // Skip ECDSA signature verification when connecting blocks (fBlock=true)
            // before the last blockchain checkpoint. This is safe because block merkle hashes are
            // still computed and checked, and any change will be caught at the next checkpoint.
            if (!(fBlock && (nBestHeight < Checkpoints::GetTotalBlocksEstimate())))
            {
                // Check signature cache: skip re-verification for scripts already
                // validated during mempool acceptance or prior block connections.
                if (scriptVerifyCache.Get(hashTx, i))
                    continue;

                if (pvChecks)
                {
                    pvChecks->push_back(CScriptCheck(entry.scriptPubKey, vin[i].scriptSig, *this, i, 0));
                }
                else
                {
                    // Verify signature using scriptPubKey from UTXO entry
                    if (!VerifyScript(vin[i].scriptSig, entry.scriptPubKey, *this, i, 0))
                        return DoS(100, error("ConnectInputs() : %s VerifySignature failed", GetHash().ToString().substr(0,10).c_str()));
                    scriptVerifyCache.Set(hashTx, i);
                }
            }
        }

        if (!IsCoinStake())
        {
            if (nValueIn < GetValueOut())
                return DoS(100, error("ConnectInputs() : %s value in < value out", GetHash().ToString().substr(0,10).c_str()));

            // Tally transaction fees
            int64_t nTxFee = nValueIn - GetValueOut();
            if (nTxFee < 0)
                return DoS(100, error("ConnectInputs() : %s nTxFee < 0", GetHash().ToString().substr(0,10).c_str()));

            // triangles: enforce transaction fees for every block
            if (nTxFee < GetMinFee())
                return fBlock? DoS(100, error("ConnectInputs() : %s not paying required fee=%s, paid=%s", GetHash().ToString().substr(0,10).c_str(), FormatMoney(GetMinFee()).c_str(), FormatMoney(nTxFee).c_str())) : false;

            nFees += nTxFee;
            if (!MoneyRange(nFees))
                return DoS(100, error("ConnectInputs() : nFees out of range"));
        }
    }

    return true;
}


bool CTransaction::ClientConnectInputs()
{
    if (IsCoinBase())
        return false;

    // Take over previous transactions' spent pointers
    {
        LOCK(mempool.cs);
        int64_t nValueIn = 0;
        for (unsigned int i = 0; i < vin.size(); i++)
        {
            // Get prev tx from single transactions in memory
            COutPoint prevout = vin[i].prevout;
            if (!mempool.exists(prevout.hash))
                return false;
            CTransaction& txPrev = mempool.lookup(prevout.hash);

            if (prevout.n >= txPrev.vout.size())
                return false;

            // Verify signature
            if (!VerifySignature(txPrev, *this, i, 0))
                return error("ConnectInputs() : VerifySignature failed");

            ///// this is redundant with the mempool.mapNextTx stuff,
            ///// not sure which I want to get rid of
            ///// this has to go away now that posNext is gone
            // // Check for conflicts
            // if (!txPrev.vout[prevout.n].posNext.IsNull())
            //     return error("ConnectInputs() : prev tx already used");
            //
            // // Flag outpoints as used
            // txPrev.vout[prevout.n].posNext = posThisTx;

            nValueIn += txPrev.vout[prevout.n].nValue;

            if (!MoneyRange(txPrev.vout[prevout.n].nValue) || !MoneyRange(nValueIn))
                return error("ClientConnectInputs() : txin values out of range");
        }
        if (GetValueOut() > nValueIn)
            return false;
    }

    return true;
}




/**
 * Extract address type and hash160 from a script for address indexing.
 * Returns true if the script is a supported type (P2PKH or P2SH).
 */
static bool GetAddressFromScript(const CScript& script, int& nType, uint160& hashBytes)
{
    CTxDestination dest;
    if (!ExtractDestination(script, dest))
        return false;

    const CKeyID* keyId = std::get_if<CKeyID>(&dest);
    if (keyId) {
        nType = ADDR_TYPE_P2PKH;
        hashBytes = *keyId;
        return true;
    }

    const CScriptID* scriptId = std::get_if<CScriptID>(&dest);
    if (scriptId) {
        nType = ADDR_TYPE_P2SH;
        hashBytes = *scriptId;
        return true;
    }

    return false;
}

bool CBlock::DisconnectBlock(CTxDBBase& txdb, CBlockIndex* pindex)
{
    // Disconnect in reverse order
    for (int i = vtx.size()-1; i >= 0; i--)
        if (!vtx[i].DisconnectInputs(txdb))
            return false;

    // Undo UTXO entries for this block (reverse of ConnectBlock's UTXO writes)
    for (int i = (int)vtx.size()-1; i >= 0; i--)
    {
        const CTransaction& tx = vtx[i];
        uint256 txhash = tx.GetHash();

        // Erase outputs this block created
        for (unsigned int k = 0; k < tx.vout.size(); k++)
        {
            if (!tx.vout[k].IsEmpty())
                txdb.EraseUtxo(txhash, k);
        }

        // Restore inputs this block spent (read prev tx from disk to rebuild UTXO entry)
        if (!tx.IsCoinBase())
        {
            for (const CTxIn& txin : tx.vin)
            {
                CTransaction txPrev;
                CTxIndex txindex;
                if (txdb.ReadDiskTx(txin.prevout.hash, txPrev, txindex))
                {
                    if (txin.prevout.n < txPrev.vout.size())
                    {
                        const CTxOut& prevout = txPrev.vout[txin.prevout.n];
                        CUtxoEntry utxo;
                        utxo.nValue = prevout.nValue;
                        utxo.nHeight = 0; // approximation; exact height not critical for restored UTXOs
                        utxo.scriptPubKey = prevout.scriptPubKey;
                        utxo.fCoinBase = txPrev.IsCoinBase();
                        utxo.fCoinStake = txPrev.IsCoinStake();
                        utxo.nTxTime = txPrev.nTime;
                        txdb.WriteUtxo(txin.prevout.hash, txin.prevout.n, utxo);
                    }
                }
            }
        }
    }

    // Undo address index entries for this block
    if (fAddressIndex)
    {
        for (int i = (int)vtx.size()-1; i >= 0; i--)
        {
            const CTransaction& tx = vtx[i];
            uint256 txhash = tx.GetHash();

            // Undo outputs (remove UTXOs, subtract from balance)
            for (unsigned int k = 0; k < tx.vout.size(); k++)
            {
                const CTxOut& txout = tx.vout[k];
                int nType;
                uint160 hashBytes;
                if (GetAddressFromScript(txout.scriptPubKey, nType, hashBytes))
                {
                    txdb.EraseAddressUtxo(nType, hashBytes, txhash, k);
                    int64_t nBalance = 0;
                    txdb.ReadAddressBalance(nType, hashBytes, nBalance);
                    nBalance -= txout.nValue;
                    txdb.WriteAddressBalance(nType, hashBytes, nBalance);
                    txdb.EraseAddressTxId(nType, hashBytes, pindex->nHeight, i, txhash);
                }
            }

            // Undo inputs (re-add spent UTXOs, add back to balance)
            if (!tx.IsCoinBase())
            {
                for (unsigned int j = 0; j < tx.vin.size(); j++)
                {
                    const CTxIn& txin = tx.vin[j];
                    CTransaction txPrev;
                    CTxIndex txindex;
                    if (txdb.ReadDiskTx(txin.prevout.hash, txPrev, txindex))
                    {
                        if (txin.prevout.n < txPrev.vout.size())
                        {
                            const CTxOut& prevout = txPrev.vout[txin.prevout.n];
                            int nType;
                            uint160 hashBytes;
                            if (GetAddressFromScript(prevout.scriptPubKey, nType, hashBytes))
                            {
                                // Re-add the UTXO that was spent
                                // Find the height of the prev tx block
                                int nPrevHeight = 0;
                                if (txindex.pos.nBlockPos > 0)
                                {
                                    CBlock blockPrev;
                                    // Use a rough estimate - look up via block index
                                    // The exact height isn't critical for the UTXO entry
                                    nPrevHeight = pindex->nHeight; // approximation
                                }
                                txdb.WriteAddressUtxo(nType, hashBytes, txin.prevout.hash, txin.prevout.n,
                                                      prevout.nValue, nPrevHeight, prevout.scriptPubKey);
                                int64_t nBalance = 0;
                                txdb.ReadAddressBalance(nType, hashBytes, nBalance);
                                nBalance += prevout.nValue;
                                txdb.WriteAddressBalance(nType, hashBytes, nBalance);
                            }
                        }
                    }
                }
            }
        }
    }

    // Update block index on disk without changing it in memory.
    // The memory index structure will be changed after the db commits.
    if (pindex->pprev)
    {
        CDiskBlockIndex blockindexPrev(pindex->pprev);
        blockindexPrev.hashNext = 0;
        if (!txdb.WriteBlockIndex(blockindexPrev))
            return error("DisconnectBlock() : WriteBlockIndex failed");
    }

    // triangles: clean up wallet after disconnecting coinstake
    for (CTransaction& tx : vtx)
        SyncWithWallets(tx, this, false, false);

    return true;
}

bool CBlock::ConnectBlock(CTxDBBase& txdb, CBlockIndex* pindex, bool fJustCheck)
{
    // Check it again in case a previous version let a bad block in, but skip BlockSig checking
    if (!CheckBlock(!fJustCheck, !fJustCheck, false))
        return false;

    // Determine if this block is covered by the hardcoded checkpoint or
    // our rolling assumeValid threshold. Below either: skip all input
    // validation, FetchInputs, ConnectInputs, and wallet sync. Trust
    // comes from either the static checkpoint map (compile-time, signed
    // hashes baked into the binary) OR our own prior validation history
    // (nAssumeValidThreshold, advanced after each successful connect).
    //
    // For the rolling threshold: only the last ASSUME_VALID_BUFFER blocks
    // are fully validated every time. Everything older takes the fast
    // path because we've already connected it successfully. A reorg that
    // tries to rewrite within the buffer is caught by full validation.
    bool fAssumeValid = IsConsensusAssumeValidHeight(pindex->nHeight);
    bool fIsInitialDownload = IsInitialBlockDownload();

    //// issue here: it doesn't know the version
    unsigned int nTxPos;
    if (fJustCheck)
        // FetchInputs treats CDiskTxPos(1,1,1) as a special "refer to memorypool" indicator
        // Since we're just checking the block and not actually connecting it, it might not (and probably shouldn't) be on the disk to get the transaction from
        nTxPos = 1;
    else
        nTxPos = pindex->nBlockPos + ::GetSerializeSize(CBlock(), SER_DISK, CLIENT_VERSION) - (2 * GetSizeOfCompactSize(0)) + GetSizeOfCompactSize(vtx.size());

    map<uint256, CTxIndex> mapQueuedChanges;  // tx position index (for getrawtransaction)
    MapPrevTx mapPendingUtxos;                // in-block UTXO tracking
    std::vector<CScriptCheck> vChecks;
    CCheckQueueControl<CScriptCheck> scriptcheckcontrol(pScriptCheckQueue.get());
    int64_t nFees = 0;
    int64_t nValueIn = 0;
    int64_t nValueOut = 0;
    int64_t nStakeReward = 0;
    unsigned int nSigOps = 0;
    for (CTransaction& tx : vtx)
    {
        uint256 hashTx = tx.GetHash();

        CDiskTxPos posThisTx(pindex->nFile, pindex->nBlockPos, nTxPos);
        if (!fJustCheck)
            nTxPos += ::GetSerializeSize(tx, SER_DISK, CLIENT_VERSION);

        // Record tx position for getrawtransaction (both fast and full paths)
        mapQueuedChanges[hashTx] = CTxIndex(posThisTx, tx.vout.size());

        // Fast path: below checkpoint, skip all input validation.
        // Track pending UTXOs so later txs in the same block can find inputs.
        if (fAssumeValid)
        {
            // Track money supply from input/output values
            int64_t nTxValueOut = tx.GetValueOut();
            nValueOut += nTxValueOut;

            if (!tx.IsCoinBase())
            {
                int64_t nTxValueIn = 0;
                for (const CTxIn& txin : tx.vin)
                {
                    // Check in-block pending UTXOs first, then UTXO database
                    auto it = mapPendingUtxos.find(txin.prevout);
                    if (it != mapPendingUtxos.end())
                        nTxValueIn += it->second.nValue;
                    else
                    {
                        CUtxoEntry utxo;
                        if (txdb.ReadUtxo(txin.prevout.hash, txin.prevout.n, utxo))
                            nTxValueIn += utxo.nValue;
                    }
                }
                nValueIn += nTxValueIn;
                if (!tx.IsCoinStake())
                    nFees += nTxValueIn - nTxValueOut;
            }

            // Add outputs to pending UTXOs
            for (unsigned int k = 0; k < tx.vout.size(); k++)
            {
                if (!tx.vout[k].IsEmpty())
                {
                    CUtxoEntry entry;
                    entry.nValue = tx.vout[k].nValue;
                    entry.nHeight = pindex->nHeight;
                    entry.scriptPubKey = tx.vout[k].scriptPubKey;
                    entry.fCoinBase = tx.IsCoinBase();
                    entry.fCoinStake = tx.IsCoinStake();
                    entry.nTxTime = tx.nTime;
                    mapPendingUtxos[COutPoint(hashTx, k)] = entry;
                }
            }
            // Remove spent inputs from pending UTXOs
            if (!tx.IsCoinBase())
                for (const CTxIn& txin : tx.vin)
                    mapPendingUtxos.erase(txin.prevout);
            continue;
        }

        // Full validation path (above checkpoint)

        // BIP30: check for duplicate transaction with unspent outputs.
        // With UTXO model, if any output of this txid exists in the UTXO DB, it's a duplicate.
        for (unsigned int k = 0; k < tx.vout.size(); k++)
        {
            if (!tx.vout[k].IsEmpty() && txdb.HaveUtxo(hashTx, k))
                return false;
        }

        nSigOps += tx.GetLegacySigOpCount();
        if (nSigOps > MAX_BLOCK_SIGOPS)
            return DoS(100, error("ConnectBlock() : too many sigops"));

        MapPrevTx mapInputs;
        if (tx.IsCoinBase())
            nValueOut += tx.GetValueOut();
        else
        {
            bool fInvalid;
            if (!tx.FetchInputs(txdb, mapPendingUtxos, true, false, mapInputs, fInvalid))
                return false;

            // Add in sigops done by pay-to-script-hash inputs;
            // this is to prevent a "rogue miner" from creating
            // an incredibly-expensive-to-validate block.
            nSigOps += tx.GetP2SHSigOpCount(mapInputs);
            if (nSigOps > MAX_BLOCK_SIGOPS)
                return DoS(100, error("ConnectBlock() : too many sigops"));

            int64_t nTxValueIn = tx.GetValueIn(mapInputs);
            int64_t nTxValueOut = tx.GetValueOut();
            nValueIn += nTxValueIn;
            nValueOut += nTxValueOut;
            if (!tx.IsCoinStake())
                nFees += nTxValueIn - nTxValueOut;
            if (tx.IsCoinStake())
                nStakeReward = nTxValueOut - nTxValueIn;

            if (!tx.ConnectInputs(txdb, mapInputs, pindex, true, false,
                                  pScriptCheckQueue ? &vChecks : nullptr))
                return false;
            if (pScriptCheckQueue && vChecks.size() >= 32)
            {
                scriptcheckcontrol.Add(vChecks);
                vChecks.clear();
            }
        }

        // Add this tx's outputs to pending UTXOs for later txs in the block
        for (unsigned int k = 0; k < tx.vout.size(); k++)
        {
            if (!tx.vout[k].IsEmpty())
            {
                CUtxoEntry entry;
                entry.nValue = tx.vout[k].nValue;
                entry.nHeight = pindex->nHeight;
                entry.scriptPubKey = tx.vout[k].scriptPubKey;
                entry.fCoinBase = tx.IsCoinBase();
                entry.fCoinStake = tx.IsCoinStake();
                entry.nTxTime = tx.nTime;
                mapPendingUtxos[COutPoint(hashTx, k)] = entry;
            }
        }
        // Remove spent inputs from pending UTXOs
        if (!tx.IsCoinBase())
            for (const CTxIn& txin : tx.vin)
                mapPendingUtxos.erase(txin.prevout);
    }

    if (!fAssumeValid)
    {
        scriptcheckcontrol.Add(vChecks);
        vChecks.clear();
        if (!scriptcheckcontrol.Wait())
            return DoS(100, error("ConnectBlock() : script verification failed"));

        if (IsProofOfWork())
        {
            int64_t nReward = GetProofOfWorkReward(nFees);
            // Check coinbase reward
            if (vtx[0].GetValueOut() > nReward)
                return DoS(50, error("ConnectBlock() : coinbase reward exceeded (actual=%" PRId64 " vs calculated=%" PRId64 ")",
                       vtx[0].GetValueOut(),
                       nReward));
        }
        if (IsProofOfStake())
        {
            // triangles: coin stake tx earns reward instead of paying fee
            uint64_t nCoinAge;
            if (!vtx[1].GetCoinAge(txdb, nCoinAge))
                return error("ConnectBlock() : %s unable to get coin age for coinstake", vtx[1].GetHash().ToString().substr(0,10).c_str());

            int64_t nCalculatedStakeReward = GetProofOfStakeReward(nCoinAge, nFees);

            // Enforce coinstake reward for every fully validated block.
            // Historical checkpoint / rolling-assume-valid blocks take the
            // fAssumeValid fast path above; stale-tip IBD must not disable
            // live reward validation for blocks above that fast path.
            if (!fAssumeValid)
            {
                if (nStakeReward > nCalculatedStakeReward)
                    return DoS(100, error("ConnectBlock() : coinstake pays too much(actual=%" PRId64 " vs calculated=%" PRId64 ")", nStakeReward, nCalculatedStakeReward));
            }
        }
    }

    // triangles: track money supply and mint amount info
    pindex->nMint = nValueOut - nValueIn + nFees;
    pindex->nMoneySupply = (pindex->pprev? pindex->pprev->nMoneySupply : 0) + nValueOut - nValueIn;
    if (!txdb.WriteBlockIndex(CDiskBlockIndex(pindex)))
        return error("Connect() : WriteBlockIndex for pindex failed");

    if (fJustCheck)
        return true;

    // Write queued txindex changes
    for (const auto& [hash, txindex] : mapQueuedChanges)
    {
        if (!txdb.UpdateTxIndex(hash, txindex))
            return error("ConnectBlock() : UpdateTxIndex failed");
    }

    // Write UTXO database entries: add new outputs, erase spent inputs.
    // Runs for both fAssumeValid (fast) and full validation paths.
    for (const CTransaction& tx : vtx)
    {
        uint256 hashTx = tx.GetHash();

        for (unsigned int k = 0; k < tx.vout.size(); k++)
        {
            const CTxOut& txout = tx.vout[k];
            if (txout.IsEmpty())
                continue;

            CUtxoEntry utxo;
            utxo.nValue = txout.nValue;
            utxo.nHeight = pindex->nHeight;
            utxo.scriptPubKey = txout.scriptPubKey;
            utxo.fCoinBase = tx.IsCoinBase();
            utxo.fCoinStake = tx.IsCoinStake();
            utxo.nTxTime = tx.nTime;
            if (!txdb.WriteUtxo(hashTx, k, utxo))
                return error("ConnectBlock() : WriteUtxo failed");
        }

        if (!tx.IsCoinBase())
        {
            for (const CTxIn& txin : tx.vin)
            {
                if (!txdb.EraseUtxo(txin.prevout.hash, txin.prevout.n))
                    return error("ConnectBlock() : EraseUtxo failed");
            }
        }
    }

    // Update address index
    if (fAddressIndex)
    {
        // Batch balance deltas: accumulate net change per address, then
        // do a single read-modify-write per unique address at the end.
        // This avoids hundreds of per-output DB reads/writes per block.
        std::map<std::pair<int, uint160>, int64_t> mapBalanceDeltas;

        for (unsigned int i = 0; i < vtx.size(); i++)
        {
            const CTransaction& tx = vtx[i];
            uint256 txhash = tx.GetHash();

            // Index spent inputs (remove UTXOs, reduce balance)
            if (!tx.IsCoinBase())
            {
                for (unsigned int j = 0; j < tx.vin.size(); j++)
                {
                    const CTxIn& txin = tx.vin[j];
                    bool fFoundPrevout = false;

                    // Check mapPendingUtxos first to avoid a DB hit for
                    // outputs created earlier in this same block.
                    auto itPending = mapPendingUtxos.find(txin.prevout);
                    if (itPending != mapPendingUtxos.end())
                    {
                        const CUtxoEntry& utxo = itPending->second;
                        int nType;
                        uint160 hashBytes;
                        if (GetAddressFromScript(utxo.scriptPubKey, nType, hashBytes))
                        {
                            txdb.EraseAddressUtxo(nType, hashBytes, txin.prevout.hash, txin.prevout.n);
                            mapBalanceDeltas[std::make_pair(nType, hashBytes)] -= utxo.nValue;
                        }
                        fFoundPrevout = true;
                    }

                    // Fall back to reading the full transaction from disk
                    if (!fFoundPrevout)
                    {
                        CTransaction txPrev;
                        CTxIndex txindex;
                        if (txdb.ReadDiskTx(txin.prevout.hash, txPrev, txindex))
                        {
                            if (txin.prevout.n < txPrev.vout.size())
                            {
                                const CTxOut& prevout = txPrev.vout[txin.prevout.n];
                                int nType;
                                uint160 hashBytes;
                                if (GetAddressFromScript(prevout.scriptPubKey, nType, hashBytes))
                                {
                                    txdb.EraseAddressUtxo(nType, hashBytes, txin.prevout.hash, txin.prevout.n);
                                    mapBalanceDeltas[std::make_pair(nType, hashBytes)] -= prevout.nValue;
                                }
                            }
                        }
                    }
                }
            }

            // Index new outputs (add UTXOs, increase balance)
            for (unsigned int k = 0; k < tx.vout.size(); k++)
            {
                const CTxOut& txout = tx.vout[k];
                if (txout.scriptPubKey.empty() || txout.nValue == 0)
                    continue;

                int nType;
                uint160 hashBytes;
                if (GetAddressFromScript(txout.scriptPubKey, nType, hashBytes))
                {
                    // Add new UTXO
                    txdb.WriteAddressUtxo(nType, hashBytes, txhash, k,
                                          txout.nValue, pindex->nHeight, txout.scriptPubKey);
                    // Accumulate balance increase (batched write at end)
                    mapBalanceDeltas[std::make_pair(nType, hashBytes)] += txout.nValue;
                    // Record tx in address history
                    txdb.WriteAddressTxId(nType, hashBytes, pindex->nHeight, i, txhash);
                }
            }
        }

        // Batch-write all accumulated balance changes: one read + one write
        // per unique address instead of per-output.
        for (const auto& entry : mapBalanceDeltas)
        {
            if (entry.second == 0)
                continue;
            int64_t nBalance = 0;
            txdb.ReadAddressBalance(entry.first.first, entry.first.second, nBalance);
            nBalance += entry.second;
            txdb.WriteAddressBalance(entry.first.first, entry.first.second, nBalance);
        }
    }

    // Update block index on disk without changing it in memory.
    // The memory index structure will be changed after the db commits.
    if (pindex->pprev)
    {
        CDiskBlockIndex blockindexPrev(pindex->pprev);
        blockindexPrev.hashNext = pindex->GetBlockHash();
        if (!txdb.WriteBlockIndex(blockindexPrev))
            return error("ConnectBlock() : WriteBlockIndex failed");
    }

    // Skip wallet sync during IBD - a full wallet rescan runs when IBD completes.
    // This eliminates millions of per-transaction wallet lookups during sync.
    if (!fIsInitialDownload)
    {
        for (CTransaction& tx : vtx)
            SyncWithWallets(tx, this, true);
    }

    return true;
}

bool static Reorganize(CTxDBBase& txdb, CBlockIndex* pindexNew)
{
    printf("REORGANIZE: Switching chains\n");
    printf("  Old tip: %s height %d trust %s\n",
        pindexBest->GetBlockHash().ToString().substr(0,20).c_str(),
        pindexBest->nHeight,
        CBigNum(pindexBest->nChainTrust).ToString().c_str());
    printf("  New tip: %s height %d trust %s\n",
        pindexNew->GetBlockHash().ToString().substr(0,20).c_str(),
        pindexNew->nHeight,
        CBigNum(pindexNew->nChainTrust).ToString().c_str());

    // Find the fork
    CBlockIndex* pfork = pindexBest;
    CBlockIndex* plonger = pindexNew;
    while (pfork != plonger)
    {
        while (plonger->nHeight > pfork->nHeight)
            if (!(plonger = plonger->pprev))
                return error("Reorganize() : plonger->pprev is null");
        if (pfork == plonger)
            break;
        if (!(pfork = pfork->pprev))
            return error("Reorganize() : pfork->pprev is null");
    }

    // Convergence rule (fix/consensus-convergence):
    //
    // Above the last globally shared hardened checkpoint, the valid chain
    // with strictly greater cumulative chain trust wins — no depth cap,
    // no local finality, no trust hysteresis.
    //
    // Below the hardened checkpoint: reject unconditionally. The
    // checkpoint is sourced from the same compiled map (Checkpoints::
    // mapCheckpoints via GetLastCheckpointHeight) on every node, so
    // it is a globally shared anchor, not locally invented finality.
    //
    // pindexLastHardenedCheckpoint is set at startup from the same map,
    // keyed by mapBlockIndex lookup of the compiled checkpoint hash. If
    // that lookup fails (early IBD, reindex, or bootstrap before the
    // checkpoint block has been downloaded into the local block index)
    // the pointer is NULL. In that state we still know the *height* of
    // the checkpoint from the compiled map directly — every node built
    // from the same binary sees the same value — and we use it as the
    // fail-closed floor. Without this second path, an IBD-time reorg
    // attempt below the compiled checkpoint height would silently slip
    // through the guard.
    int nHardenedCheckpointHeight = -1;
    if (pindexLastHardenedCheckpoint)
        nHardenedCheckpointHeight = pindexLastHardenedCheckpoint->nHeight;
    else
        nHardenedCheckpointHeight = Checkpoints::GetLastCheckpointHeight();
    if (nHardenedCheckpointHeight >= 0 && pfork->nHeight <= nHardenedCheckpointHeight)
    {
        printf("REORGANIZE: REJECTED — fork point %d is at or below shared hardened checkpoint %d\n",
            pfork->nHeight, nHardenedCheckpointHeight);
        return error("Reorganize() : fork point %d at or below shared hardened checkpoint %d",
            pfork->nHeight, nHardenedCheckpointHeight);
    }

    // List of what to disconnect
    vector<CBlockIndex*> vDisconnect;
    for (CBlockIndex* pindex = pindexBest; pindex != pfork; pindex = pindex->pprev)
        vDisconnect.push_back(pindex);

    // List of what to connect
    vector<CBlockIndex*> vConnect;
    for (CBlockIndex* pindex = pindexNew; pindex != pfork; pindex = pindex->pprev)
        vConnect.push_back(pindex);
    reverse(vConnect.begin(), vConnect.end());

    printf("REORGANIZE: Fork point at height %d: %s\n",
        pfork->nHeight,
        pfork->GetBlockHash().ToString().substr(0,20).c_str());
    printf("REORGANIZE: Disconnect %" PRIszu " blocks (heights %d..%d)\n",
        vDisconnect.size(),
        pfork->nHeight + 1,
        pindexBest->nHeight);
    printf("REORGANIZE: Connect %" PRIszu " blocks (heights %d..%d)\n",
        vConnect.size(),
        pfork->nHeight + 1,
        pindexNew->nHeight);

    // Disconnect shorter branch
    vector<CTransaction> vResurrect;
    for (CBlockIndex* pindex : vDisconnect)
    {
        CBlock block;
        if (!block.ReadFromDisk(pindex))
            return error("Reorganize() : ReadFromDisk for disconnect failed");
        if (!block.DisconnectBlock(txdb, pindex))
            return error("Reorganize() : DisconnectBlock %s failed", pindex->GetBlockHash().ToString().substr(0,20).c_str());

        // Queue memory transactions to resurrect
        for (const CTransaction& tx : block.vtx)
            if (!(tx.IsCoinBase() || tx.IsCoinStake()))
                vResurrect.push_back(tx);
    }

    // Connect longer branch
    vector<CTransaction> vDelete;
    for (CBlockIndex* pindex : vConnect)
    {
        CBlock block;
        if (!block.ReadFromDisk(pindex))
            return error("Reorganize() : ReadFromDisk for connect failed");
        if (!block.ConnectBlock(txdb, pindex))
        {
            // Invalid block
            return error("Reorganize() : ConnectBlock %s failed", pindex->GetBlockHash().ToString().substr(0,20).c_str());
        }

        // Queue memory transactions to delete
        for (const CTransaction& tx : block.vtx)
            vDelete.push_back(tx);
    }
    if (!txdb.WriteHashBestChain(pindexNew->GetBlockHash()))
        return error("Reorganize() : WriteHashBestChain failed");
    if (!UpdateAddressIndexSyncState(txdb, pindexNew))
        return error("Reorganize() : WriteAddressIndexBestChain failed");

    // Make sure it's successfully written to disk before changing memory structure
    if (!txdb.TxnCommit())
        return error("Reorganize() : TxnCommit failed");

    // ======================================================================
    // CRITICAL: All operations below this point must be in-memory only and
    // should never fail. The DB transaction is committed, so we cannot abort.
    // ======================================================================

    // Disconnect shorter branch (in-memory only)
    for (CBlockIndex* pindex : vDisconnect)
        if (pindex->pprev)
            pindex->pprev->pnext = nullptr;

    // Connect longer branch (in-memory only)
    for (CBlockIndex* pindex : vConnect)
        if (pindex->pprev)
            pindex->pprev->pnext = pindex;

    // Remove disconnected PoS blocks from setStakeSeen so they don't
    // block acceptance of valid blocks on the winning chain.
    // This MUST happen after commit to maintain consistency.
    for (CBlockIndex* pindex : vDisconnect)
        if (pindex->IsProofOfStake())
            setStakeSeen.erase(make_pair(pindex->prevoutStake, pindex->nStakeTime));

    // Resurrect memory transactions that were in the disconnected branch
    unsigned int nResurrected = 0;
    for (CTransaction& tx : vResurrect)
    {
        if (tx.AcceptToMemoryPool(txdb, false))
            nResurrected++;
    }
    if (nResurrected > 0)
        printf("REORGANIZE: resurrected %u transactions to mempool\n", nResurrected);

    // Delete redundant memory transactions that are in the connected branch
    for (CTransaction& tx : vDelete) {
        mempool.remove(tx);
        mempool.removeConflicts(tx);
    }

    printf("REORGANIZE: done (fork at height %d, %zu disconnected, %zu connected)\n",
           pfork->nHeight, vDisconnect.size(), vConnect.size());

    return true;
}


// Called from inside SetBestChain: attaches a block to the new best chain being built
bool CBlock::SetBestChainInner(CTxDBBase& txdb, CBlockIndex *pindexNew)
{
    uint256 hash = GetHash();

    // Adding to current best branch
    if (!ConnectBlock(txdb, pindexNew) || !txdb.WriteHashBestChain(hash) || !UpdateAddressIndexSyncState(txdb, pindexNew))
    {
        txdb.TxnAbort();
        InvalidChainFound(pindexNew);
        return false;
    }
    if (!txdb.TxnCommit())
        return error("SetBestChain() : TxnCommit failed");

    // Add to current best branch
    pindexNew->pprev->pnext = pindexNew;

    // Delete redundant memory transactions
    for (CTransaction& tx : vtx)
        mempool.remove(tx);

    return true;
}

bool CBlock::SetBestChain(CTxDBBase& txdb, CBlockIndex* pindexNew)
{
    uint256 hash = GetHash();

    if (!txdb.TxnBegin())
        return error("SetBestChain() : TxnBegin failed");

    if (pindexGenesisBlock == nullptr && hash == (!fTestNet ? hashGenesisBlockOfficial : hashGenesisBlockTestNet))
    {
        txdb.WriteHashBestChain(hash);
        if (!UpdateAddressIndexSyncState(txdb, pindexNew))
            return error("SetBestChain() : WriteAddressIndexBestChain failed");
        if (!txdb.TxnCommit())
            return error("SetBestChain() : TxnCommit failed");
        pindexGenesisBlock = pindexNew;
    }
    else if (hashPrevBlock == hashBestChain)
    {
        if (!SetBestChainInner(txdb, pindexNew))
            return error("SetBestChain() : SetBestChainInner failed");
    }
    else
    {
        // the first block in the new chain that will cause it to become the new best chain
        CBlockIndex *pindexIntermediate = pindexNew;

        // list of blocks that need to be connected afterwards
        std::vector<CBlockIndex*> vpindexSecondary;

        // Reorganize is costly in terms of db load, as it works in a single db transaction.
        // Try to limit how much needs to be done inside
        while (pindexIntermediate->pprev && pindexIntermediate->pprev->nChainTrust > pindexBest->nChainTrust)
        {
            vpindexSecondary.push_back(pindexIntermediate);
            pindexIntermediate = pindexIntermediate->pprev;
        }

        if (!vpindexSecondary.empty())
            printf("Postponing %" PRIszu " reconnects\n", vpindexSecondary.size());

        // Switch to new best branch
        if (!Reorganize(txdb, pindexIntermediate))
        {
            txdb.TxnAbort();
            InvalidChainFound(pindexNew);
            return error("SetBestChain() : Reorganize failed");
        }

        // Connect further blocks
        for (auto rit = vpindexSecondary.rbegin(); rit != vpindexSecondary.rend(); ++rit)
        {
            CBlockIndex *pindex = *rit;
            CBlock block;
            if (!block.ReadFromDisk(pindex))
            {
                printf("SetBestChain() : ReadFromDisk failed\n");
                break;
            }
            if (!txdb.TxnBegin()) {
                printf("SetBestChain() : TxnBegin 2 failed\n");
                break;
            }
            // errors now are not fatal, we still did a reorganisation to a new chain in a valid way
            if (!block.SetBestChainInner(txdb, pindex))
                break;
        }
    }

    // Update best block in wallet (so we can detect restored wallets).
    // During IBD, skip this so the wallet knows it needs rescanning on restart.
    bool fIsInitialDownload = IsInitialBlockDownload();
    if (!fIsInitialDownload)
    {
        const CBlockLocator locator(pindexNew);
        ::SetBestChain(locator);
    }

    // New best block
    hashBestChain = hash;
    pindexBest = pindexNew;
    pblockindexFBBHLast = nullptr;
    nBestHeight = pindexBest->nHeight;
    nBestChainTrust = pindexNew->nChainTrust;
    nTimeBestReceived = GetTime();
    nTransactionsUpdated++;

    // pindexLastHardenedCheckpoint is intentionally NOT advanced here. See
    // fix/consensus-convergence in init.cpp and Reorganize().

    // Rolling assumeValid threshold: advance so blocks older than
    // ASSUME_VALID_BUFFER from the tip take the fast path on future
    // connects. We do this AFTER the finality checkpoint update so the
    // fast-path boundary always lags the finality boundary by at least
    // ASSUME_VALID_BUFFER — no gap, no overlap risk on reorgs.
    //
    // Only advance when fully synced. During IBD we want full validation
    // until we're confident the chain is correct, then we can lean on
    // prior validation history.
    if (!IsInitialBlockDownload() && nBestHeight > (int)ASSUME_VALID_BUFFER)
    {
        int newThreshold = nBestHeight - (int)ASSUME_VALID_BUFFER;
        if (newThreshold > nAssumeValidThreshold)
        {
            nAssumeValidThreshold = newThreshold;
            printf("ASSUME-VALID: threshold advanced to block %d (full validation only for last %d blocks)\n",
                nAssumeValidThreshold, ASSUME_VALID_BUFFER);
        }
    }

    uint256 nBestBlockTrust = (pindexBest->nHeight != 0 && pindexBest->pprev) ? (pindexBest->nChainTrust - pindexBest->pprev->nChainTrust) : pindexBest->nChainTrust;

    // Log every 5000 blocks during sync, every block once caught up
    if (nBestHeight % 5000 == 0 || !IsInitialBlockDownload())
    {
        static int64_t nLastLogTime = 0;
        static int nLastLogHeight = 0;
        int64_t nNow = GetTimeMillis();
        double dRate = 0;
        if (nLastLogTime > 0 && nNow > nLastLogTime)
            dRate = (double)(nBestHeight - nLastLogHeight) * 1000.0 / (double)(nNow - nLastLogTime);
        printf("SetBestChain: new best=%s  height=%d  trust=%s  blocktrust=%" PRId64 "  date=%s  %.1f blk/s\n",
          hashBestChain.ToString().substr(0,20).c_str(), nBestHeight,
          CBigNum(nBestChainTrust).ToString().c_str(),
          nBestBlockTrust.Get64(),
          DateTimeStrFormat("%x %H:%M:%S", pindexBest->GetBlockTime()).c_str(),
          dRate);
        nLastLogTime = nNow;
        nLastLogHeight = nBestHeight;
    }

    if (fDebug)
		printf("Stake checkpoint: %x\n", pindexBest->nStakeModifierChecksum);

    // Check the version of the last 100 blocks to see if we need to upgrade:
    if (!fIsInitialDownload)
    {
        int nUpgraded = 0;
        const CBlockIndex* pindex = pindexBest;
        for (int i = 0; i < 100 && pindex != nullptr; i++)
        {
            if (pindex->nVersion > CBlock::CURRENT_VERSION)
                ++nUpgraded;
            pindex = pindex->pprev;
        }
        if (nUpgraded > 0)
            printf("SetBestChain: %d of last 100 blocks above version %d\n", nUpgraded, CBlock::CURRENT_VERSION);
        if (nUpgraded > 100/2)
            // strMiscWarning is read by GetWarnings(), called by Qt and the JSON-RPC code to warn the user:
            strMiscWarning = _("Warning: This version is obsolete, upgrade required!");
    }

    std::string strCmd = GetArg(std::string_view{"-blocknotify"}, std::string_view{""});

    if (!fIsInitialDownload && !strCmd.empty())
    {
        ReplaceAll(strCmd, "%s", hashBestChain.GetHex());
        std::thread(runCommand, strCmd).detach(); // thread runs free
    }

#ifdef ENABLE_ZMQ
    if (!fIsInitialDownload && pzmqNotifier)
        pzmqNotifier->NotifyBlockHash(hashBestChain);
#endif

    // SSE notification for new block
    if (!fIsInitialDownload && pNotificationQueue)
    {
        std::string strBlockEvent = strprintf(
            "{\"type\":\"block\",\"hash\":\"%s\",\"height\":%d}",
            hashBestChain.GetHex().c_str(),
            pindexBest->nHeight);
        pNotificationQueue->Push(strBlockEvent);
    }

    // Detect IBD-to-synced transition and trigger deferred work:
    // wallet rescan (since SyncWithWallets was skipped) and smsg chain scan.
    {
        static bool fWasInitialDownload = true;
        if (fWasInitialDownload && !fIsInitialDownload)
        {
            printf("*** Initial block download complete at height %d ***\n", nBestHeight);

            // Trim orphan blocks to normal limit now that IBD is done
            LimitOrphanBlocks(MAX_ORPHAN_BLOCKS);

            // Update wallet best chain locator now that IBD is done
            const CBlockLocator locator(pindexBest);
            ::SetBestChain(locator);

            // Run expensive post-IBD scans in the background so reaching tip
            // is not blocked by wallet/message index rebuild work.
            bool fStartPostIbdWork = false;
            {
                LOCK(cs_PostIbdWork);
                if (!fPostIbdWorkStarted)
                {
                    fPostIbdWorkStarted = true;
                    fStartPostIbdWork = true;
                }
            }

            if (fStartPostIbdWork && !NewThread(ThreadPostIbdWork, nullptr))
            {
                LOCK(cs_PostIbdWork);
                fPostIbdWorkStarted = false;
                printf("Warning: post-IBD background work thread could not be started; scans skipped.\n");
            }
        }
        fWasInitialDownload = fIsInitialDownload;
    }

    return true;
}

// triangles: total coin age spent in transaction, in the unit of coin-days.
// Only those coins meeting minimum age requirement counts. As those
// transactions not in main chain are not currently indexed so we
// might not find out about their coin age. Older transactions are 
// guaranteed to be in main chain by sync-checkpoint. This rule is
// introduced to help nodes establish a consistent view of the coin
// age (trust score) of competing branches.
bool CTransaction::GetCoinAge(CTxDBBase& txdb, uint64_t& nCoinAge) const
{
    CBigNum bnCentSecond = 0;  // coin age in the unit of cent-seconds
    nCoinAge = 0;

    if (IsCoinBase())
        return true;

    for (const CTxIn& txin : vin)
    {
        // Look up the UTXO entry for this input
        CUtxoEntry utxo;
        if (!txdb.ReadUtxo(txin.prevout.hash, txin.prevout.n, utxo))
        {
            // Lazy fallback: try old CTxIndex path
            CTxIndex txindexFallback;
            if (!txdb.ReadTxIndex(txin.prevout.hash, txindexFallback))
                continue;
            CTransaction txPrev;
            if (!txPrev.ReadFromDisk(txindexFallback.pos))
                continue;
            if (txin.prevout.n >= txPrev.vout.size())
                continue;

            utxo.nValue = txPrev.vout[txin.prevout.n].nValue;
            utxo.scriptPubKey = txPrev.vout[txin.prevout.n].scriptPubKey;
            utxo.fCoinBase = txPrev.IsCoinBase();
            utxo.fCoinStake = txPrev.IsCoinStake();
            utxo.nTxTime = txPrev.nTime;
            utxo.nHeight = 0;
        }

        if (nTime < utxo.nTxTime)
            return false;  // Transaction timestamp violation

        // Read block header to check min age.
        // Use the tx position index to find the block file/position.
        CTxIndex txindex;
        if (!txdb.ReadTxIndex(txin.prevout.hash, txindex))
            continue;
        CBlock block;
        if (!block.ReadFromDisk(txindex.pos.nFile, txindex.pos.nBlockPos, false))
            return false; // unable to read block of previous transaction
        if (block.GetBlockTime() + nStakeMinAge > nTime)
            continue; // only count coins meeting min age requirement

        int64_t nValueIn = utxo.nValue;
        bnCentSecond += CBigNum(nValueIn) * (nTime - utxo.nTxTime) / CENT;

        if (fDebug && GetBoolArg("-printcoinage"))
            printf("coin age nValueIn=%" PRId64 " nTimeDiff=%d bnCentSecond=%s\n", nValueIn, nTime - utxo.nTxTime, bnCentSecond.ToString().c_str());
    }

    CBigNum bnCoinDay = bnCentSecond * CENT / (24 * 60 * 60);
    if (fDebug && GetBoolArg("-printcoinage"))
        printf("coin age bnCoinDay=%s\n", bnCoinDay.ToString().c_str());
    nCoinAge = bnCoinDay.getuint64();
    return true;
}

// triangles: total coin age spent in block, in the unit of coin-days.
bool CBlock::GetCoinAge(uint64_t& nCoinAge) const
{
    nCoinAge = 0;

    auto txdb_holder = MakeChainDB("r"); CTxDBBase& txdb = *txdb_holder;
    for (const CTransaction& tx : vtx)
    {
        uint64_t nTxCoinAge;
        if (tx.GetCoinAge(txdb, nTxCoinAge))
            nCoinAge += nTxCoinAge;
        else
            return false;
    }

    if (nCoinAge == 0) // block coin age minimum 1 coin-day
        nCoinAge = 1;
    if (fDebug && GetBoolArg("-printcoinage"))
        printf("block coin age total nCoinDays=%" PRId64 "\n", nCoinAge);
    return true;
}

bool CBlock::AddToBlockIndex(unsigned int nFile, unsigned int nBlockPos, const uint256& hashProofOfStake)
{
    // Check for duplicate
    uint256 hash = GetHash();
    if (mapBlockIndex.count(hash))
        return error("AddToBlockIndex() : %s already exists", hash.ToString().substr(0,20).c_str());

    // Construct new block index object
    CBlockIndex* pindexNew = new CBlockIndex(nFile, nBlockPos, *this);
    if (!pindexNew)
        return error("AddToBlockIndex() : new CBlockIndex failed");
    pindexNew->phashBlock = &hash;
    auto miPrev = mapBlockIndex.find(hashPrevBlock);
    if (miPrev != mapBlockIndex.end())
    {
        pindexNew->pprev = miPrev->second;
        pindexNew->nHeight = pindexNew->pprev->nHeight + 1;
    }

    // triangles: compute chain trust score
    pindexNew->nChainTrust = (pindexNew->pprev ? pindexNew->pprev->nChainTrust : 0) + pindexNew->GetBlockTrust();

    // triangles: compute stake entropy bit for stake modifier
    if (!pindexNew->SetStakeEntropyBit(GetStakeEntropyBit()))
            return error("AddToBlockIndex() : SetStakeEntropyBit() failed");

    // triangles: record proof-of-stake hash value
    pindexNew->hashProofOfStake = hashProofOfStake;

    // triangles: compute stake modifier
    // Skip expensive computation during initial sync for blocks far below checkpoint.
    // Only compute for last 1000 blocks before checkpoint and all blocks above it.
    // This is safe because PoS verification is already skipped below checkpoint.
    uint64_t nStakeModifier = 0;
    bool fGeneratedStakeModifier = false;
    int nCheckpointHeight = Checkpoints::GetTotalBlocksEstimate();
    if (pindexNew->nHeight >= nCheckpointHeight - 1000)
    {
        if (!ComputeNextStakeModifier(pindexNew->pprev, nStakeModifier, fGeneratedStakeModifier))
            return error("AddToBlockIndex() : ComputeNextStakeModifier() failed");
        pindexNew->SetStakeModifier(nStakeModifier, fGeneratedStakeModifier);
        pindexNew->nStakeModifierChecksum = GetStakeModifierChecksum(pindexNew);
        if (!CheckStakeModifierCheckpoints(pindexNew->nHeight, pindexNew->nStakeModifierChecksum))
            return error("AddToBlockIndex() : Rejected by stake modifier checkpoint height=%d, modifier=0x%016"PRIx64, pindexNew->nHeight, nStakeModifier);
    }
    else
    {
        // Set minimal defaults during fast import
        pindexNew->SetStakeModifier(0, pindexNew->nHeight == 0);
        pindexNew->nStakeModifierChecksum = GetStakeModifierChecksum(pindexNew);
        // Only check modifier checkpoint at genesis (the only one defined)
        if (pindexNew->nHeight == 0 && !CheckStakeModifierCheckpoints(0, pindexNew->nStakeModifierChecksum))
            return error("AddToBlockIndex() : Rejected by stake modifier checkpoint at genesis");
    }

    // Add to mapBlockIndex
    auto mi = mapBlockIndex.insert(make_pair(hash, pindexNew)).first;
    if (pindexNew->IsProofOfStake())
        setStakeSeen.insert(make_pair(pindexNew->prevoutStake, pindexNew->nStakeTime));
    pindexNew->phashBlock = &mi->first;

    // Write to disk block index
    auto txdb_holder = MakeChainDB(); CTxDBBase& txdb = *txdb_holder;
    if (!txdb.TxnBegin())
        return false;
    txdb.WriteBlockIndex(CDiskBlockIndex(pindexNew));

    // New best — keep the batch open so SetBestChain can add ConnectBlock
    // writes to the same transaction, cutting the per-block commit count in half.
    //
    // Chain selection rules:
    //   1. Strictly greater trust always wins (normal case).
    //      Deep reorgs are further gated by a 10% trust-delta check
    //      inside Reorganize() to prevent long-range attacks while
    //      still allowing natural short-fork convergence.
    //   2. Equal trust: deterministic tiebreaker with timestamp preference.
    //      First prefer the block with the earlier timestamp (lower nTime),
    //      then break remaining ties by lower hash. This converges faster
    //      because the earlier block is more likely to have propagated first.
    //      Rate-limited to one equal-trust reorg per 2 minutes.
    bool fNewBest = false;
    static int64_t nLastEqualTrustReorg = 0;
    if (pindexNew->nChainTrust > nBestChainTrust)
        fNewBest = true;
    else if (pindexNew->nChainTrust == nBestChainTrust && pindexBest &&
             GetTime() - nLastEqualTrustReorg > 2 * 60)
    {
        // Prefer earlier timestamp, then lower hash as final tiebreaker
        bool fPreferNew = false;
        if (pindexNew->nTime < pindexBest->nTime)
            fPreferNew = true;
        else if (pindexNew->nTime == pindexBest->nTime)
            fPreferNew = (pindexNew->GetBlockHash() < pindexBest->GetBlockHash());

        if (fPreferNew)
        {
            fNewBest = true;
            nLastEqualTrustReorg = GetTime();
        }
    }

    if (fNewBest)
    {
        if (!SetBestChain(txdb, pindexNew))
            return false;
    }
    else
    {
        if (!txdb.TxnCommit())
            return false;
    }

    if (pindexNew == pindexBest)
    {
        // Notify UI to display prev block's coinbase if it was ours
        static uint256 hashPrevBestCoinBase;
        UpdatedTransaction(hashPrevBestCoinBase);
        hashPrevBestCoinBase = vtx[0].GetHash();
    }

    uiInterface.NotifyBlocksChanged();
    return true;
}




bool CBlock::CheckBlock(bool fCheckPOW, bool fCheckMerkleRoot, bool fCheckSig) const
{
    // These are checks that are independent of context
    // that can be verified before saving an orphan block.

    // Size limits
    if (vtx.empty() || vtx.size() > MAX_BLOCK_SIZE || ::GetSerializeSize(*this, SER_NETWORK, PROTOCOL_VERSION) > MAX_BLOCK_SIZE)
        return DoS(100, error("CheckBlock() : size limits failed"));

    // Check proof of work matches claimed amount
    if (fCheckPOW && IsProofOfWork() && !CheckProofOfWork(GetHash(), nBits))
        return DoS(50, error("CheckBlock() : proof of work failed"));

    // Check timestamp: reject blocks obviously too far in the future.
    // Use a generous 15-minute window from the raw system clock.
    // GetAdjustedTime() is NOT used here because it incorporates peer-reported
    // time offsets that differ between Tor nodes, causing nondeterministic
    // block rejection — the primary cause of persistent chain splits.
    // The deterministic timestamp checks in AcceptBlock (median-time-past,
    // prev-block-time with 3-min drift) still enforce tight rules.
    if (GetBlockTime() > GetTime() + 15 * 60)
        return error("CheckBlock() : block timestamp too far in the future");

    // First transaction must be coinbase, the rest must not be
    if (vtx.empty() || !vtx[0].IsCoinBase())
        return DoS(100, error("CheckBlock() : first tx is not coinbase"));
    for (unsigned int i = 1; i < vtx.size(); i++)
        if (vtx[i].IsCoinBase())
            return DoS(100, error("CheckBlock() : more than one coinbase"));

    // Check coinbase timestamp
    // NOTE: Must use the pre-fork (10-minute) drift tolerance here because
    // CheckBlock() is context-free (no nHeight) and can be called on blocks
    // at any height, including during chain reorgs from old fork chains.
    // The tight 90-second drift for post-fork blocks is enforced separately
    // in AcceptBlock/ConnectBlock with proper height context.
    if (GetBlockTime() > (int64_t)vtx[0].nTime + 10 * 60)
        return DoS(50, error("CheckBlock() : coinbase timestamp is too early"));

    if (IsProofOfStake())
    {
        // Coinbase output should be empty if proof-of-stake block
        if (vtx[0].vout.size() != 1 || !vtx[0].vout[0].IsEmpty())
            return DoS(100, error("CheckBlock() : coinbase output not empty for proof-of-stake block"));

        // Second transaction must be coinstake, the rest must not be
        if (vtx.empty() || !vtx[1].IsCoinStake())
            return DoS(100, error("CheckBlock() : second tx is not coinstake"));
        for (unsigned int i = 2; i < vtx.size(); i++)
            if (vtx[i].IsCoinStake())
                return DoS(100, error("CheckBlock() : more than one coinstake"));

        // Check coinstake timestamp
        if (!CheckCoinStakeTimestamp(GetBlockTime(), (int64_t)vtx[1].nTime))
            return DoS(50, error("CheckBlock() : coinstake timestamp violation nTimeBlock=%" PRId64 " nTimeTx=%u", GetBlockTime(), vtx[1].nTime));

    	// triangles: check proof-of-stake block signature
        if (fCheckSig && !CheckBlockSignature())
            return DoS(100, error("CheckBlock() : bad proof-of-stake block signature"));
    }

    // Check transactions
    for (const CTransaction& tx : vtx)
    {
        if (!tx.CheckTransaction())
            return DoS(tx.nDoS, error("CheckBlock() : CheckTransaction failed"));

        // triangles: check transaction timestamp
        if (GetBlockTime() < (int64_t)tx.nTime)
            return DoS(50, error("CheckBlock() : block timestamp earlier than transaction timestamp"));
    }

    // Check for duplicate txids. This is caught by ConnectInputs(),
    // but catching it earlier avoids a potential DoS attack:
    set<uint256> uniqueTx;
    for (const CTransaction& tx : vtx)
    {
        uniqueTx.insert(tx.GetHash());
    }
    if (uniqueTx.size() != vtx.size())
        return DoS(100, error("CheckBlock() : duplicate transaction"));

    unsigned int nSigOps = 0;
    for (const CTransaction& tx : vtx)
    {
        nSigOps += tx.GetLegacySigOpCount();
    }
    if (nSigOps > MAX_BLOCK_SIGOPS)
        return DoS(100, error("CheckBlock() : out-of-bounds SigOpCount"));

    // Check merkle root
    if (fCheckMerkleRoot && hashMerkleRoot != BuildMerkleTree())
        return DoS(100, error("CheckBlock() : hashMerkleRoot mismatch"));


    return true;
}

bool CBlock::AcceptBlock()
{
    // Check for duplicate
    uint256 hash = GetHash();
    if (mapBlockIndex.count(hash))
        return error("AcceptBlock() : block already in mapBlockIndex");

    // Get prev block index
    auto mi = mapBlockIndex.find(hashPrevBlock);
    if (mi == mapBlockIndex.end())
        return DoS(10, error("AcceptBlock() : prev block not found"));
    CBlockIndex* pindexPrev = mi->second;
    int nHeight = pindexPrev->nHeight+1;

    if (IsProofOfWork() && nHeight > CUTOFF_POW_BLOCK)
        return DoS(100, error("AcceptBlock() : No proof-of-work allowed anymore (height = %d)", nHeight));

    if (IsProofOfStake() && nHeight < MODIFIER_INTERVAL_SWITCH)
        return DoS(100, error("AcceptBlock() : reject proof-of-stake at height %d", nHeight));

    // Check proof-of-work or proof-of-stake
    if (nBits != GetNextTargetRequired(pindexPrev, IsProofOfStake()))
    {
        if (nHeight > CRAPCHAIN_CUTOFF_BLOCK)
        {
            return DoS(100, error("AcceptBlock() : incorrect %s", IsProofOfWork() ? "proof-of-work" : "proof-of-stake"));
        }
        else
        {
        // blocks generated prior to Pharao release (version 4) are automatically accepted
        if (fDebug)
             printf("ProcessBlock(): pre-Pharao version proof-of-stake accepted for block %d\n", nHeight);
        }
    }
    else
        if (nHeight % 10000 == 0 || nHeight > 2186900)
            printf("ProcessBlock(): Check proof-of-stake/work OK for block %d\n", nHeight);
    // Check timestamp against prev
    if (GetBlockTime() <= pindexPrev->GetPastTimeLimit() || FutureDrift(GetBlockTime(), nHeight) < pindexPrev->GetBlockTime())
        return error("AcceptBlock() : block's timestamp is too early");

    // Check that all transactions are finalized
    for (const CTransaction& tx : vtx)
        if (!tx.IsFinal(nHeight, GetBlockTime()))
            return DoS(10, error("AcceptBlock() : contains a non-final transaction"));

    // Check that the block chain matches the known block chain up to a checkpoint
    if (!Checkpoints::CheckHardened(nHeight, hash))
        return DoS(100, error("AcceptBlock() : rejected by hardened checkpoint lock-in at %d", nHeight));

    // triangles: verify hash target and signature of coinstake tx
    uint256 hashProofOfStake = 0, targetProofOfStake = 0;
    if (IsProofOfStake())
    {
        // The rolling validation optimization is not a signature trust root.
        // Every PoS block above the compiled checkpoint must authorize its
        // exact block contents, including while the local tip is stale.
        if (IsBlockSignatureRequiredAtHeight(nHeight) && !CheckBlockSignature())
            return DoS(100, error("AcceptBlock() : bad proof-of-stake block signature at height %d", nHeight));

        if (IsConsensusAssumeValidHeight(nHeight))
        {
            // Historical fast path: blocks at/below hardcoded checkpoint or
            // rolling assume-valid have already been accepted by chain-level
            // trust, so skip expensive PoS kernel verification there only.
            // Do not key this off IsInitialBlockDownload(): stale-tip IBD is
            // operational state, not permission to accept unchecked live PoS.
            if (nHeight % 10000 == 0)
                printf("SKIP: PoS kernel check skipped for historical fast-path block %d\n", nHeight);
            hashProofOfStake = 0; targetProofOfStake = 0;
        }
        else
        {
            // Verify the PoS kernel signature normally for every live block
            // above the historical fast path, even if the tip is stale enough
            // for IsInitialBlockDownload() to be true.
            if (!CheckProofOfStake(vtx[1], nBits, hashProofOfStake, targetProofOfStake))
                return DoS(100, error("AcceptBlock() : check proof-of-stake failed for block %d", nHeight));
        }
    }

    // Sync checkpoint enforcement is disabled:
    // - Master key was removed in V5 fork, no new sync checkpoints will be broadcast
    // - Hardcoded checkpoints already guarantee chain integrity
    // - The persisted hashSyncCheckpoint in LevelDB blocks IBD from progressing

    // Legacy Triangles blocks were created before mandatory coinbase-height enforcement.
    // Do NOT enforce this rule against historical chain data during recovery/import.
    static const int COINBASE_HEIGHT_ENFORCEMENT_HEIGHT = 2300000;

    if (nHeight >= COINBASE_HEIGHT_ENFORCEMENT_HEIGHT)
    {
        CScript expect = CScript() << nHeight;
        if (vtx[0].vin[0].scriptSig.size() < expect.size() ||
            !std::equal(expect.begin(), expect.end(), vtx[0].vin[0].scriptSig.begin()))
            return DoS(100, error("AcceptBlock() : block height mismatch in coinbase"));
    }

    // Write block to history file
    if (!CheckDiskSpace(::GetSerializeSize(*this, SER_DISK, CLIENT_VERSION)))
        return error("AcceptBlock() : out of disk space");
    unsigned int nFile = -1;
    unsigned int nBlockPos = 0;
    if (!WriteToDisk(nFile, nBlockPos))
        return error("AcceptBlock() : WriteToDisk failed");
    if (!AddToBlockIndex(nFile, nBlockPos, hashProofOfStake))
        return error("AcceptBlock() : AddToBlockIndex failed");

    // Push new tip block directly to peers that are near our tip.
    // On a small Tor-only network the inv->getdata->block round-trip adds
    // 1-2 seconds of latency per hop. Pushing immediately cuts propagation
    // to a single hop. Uses nBestKnownHeight (updated from inv/block msgs)
    // rather than nStartingHeight (static, set at connect time only).
    //
    // For peers with fPreferHeaders (sendheaders negotiated), send a header
    // announcement — saves one round-trip vs inv->getdata->block.
    if (hashBestChain == hash)
    {
        LOCK(cs_vNodes);
        for (CNode* pnode : vNodes)
        {
            if (!pnode->fSuccessfullyConnected)
                continue;

            bool fNearTip = (pnode->nBestKnownHeight >= nBestHeight - 10) ||
                            (pnode->nBlocksDelivered > 0);
            if (fNearTip && pnode->fSendCmpct)
            {
                // BIP152 compact block relay: header + prefilled coinbase/coinstake +
                // short IDs for remaining txs.  For typical PoS blocks (0-2 txs)
                // this is the complete block — no follow-up needed.
                SendCompactBlock(pnode, *this);
            }
            else if (fNearTip)
            {
                // Direct full block push to near-tip peers
                pnode->PushMessage("block", *this);
                pnode->AddInventoryKnown(CInv(MSG_BLOCK, hash));
            }
            else if (pnode->fPreferHeaders)
            {
                // Header announcement for peers that requested sendheaders.
                // Construct a header-only CBlock (no transactions/signature).
                CBlock hdr;
                hdr.nVersion = nVersion;
                hdr.hashPrevBlock = hashPrevBlock;
                hdr.hashMerkleRoot = hashMerkleRoot;
                hdr.nTime = nTime;
                hdr.nBits = nBits;
                hdr.nNonce = nNonce;
                std::vector<CBlock> vHeaders(1, hdr);
                pnode->PushMessage("headers", vHeaders);
                pnode->AddInventoryKnown(CInv(MSG_BLOCK, hash));
            }
        }
    }

    return true;
}

uint256 CBlockIndex::GetBlockTrust() const
{
    CBigNum bnTarget;
    bnTarget.SetCompact(nBits);

    if (bnTarget <= 0)
        return 0;

    return ((CBigNum(1)<<256) / (bnTarget+1)).getuint256();
}

bool CBlockIndex::IsSuperMajority(int minVersion, const CBlockIndex* pstart, unsigned int nRequired, unsigned int nToCheck)
{
    unsigned int nFound = 0;
    for (unsigned int i = 0; i < nToCheck && nFound < nRequired && pstart != nullptr; i++)
    {
        if (pstart->nVersion >= minVersion)
            ++nFound;
        pstart = pstart->pprev;
    }
    return (nFound >= nRequired);
}

bool ProcessBlock(CNode* pfrom, CBlock* pblock)
{
    // Check for duplicate
    uint256 hash = pblock->GetHash();
    if (mapBlockIndex.count(hash))
        return error("ProcessBlock() : already have block %d %s", mapBlockIndex[hash]->nHeight, hash.ToString().substr(0,20).c_str());
    if (mapOrphanBlocks.count(hash))
        return error("ProcessBlock() : already have block (orphan) %s", hash.ToString().substr(0,20).c_str());

    // triangles: check proof-of-stake
    // Limited duplicity on stake: prevents block flood attack
    // Duplicate stake allowed only when there is orphan child block
    if (pblock->IsProofOfStake() && !GetBoolArg("-ignoredupstake", false) && setStakeSeen.count(pblock->GetProofOfStake()) && !mapOrphanBlocksByPrev.count(hash))
        return error("ProcessBlock() : duplicate proof-of-stake (%s, %d) for block %s", pblock->GetProofOfStake().first.ToString().c_str(), pblock->GetProofOfStake().second, hash.ToString().c_str());

    // Operational IBD state is never permission to skip a live proof-of-stake
    // block signature. Only a candidate height committed by the latest
    // hardened checkpoint uses the historical fast path.
    bool checkBlockSignature = true;
    const auto prevIt = mapBlockIndex.find(pblock->hashPrevBlock);
    if (prevIt != mapBlockIndex.end()) {
        const int candidateHeight = prevIt->second->nHeight + 1;
        checkBlockSignature = IsBlockSignatureRequiredAtHeight(candidateHeight);
    }
    if (!pblock->CheckBlock(true, true, checkBlockSignature))
    {
        printf("IBD-DIAG: CheckBlock FAILED for %s (PoS=%d, IBD=%d)\n",
            hash.ToString().substr(0,20).c_str(), pblock->IsProofOfStake(), IsInitialBlockDownload());
        return error("ProcessBlock() : CheckBlock FAILED");
    }

    // Anti-spam: reject blocks with insufficient difficulty to prevent memory flooding.
    // Use the most recent hardened checkpoint we know about; fall back to the chain tip.
    CBlockIndex* pcheckpoint = Checkpoints::GetLastCheckpoint(mapBlockIndex);
    if (!pcheckpoint)
        pcheckpoint = pindexBest;

    if (pcheckpoint && pblock->hashPrevBlock != hashBestChain)
    {
        int64_t deltaTime = pblock->GetBlockTime() - pcheckpoint->nTime;
        CBigNum bnNewBlock;
        bnNewBlock.SetCompact(pblock->nBits);
        CBigNum bnRequired;

        if (pblock->IsProofOfStake())
        {
            const CBlockIndex* pindexLastPos = GetLastBlockIndex(pcheckpoint, true);
            if (pindexLastPos)
                bnRequired.SetCompact(ComputeMinStake(pindexLastPos->nBits, deltaTime, pblock->nTime));
            // else: no PoS history yet (below block 9001), skip — AcceptBlock rejects PoS below MODIFIER_INTERVAL_SWITCH
        }
        else
        {
            const CBlockIndex* pindexLastPow = GetLastBlockIndex(pcheckpoint, false);
            if (pindexLastPow)
                bnRequired.SetCompact(ComputeMinWork(pindexLastPow->nBits, deltaTime));
        }

        // Anti-spam: reject blocks whose target exceeds the required minimum (i.e. blocks
        // with less difficulty than required for the elapsed time-since-checkpoint).
        // bnNewBlock is the candidate's compact-bits target; bnRequired is the minimum
        // target for the elapsed time. In Bitcoin/PoS, a LARGER target means EASIER
        // difficulty. So: bnNewBlock > bnRequired => block is easier than required =>
        // "too little proof-of-stake/work" => reject.
        //
        // The 2026-06-30 commit cbb189a inverted this to bnNewBlock < bnRequired which
        // rejected blocks that are HARDER than required (good blocks!) — verified by
        // DNS3 stalling at snapshot height 2,214,547 because every canonical post-snapshot
        // block was being rejected as "too little proof-of-stake". This restores the
        // correct comparison and keeps the soft Misbehaving(5) score from cbb189a.
        if (bnRequired != 0 && bnNewBlock > bnRequired)
        {
            // Anti-spam is a soft scoring signal, NOT a hard ban trigger. A single
            // violation should log + score modestly, not 24-hour-ban honest peers
            // (which is what happened during the 2026-06-23 DNS2 clearnet-fork
            // incident — `Misbehaving(100)` crossed the banscore threshold on the
            // FIRST block, instantly banning every honest peer feeding us fork blocks).
            if (pfrom)
                pfrom->Misbehaving(5);
            return error("ProcessBlock() : block with too little %s", pblock->IsProofOfStake()? "proof-of-stake" : "proof-of-work");
        }
    }

    // If don't already have its previous block, shunt it off to holding area until we get it
    if (!mapBlockIndex.count(pblock->hashPrevBlock))
    {
        if (fDebug)
            printf("ProcessBlock: ORPHAN BLOCK, prev=%s\n", pblock->hashPrevBlock.ToString().substr(0,20).c_str());
        std::unique_ptr<CBlock> pblock2 = std::make_unique<CBlock>(*pblock);
        // triangles: check proof-of-stake
        if (pblock2->IsProofOfStake())
        {
            // Limited duplicity on stake: prevents block flood attack
            // Duplicate stake allowed only when there is orphan child block
            if (!GetBoolArg("-ignoredupstake", false) && setStakeSeenOrphan.count(pblock2->GetProofOfStake()) && !mapOrphanBlocksByPrev.count(hash))
                return error("ProcessBlock() : duplicate proof-of-stake (%s, %d) for orphan block %s", pblock2->GetProofOfStake().first.ToString().c_str(), pblock2->GetProofOfStake().second, hash.ToString().c_str());
            else
                setStakeSeenOrphan.insert(pblock2->GetProofOfStake());
        }
        uint256 hashPrevOrphan = pblock2->hashPrevBlock;
        CBlock* pblock2raw = pblock2.get();
        mapOrphanBlocks.insert(make_pair(hash, std::move(pblock2)));
        mapOrphanBlocksByPrev.insert(make_pair(hashPrevOrphan, pblock2raw));
        dequeOrphanOrder.push_back(hash);  // track insertion order for FIFO eviction

        // Limit orphan blocks to prevent memory exhaustion.
        // Allow more orphans during IBD so out-of-order blocks from parallel
        // downloads don't get evicted and re-requested.
        unsigned int nMaxOrphans = IsInitialBlockDownload() ? MAX_ORPHAN_BLOCKS_IBD : MAX_ORPHAN_BLOCKS;
        LimitOrphanBlocks(nMaxOrphans);

        // Ask this guy to fill in what we're missing
        if (pfrom && pindexBest)
        {
            pfrom->PushGetBlocks(pindexBest, GetOrphanRoot(pblock2raw));
            // triangles: getblocks may not obtain the ancestor block rejected
            // earlier by duplicate-stake check so we ask for it again directly
            if (!IsInitialBlockDownload())
                pfrom->AskFor(CInv(MSG_BLOCK, WantedByOrphan(pblock2raw)));
        }
        return true;
    }

    // Store to disk
    if (!pblock->AcceptBlock())
        return error("ProcessBlock() : AcceptBlock FAILED");

    g_syncManager.BlockAccepted(hash);

    // Recursively process any orphan blocks that depended on this one
    vector<uint256> vWorkQueue;
    vWorkQueue.push_back(hash);
    for (unsigned int i = 0; i < vWorkQueue.size(); i++)
    {
        uint256 hashPrev = vWorkQueue[i];
    for (auto mi = mapOrphanBlocksByPrev.lower_bound(hashPrev);
         mi != mapOrphanBlocksByPrev.upper_bound(hashPrev);
         ++mi)
    {
        CBlock* pblockOrphan = mi->second;
            if (pblockOrphan->AcceptBlock())
            {
                vWorkQueue.push_back(pblockOrphan->GetHash());
                g_syncManager.BlockAccepted(pblockOrphan->GetHash());
            }
            mapOrphanBlocks.erase(pblockOrphan->GetHash());
            setStakeSeenOrphan.erase(pblockOrphan->GetProofOfStake());
        }
        mapOrphanBlocksByPrev.erase(hashPrev);
    }

    if (nBestHeight % 5000 == 0 || !IsInitialBlockDownload())
        printf("ProcessBlock: ACCEPTED block %d\n", nBestHeight);

    if (IsInitialBlockDownload())
    {
        const unsigned int nQueued =
            (g_syncManager.GetBestHeader() != 0) ? g_syncManager.QueueBlocksParallel() : 0;
        if (nQueued > 0 && fDebug)
            printf("IBD-DIAG: queued %u more blocks from header planner after accepting %s\n",
                nQueued, hash.ToString().substr(0,20).c_str());

        const unsigned int nPlannerDepth = g_syncManager.GetPlannerDepth();
        if (nPlannerDepth <= CSyncManager::HEADER_SYNC_LOW_WATER)
        {
            const unsigned int nRefilled = g_syncManager.RequestRefillAllPeers(
                g_syncManager.GetBestHeader(), CSyncManager::HEADER_SYNC_REFILL_MIN_INTERVAL_SECONDS,
                (nPlannerDepth == 0) ? "post-accept planner empty" : "post-accept planner low-water");
            if (nRefilled > 0 && fDebug)
                printf("IBD-DIAG: post-accept requested headers from %u peers at plannerDepth=%u after block %s\n",
                    nRefilled, nPlannerDepth, hash.ToString().substr(0,20).c_str());
        }
    }

    return true;
}

// triangles: sign block
bool CBlock::SignBlock(CWallet& wallet, int64_t nFees)
{
    // if we are trying to sign
    //    something except proof-of-stake block template
    if (!vtx[0].vout[0].IsEmpty())
        return false;

    // if we are trying to sign
    //    a complete proof-of-stake block
    if (IsProofOfStake())
        return true;

    static int64_t nLastCoinStakeSearchTime = GetAdjustedTime();  // only initialized at startup

    CKey key;
    CTransaction txCoinStake;
    int64_t nSearchTime = txCoinStake.nTime; // search to current time

    if (nSearchTime > nLastCoinStakeSearchTime)
    {
        if (wallet.CreateCoinStake(wallet, nBits, nSearchTime-nLastCoinStakeSearchTime, nFees, txCoinStake, key))
        {
            if (txCoinStake.nTime >= max(pindexBest->GetPastTimeLimit()+1, PastDrift(pindexBest->GetBlockTime(), pindexBest->nHeight + 1)))
            {
                // make sure coinstake would meet timestamp protocol
                //    as it would be the same as the block timestamp
                vtx[0].nTime = nTime = txCoinStake.nTime;
                nTime = max(pindexBest->GetPastTimeLimit()+1, GetMaxTransactionTime());
                nTime = max(GetBlockTime(), PastDrift(pindexBest->GetBlockTime(), pindexBest->nHeight + 1));

                // we have to make sure that we have no future timestamps in
                //    our transactions set
                for (vector<CTransaction>::iterator it = vtx.begin(); it != vtx.end();)
                    if (it->nTime > nTime) { it = vtx.erase(it); } else { ++it; }

                vtx.insert(vtx.begin() + 1, txCoinStake);
                hashMerkleRoot = BuildMerkleTree();

                // append a signature to our block
                return key.Sign(GetHash(), vchBlockSig);
            }
        }
        nLastCoinStakeSearchInterval = nSearchTime - nLastCoinStakeSearchTime;
        nLastCoinStakeSearchTime = nSearchTime;
    }

    //printf("Sign failed\n");
    return false;
}

// triangles: check block signature
bool CBlock::CheckBlockSignature() const
{
    if (IsProofOfWork())
        return vchBlockSig.empty();

    vector<valtype> vSolutions;
    TxnOutType whichType;

    const CTxOut& txout = vtx[1].vout[1];

    if (!Solver(txout.scriptPubKey, whichType, vSolutions))
        return false;

    if (whichType == TxnOutType::PubKey)
    {
        valtype& vchPubKey = vSolutions[0];
        CKey key;
        if (!key.SetPubKey(vchPubKey))
            return false;
        if (vchBlockSig.empty())
            return false;
        return key.Verify(GetHash(), vchBlockSig);
    }

    return false;
}

bool CheckDiskSpace(uint64_t nAdditionalBytes)
{
    uint64_t nFreeBytesAvailable = fs::space(GetDataDir()).available;

    // Check for nMinDiskSpace bytes (currently 50MB)
    if (nFreeBytesAvailable < nMinDiskSpace + nAdditionalBytes)
    {
        fShutdown = true;
        string strMessage = _("Warning: Disk space is low!");
        strMiscWarning = strMessage;
        printf("*** %s\n", strMessage.c_str());
        uiInterface.ThreadSafeMessageBox(strMessage, "Triangles", CClientUIInterface::OK | CClientUIInterface::ICON_EXCLAMATION | CClientUIInterface::MODAL);
        MarkShutdownFailure();
        StartShutdown();
        return false;
    }
    return true;
}

static fs::path BlockFilePath(unsigned int nFile)
{
    string strBlockFn = strprintf("blk%04u.dat", nFile);
    return GetDataDir() / strBlockFn;
}

FILE* OpenBlockFile(unsigned int nFile, unsigned int nBlockPos, const char* pszMode)
{
    if ((nFile < 1) || (nFile == (unsigned int) -1))
        return nullptr;
    FILE* file = fopen(BlockFilePath(nFile).string().c_str(), pszMode);
    if (!file)
        return nullptr;
    if (nBlockPos != 0 && !strchr(pszMode, 'a') && !strchr(pszMode, 'w'))
    {
        if (fseek(file, nBlockPos, SEEK_SET) != 0)
        {
            fclose(file);
            return nullptr;
        }
    }
    return file;
}

static unsigned int nCurrentBlockFile = 1;

FILE* AppendBlockFile(unsigned int& nFileRet)
{
    nFileRet = 0;
    while (true)
    {
        FILE* file = OpenBlockFile(nCurrentBlockFile, 0, "ab");
        if (!file)
            return nullptr;
        if (fseek(file, 0, SEEK_END) != 0)
            return nullptr;
        // FAT32 file size max 4GB, fseek and ftell max 2GB, so we must stay under 2GB
        if (ftell(file) < (long)(0x7F000000 - MAX_SIZE))
        {
            nFileRet = nCurrentBlockFile;
            return file;
        }
        fclose(file);
        nCurrentBlockFile++;
    }
}

bool LoadBlockIndex(bool fAllowNew)
{
    //CBigNum bnTrustedModulus;

    if (fTestNet)
    {
        pchMessageStart[0] = 0x6f;
        pchMessageStart[1] = 0x3e;
        pchMessageStart[2] = 0x04;
        pchMessageStart[3] = 0x13;

        bnProofOfStakeLimit = bnProofOfStakeLimitTestNet; // 0x00000fff PoS base target is fixed in testnet
        bnProofOfWorkLimit = bnProofOfWorkLimitTestNet; // 0x0000ffff PoW base target is fixed in testnet
        nStakeMinAge = 10 * 60; // test net min age is 10 min
        nStakeMaxAge = 30 * 60; // test net min age is 30 min
        nModifierInterval = 60; // test modifier interval is 1 minutes
        nCoinbaseMaturity = 10; // test maturity is 10 blocks
        nTargetSpacing = 1 * 60; // test block spacing is 1 minutes
    }

    //
    // Load block index
    //
    auto txdb_holder = MakeChainDB("cr+"); CTxDBBase& txdb = *txdb_holder;
    if (!txdb.LoadBlockIndex())
        return false;

    //
    // Init with genesis block
    //
    if (mapBlockIndex.empty())
    {
        if (!fAllowNew)
            return false;

		// Genesis block
			
        const char* pszTimestamp = "july 16 2014, I'm deh besht mang, I deeed et!";
        CTransaction txNew;
        txNew.nTime = nChainStartTime;
        txNew.vin.resize(1);
        txNew.vout.resize(1);
        txNew.vin[0].scriptSig = CScript() << 486604799 << CBigNum(9999) << vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
        txNew.vout[0].SetEmpty();
        CBlock block;
        block.vtx.push_back(txNew);
        block.hashPrevBlock = 0;
        block.hashMerkleRoot = block.BuildMerkleTree();
        block.nVersion = 1;
        block.nTime    = 1405500418;
        block.nBits    = bnProofOfWorkLimit.GetCompact();
        block.nNonce   = 43;





        if (false && (block.GetHash() != hashGenesisBlockOfficial)) {

		// This will figure out a valid hash and Nonce if you're
		// creating a different genesis block:
		    uint256 hashTarget = CBigNum().SetCompact(block.nBits).getuint256();
		    while (block.GetHash() > hashTarget)
		       {
		           ++block.nNonce;
		           if (block.nNonce == 0)
		           {
		               printf("NONCE WRAPPED, incrementing time");
		               ++block.nTime;
		           }
		       }
        }

        //// debug print
        block.print();
        printf("block.GetHash() == %s\n", block.GetHash().ToString().c_str());
        printf("block.hashMerkleRoot == %s\n", block.hashMerkleRoot.ToString().c_str());
        printf("block.nTime = %u \n", block.nTime);
        printf("block.nNonce = %u \n", block.nNonce);

        assert(block.hashMerkleRoot == uint256("0x27f77273afc4e7cca700b8564eed9a7cc7ee38e81189a8d57a98bc42f848d51e"));
        assert(block.GetHash() == (!fTestNet ? hashGenesisBlockOfficial : hashGenesisBlockTestNet));

        // Start new block file
        unsigned int nFile;
        unsigned int nBlockPos;
        if (!block.WriteToDisk(nFile, nBlockPos))
            return error("LoadBlockIndex() : writing genesis block to disk failed");
        if (!block.AddToBlockIndex(nFile, nBlockPos, 0))
            return error("LoadBlockIndex() : genesis block not accepted");

        // triangles: initialize synchronized checkpoint
        if (!Checkpoints::WriteSyncCheckpoint((!fTestNet ? hashGenesisBlockOfficial : hashGenesisBlockTestNet)))
            return error("LoadBlockIndex() : failed to init sync checkpoint");
    }

    string strPubKey = "";

    // if checkpoint master key changed must reset sync-checkpoint
    if (!txdb.ReadCheckpointPubKey(strPubKey) || strPubKey != CSyncCheckpoint::strMasterPubKey)
    {
        // write checkpoint master key to db
        txdb.TxnBegin();
        if (!txdb.WriteCheckpointPubKey(CSyncCheckpoint::strMasterPubKey))
            return error("LoadBlockIndex() : failed to write new checkpoint master key to db");
        if (!txdb.TxnCommit())
            return error("LoadBlockIndex() : failed to commit new checkpoint master key to db");
        if ((!fTestNet) && !Checkpoints::ResetSyncCheckpoint())
        {
            // For snapshot-sourced chains, the small initial block index may
            // not include any of the known sync checkpoints yet (snapshot only
            // includes ~1166 headers near tip). The sync checkpoint will be
            // set when the node syncs past a known checkpoint height.
            if (fLoadedFromSnapshot) {
                printf("LoadBlockIndex(): sync-checkpoint reset deferred (snapshot-sourced, no checkpoints in small index yet)\n");
            } else {
                return error("LoadBlockIndex() : failed to reset sync-checkpoint");
            }
        }
    }

    return true;
}



void PrintBlockTree()
{
    // pre-compute tree structure
    map<CBlockIndex*, vector<CBlockIndex*> > mapNext;
    for (const auto& [hash, pindex] : mapBlockIndex)
    {
        mapNext[pindex->pprev].push_back(pindex);
        // test
        //while (rand() % 3 == 0)
        //    mapNext[pindex->pprev].push_back(pindex);
    }

    vector<pair<int, CBlockIndex*> > vStack;
    vStack.push_back(make_pair(0, pindexGenesisBlock));

    int nPrevCol = 0;
    while (!vStack.empty())
    {
        int nCol = vStack.back().first;
        CBlockIndex* pindex = vStack.back().second;
        vStack.pop_back();

        // print split or gap
        if (nCol > nPrevCol)
        {
            for (int i = 0; i < nCol-1; i++)
                printf("| ");
            printf("|\\\n");
        }
        else if (nCol < nPrevCol)
        {
            for (int i = 0; i < nCol; i++)
                printf("| ");
            printf("|\n");
       }
        nPrevCol = nCol;

        // print columns
        for (int i = 0; i < nCol; i++)
            printf("| ");

        // print item
        CBlock block;
        block.ReadFromDisk(pindex);
        printf("%d (%u,%u) %s  %08x  %s  mint %7s  tx %" PRIszu "",
            pindex->nHeight,
            pindex->nFile,
            pindex->nBlockPos,
            block.GetHash().ToString().c_str(),
            block.nBits,
            DateTimeStrFormat("%x %H:%M:%S", block.GetBlockTime()).c_str(),
            FormatMoney(pindex->nMint).c_str(),
            block.vtx.size());

        PrintWallets(block);

        // put the main time-chain first
        vector<CBlockIndex*>& vNext = mapNext[pindex];
        for (unsigned int i = 0; i < vNext.size(); i++)
        {
            if (vNext[i]->pnext)
            {
                swap(vNext[0], vNext[i]);
                break;
            }
        }

        // iterate children
        for (unsigned int i = 0; i < vNext.size(); i++)
            vStack.push_back(make_pair(nCol+i, vNext[i]));
    }
}

bool LoadExternalBlockFile(FILE* fileIn)
{
    int64_t nStart = GetTimeMillis();

    // Get file size for progress reporting
    int64_t nFileSize = 0;
    fseek(fileIn, 0, SEEK_END);
    nFileSize = ftell(fileIn);
    fseek(fileIn, 0, SEEK_SET);

    int nLoaded = 0;
    int64_t nLastProgressReport = 0;
    {
        LOCK(cs_main);
        try {
            CAutoFile blkdat(fileIn, SER_DISK, CLIENT_VERSION);
            unsigned int nPos = 0;
            while (nPos != (unsigned int)-1 && blkdat.good() && !fRequestShutdown)
            {
                unsigned char pchData[65536];
                do {
                    fseek(blkdat, nPos, SEEK_SET);
                    int nRead = fread(pchData, 1, sizeof(pchData), blkdat);
                    if (nRead <= 8)
                    {
                        nPos = (unsigned int)-1;
                        break;
                    }
                    void* nFind = memchr(pchData, pchMessageStart[0], nRead+1-sizeof(pchMessageStart));
                    if (nFind)
                    {
                        if (memcmp(nFind, pchMessageStart, sizeof(pchMessageStart))==0)
                        {
                            nPos += ((unsigned char*)nFind - pchData) + sizeof(pchMessageStart);
                            break;
                        }
                        nPos += ((unsigned char*)nFind - pchData) + 1;
                    }
                    else
                        nPos += sizeof(pchData) - sizeof(pchMessageStart) + 1;
                } while(!fRequestShutdown);
                if (nPos == (unsigned int)-1)
                    break;
                fseek(blkdat, nPos, SEEK_SET);
                unsigned int nSize;
                blkdat >> nSize;
                if (nSize > 0 && nSize <= MAX_BLOCK_SIZE)
                {
                    CBlock block;
                    blkdat >> block;

                    // Quick check: skip blocks we already have in the index.
                    // This avoids full ProcessBlock overhead during resumed imports.
                    uint256 hash = block.GetHash();
                    if (mapBlockIndex.count(hash))
                    {
                        // Already indexed - skip silently
                    }
                    else if (ProcessBlock(nullptr,&block))
                        nLoaded++;
                    nPos += 4 + nSize;
                }

                // Report progress every 1000 blocks
                if (nLoaded - nLastProgressReport >= 1000)
                {
                    nLastProgressReport = nLoaded;
                    if (nFileSize > 0) {
                        int pct = (int)((int64_t)nPos * 100 / nFileSize);
                        printf("Importing blocks... %d blocks loaded (%d%%)\n", nLoaded, pct);
                        uiInterface.InitMessage(strprintf(_("Importing blocks... %d loaded (%d%%)"), nLoaded, pct));
                    } else {
                        printf("Importing blocks... %d blocks loaded\n", nLoaded);
                        uiInterface.InitMessage(strprintf(_("Importing blocks... %d loaded"), nLoaded));
                    }
                }
            }
        }
        catch (std::exception &e) {
            printf("%s() : Deserialize or I/O error caught during load\n",
                   __PRETTY_FUNCTION__);
        }
    }
    printf("Loaded %i blocks from external file in %" PRId64 "ms\n", nLoaded, GetTimeMillis() - nStart);
    return nLoaded > 0;
}


bool FastImportBlockFile()
{
    // Fast block import: reads blk0001.dat and builds the block index
    // directly without re-writing block data. LevelDB writes are batched
    // every 200K blocks for speed. Only used for trusted bootstrap data
    // (blocks below the hardcoded checkpoint).

    fs::path blkPath = GetDataDir() / "blk0001.dat";
    if (!fs::exists(blkPath))
        return false;

    printf("FastImportBlockFile: starting from %s\n", blkPath.string().c_str());
    int64_t nStart = GetTimeMillis();

    FILE* fileIn = fopen(blkPath.string().c_str(), "rb");
    if (!fileIn)
        return false;

    // Get file size for progress
    fseek(fileIn, 0, SEEK_END);
    int64_t nFileSize = ftell(fileIn);
    fseek(fileIn, 0, SEEK_SET);

    int nLoaded = 0;
    int64_t nLastProgressReport = 0;

    {
        LOCK(cs_main);
        CAutoFile blkdat(fileIn, SER_DISK, CLIENT_VERSION);

        auto txdb_holder = MakeChainDB(); CTxDBBase& txdb = *txdb_holder;
        txdb.TxnBegin();

        unsigned int nPos = 0;
        while (nPos != (unsigned int)-1 && blkdat.good() && !fRequestShutdown)
        {
            // Find message start bytes (same scan as LoadExternalBlockFile)
            unsigned char pchData[65536];
            do {
                fseek(blkdat, nPos, SEEK_SET);
                int nRead = fread(pchData, 1, sizeof(pchData), blkdat);
                if (nRead <= 8)
                {
                    nPos = (unsigned int)-1;
                    break;
                }
                void* nFind = memchr(pchData, pchMessageStart[0], nRead+1-sizeof(pchMessageStart));
                if (nFind)
                {
                    if (memcmp(nFind, pchMessageStart, sizeof(pchMessageStart))==0)
                    {
                        nPos += ((unsigned char*)nFind - pchData) + sizeof(pchMessageStart);
                        break;
                    }
                    nPos += ((unsigned char*)nFind - pchData) + 1;
                }
                else
                    nPos += sizeof(pchData) - sizeof(pchMessageStart) + 1;
            } while(!fRequestShutdown);

            if (nPos == (unsigned int)-1)
                break;

            fseek(blkdat, nPos, SEEK_SET);
            unsigned int nSize;
            blkdat >> nSize;

            if (nSize == 0 || nSize > MAX_BLOCK_SIZE)
            {
                nPos += 4 + nSize;
                continue;
            }

            // nBlockPos = file position where the block data starts
            // (after 4-byte message start + 4-byte size)
            unsigned int nBlockPos = nPos + 4;

            CBlock block;
            blkdat >> block;

            uint256 hash = block.GetHash();
            if (mapBlockIndex.count(hash))
            {
                nPos += 4 + nSize;
                continue; // already indexed
            }

            // Create CBlockIndex
            CBlockIndex* pindexNew = new CBlockIndex(1, nBlockPos, block);
            if (!pindexNew)
                break;

            // Link to previous block
            auto miPrev = mapBlockIndex.find(block.hashPrevBlock);
            if (miPrev != mapBlockIndex.end())
            {
                pindexNew->pprev = miPrev->second;
                pindexNew->nHeight = pindexNew->pprev->nHeight + 1;
            }

            // Chain trust
            pindexNew->nChainTrust = (pindexNew->pprev ? pindexNew->pprev->nChainTrust : 0) + pindexNew->GetBlockTrust();

            // Stake entropy bit
            pindexNew->SetStakeEntropyBit(block.GetStakeEntropyBit());

            // Stake modifier (minimal for blocks far below checkpoint)
            int nCheckpointHeight = Checkpoints::GetTotalBlocksEstimate();
            if (pindexNew->nHeight >= nCheckpointHeight - 1000)
            {
                uint64_t nStakeModifier = 0;
                bool fGeneratedStakeModifier = false;
                ComputeNextStakeModifier(pindexNew->pprev, nStakeModifier, fGeneratedStakeModifier);
                pindexNew->SetStakeModifier(nStakeModifier, fGeneratedStakeModifier);
            }
            else
            {
                pindexNew->SetStakeModifier(0, pindexNew->nHeight == 0);
            }
            pindexNew->nStakeModifierChecksum = GetStakeModifierChecksum(pindexNew);

            // PoS stake seen set
            if (pindexNew->IsProofOfStake())
                setStakeSeen.insert(make_pair(pindexNew->prevoutStake, pindexNew->nStakeTime));

            // Insert into mapBlockIndex
            auto mi = mapBlockIndex.insert(make_pair(hash, pindexNew)).first;
            pindexNew->phashBlock = &mi->first;

            // Link pnext for previous block
            if (pindexNew->pprev)
                pindexNew->pprev->pnext = pindexNew;

            // NOTE: tx-index, UTXO-set and money-supply application are
            // DEFERRED to a second pass over the active (best-trust) chain
            // only — see the pass after this loop. Applying them here, for
            // every block read from the file (which permanently retains
            // ORPHANED side-chain blocks), wrote those orphans' outputs into
            // the UTXO set as phantom coins and over-counted nMoneySupply.
            // That was the root cause of UTXO-set / supply inflation on every
            // reindex. Here we only build the block index for all blocks so
            // best-chain selection by trust still works.
            txdb.WriteBlockIndex(CDiskBlockIndex(pindexNew));

            // Update best chain
            if (pindexNew->nChainTrust > nBestChainTrust)
            {
                hashBestChain = hash;
                pindexBest = pindexNew;
                pblockindexFBBHLast = nullptr;
                nBestHeight = pindexNew->nHeight;
                nBestChainTrust = pindexNew->nChainTrust;
                nTimeBestReceived = GetTime();
            }

            // Set genesis block
            if (pindexGenesisBlock == nullptr && pindexNew->nHeight == 0)
                pindexGenesisBlock = pindexNew;

            nLoaded++;
            nPos += 4 + nSize;

            // Batch commit every 200K blocks for LevelDB efficiency
            if (nLoaded % 200000 == 0)
            {
                txdb.WriteHashBestChain(hashBestChain);
                txdb.TxnCommit();
                txdb.TxnBegin();
            }

            // Report progress every 5000 blocks to keep GUI responsive.
            // AppInit2 runs on the GUI thread, so uiInterface.InitMessage
            // triggers processEvents() which prevents the window from freezing.
            if (nLoaded % 5000 == 0)
            {
                int pct = (nFileSize > 0) ? (int)((int64_t)nPos * 100 / nFileSize) : 0;
                printf("FastImport: %d blocks indexed (%d%%)\n", nLoaded, pct);
                uiInterface.InitMessage(strprintf(_("Importing blocks... %d indexed (%d%%)"), nLoaded, pct));
            }
        }

        // ---- Pass 2: apply tx-index, UTXO set and money supply along the
        // ACTIVE (best-trust) chain ONLY. The file-order pass above indexed
        // every block including orphaned side-chain blocks; replaying only
        // the main chain here keeps the UTXO set and money supply exactly in
        // consensus and prevents orphan outputs becoming phantom coins. ----
        if (pindexBest)
        {
            std::vector<CBlockIndex*> vMain;
            for (CBlockIndex* p = pindexBest; p; p = p->pprev)
                vMain.push_back(p);
            std::reverse(vMain.begin(), vMain.end());
            printf("FastImportBlockFile: applying UTXO/supply along %d main-chain blocks...\n", (int)vMain.size());
            uiInterface.InitMessage(_("Building UTXO set (main chain)..."));

            int64_t nRunningSupply = 0;
            int nApplied = 0;
            for (CBlockIndex* pindex : vMain)
            {
                // Genesis (height 0) is a hardcoded special block that is not
                // re-read from disk this way; it contributes nothing to supply
                // and the genesis-walk audit skips it identically. Carry the
                // running supply (0) forward and move on.
                if (pindex->nHeight == 0)
                {
                    pindex->nMint = 0;
                    pindex->nMoneySupply = nRunningSupply; // still 0 here
                    txdb.WriteBlockIndex(CDiskBlockIndex(pindex));
                    continue;
                }

                CBlock blockMain;
                if (!blockMain.ReadFromDisk(pindex))
                    return error("FastImportBlockFile: ReadFromDisk failed at height %d", pindex->nHeight);

                int64_t nBlockValueIn = 0;
                int64_t nBlockValueOut = 0;
                unsigned int nTxPos2 = pindex->nBlockPos + ::GetSerializeSize(CBlock(), SER_DISK, CLIENT_VERSION)
                                    - (2 * GetSizeOfCompactSize(0)) + GetSizeOfCompactSize(blockMain.vtx.size());
                for (const CTransaction& tx : blockMain.vtx)
                {
                    uint256 hashTx = tx.GetHash();
                    CDiskTxPos posThisTx(1, pindex->nBlockPos, nTxPos2);
                    txdb.UpdateTxIndex(hashTx, CTxIndex(posThisTx, tx.vout.size()));
                    nTxPos2 += ::GetSerializeSize(tx, SER_DISK, CLIENT_VERSION);

                    nBlockValueOut += tx.GetValueOut();
                    if (!tx.IsCoinBase())
                    {
                        for (const CTxIn& txin : tx.vin)
                        {
                            CUtxoEntry uprev;
                            if (txdb.ReadUtxo(txin.prevout.hash, txin.prevout.n, uprev))
                                nBlockValueIn += uprev.nValue;
                            txdb.EraseUtxo(txin.prevout.hash, txin.prevout.n);
                        }
                    }
                    for (unsigned int k = 0; k < tx.vout.size(); k++)
                    {
                        if (tx.vout[k].IsEmpty())
                            continue;
                        CUtxoEntry utxo;
                        utxo.nValue = tx.vout[k].nValue;
                        utxo.nHeight = pindex->nHeight;
                        utxo.scriptPubKey = tx.vout[k].scriptPubKey;
                        utxo.fCoinBase = tx.IsCoinBase();
                        utxo.fCoinStake = tx.IsCoinStake();
                        utxo.nTxTime = tx.nTime;
                        txdb.WriteUtxo(hashTx, k, utxo);
                    }
                }

                pindex->nMint = nBlockValueOut - nBlockValueIn;
                nRunningSupply += (nBlockValueOut - nBlockValueIn);
                pindex->nMoneySupply = nRunningSupply;
                txdb.WriteBlockIndex(CDiskBlockIndex(pindex));

                if (++nApplied % 200000 == 0) { txdb.TxnCommit(); txdb.TxnBegin(); }
                if (nApplied % 5000 == 0)
                {
                    int pct2 = (int)((int64_t)nApplied * 100 / (vMain.empty() ? 1 : vMain.size()));
                    printf("FastImport UTXO apply: %d/%d main-chain blocks (%d%%)\n", nApplied, (int)vMain.size(), pct2);
                    uiInterface.InitMessage(strprintf(_("Building UTXO set... %d%%"), pct2));
                }
            }
        }

        // Final commit
        if (pindexBest)
        {
            txdb.WriteHashBestChain(hashBestChain);

            // Write sync checkpoint
            Checkpoints::WriteSyncCheckpoint(hashBestChain);
        }
        txdb.TxnCommit();
    }

    nTransactionsUpdated++;
    printf("FastImportBlockFile: indexed %d blocks in %" PRId64 "ms\n", nLoaded, GetTimeMillis() - nStart);
    return nLoaded > 0;
}

string GetWarnings(string strFor)
{
    string strStatusBar;
    string strRPC;

    // triangles: if detected invalid checkpoint enter safe mode
    if (Checkpoints::hashInvalidCheckpoint != 0)
        strStatusBar = strRPC = _("WARNING: Invalid checkpoint found! Displayed transactions may not be correct! You may need to upgrade, or notify developers.");

    if (strFor == "statusbar")
        return strStatusBar;
    else if (strFor == "rpc")
        return strRPC;
    assert(!"GetWarnings() : invalid parameter");
    return "error";
}








//////////////////////////////////////////////////////////////////////////////
//
// Messages
//


bool static AlreadyHave(CTxDBBase& txdb, const CInv& inv)
{
    switch (inv.type)
    {
    case MSG_TX:
        {
        bool txInMap = false;
            {
            LOCK(mempool.cs);
            txInMap = (mempool.exists(inv.hash));
            }
        return txInMap ||
               mapOrphanTransactions.count(inv.hash) ||
               txdb.ContainsTx(inv.hash);
        }

    case MSG_BLOCK:
    case MSG_CMPCT_BLOCK:
        return mapBlockIndex.count(inv.hash) ||
               mapOrphanBlocks.count(inv.hash);
    }
    // Don't know what it is, just say we already got one
    return true;
}




// The message start string is designed to be unlikely to occur in normal data.
// The characters are rarely used upper ASCII, not valid as UTF-8, and produce
// a large 4-byte int at any alignment.
unsigned char pchMessageStart[4] = { 0x70, 0x35, 0x22, 0x05 };

bool static ProcessMessage(CNode* pfrom, string strCommand, CDataStream& vRecv)
{
    RandAddSeedPerfmon();
    if (fDebug)
        printf("received: %s (%" PRIszu " bytes)\n", strCommand.c_str(), vRecv.size());
    if (mapArgs.count("-dropmessagestest") && GetRand(atoi(mapArgs["-dropmessagestest"])) == 0)
    {
        printf("dropmessagestest DROPPING RECV MESSAGE\n");
        return true;
    }

    if (strCommand == "version")
    {
        // Each connection can only send one version message
        if (pfrom->nVersion != 0)
        {
            pfrom->Misbehaving(1);
            return false;
        }

        int64_t nTime;
        CAddress addrMe;
        CAddress addrFrom;
        uint64_t nNonce = 1;
        //uint64_t verification_token = 0;
        vRecv >> pfrom->nVersion >> pfrom->nServices >> nTime >> addrMe;
        if (pfrom->nVersion < MIN_PROTO_VERSION)
        {
            // Since February 20, 2012, the protocol is initiated at version 209,
            // and earlier versions are no longer supported
            printf("partner %s using obsolete version %i; disconnecting\n", pfrom->addr.ToString().c_str(), pfrom->nVersion);
            pfrom->fDisconnect = true;
            return false;
        }

        if (pfrom->nVersion == 10300)
            pfrom->nVersion = 300;
        if (!vRecv.empty())
            vRecv >> addrFrom >> nNonce;
        if (!vRecv.empty()) {
            vRecv >> pfrom->strSubVer;
            //pfrom->cleanSubVer = SanitizeString(pfrom->strSubVer);
        }
        if (!vRecv.empty())
            vRecv >> pfrom->nStartingHeight;

        if (pfrom->fInbound && addrMe.IsRoutable())
        {
            pfrom->addrLocal = addrMe;
            SeenLocal(addrMe);
        }

        // Disconnect if we connected to ourself
        if (nNonce == nLocalHostNonce && nNonce > 1)
        {
            printf("connected to self at %s, disconnecting\n", pfrom->addr.ToString().c_str());
            pfrom->fDisconnect = true;
            return true;
        }

        // triangles: record my external IP reported by peer
        if (addrFrom.IsRoutable() && addrMe.IsRoutable())
            addrSeenByPeer = addrMe;

        // Be shy and don't send version until we hear
        if (pfrom->fInbound)
            pfrom->PushVersion();

        pfrom->fClient = !(pfrom->nServices & NODE_NETWORK);

        if (GetBoolArg("-synctime", true))
            AddTimeData(pfrom->addr, nTime);

        // Change version
        pfrom->PushMessage("verack");
        pfrom->ssSend.SetVersion(min(pfrom->nVersion, PROTOCOL_VERSION));

        if (!pfrom->fInbound)
        {
            // Advertise our address
            if (!fNoListen && !IsInitialBlockDownload())
            {
                CAddress addr = GetLocalAddress(&pfrom->addr);
                if (addr.IsRoutable())
                    pfrom->PushAddress(addr);
            }

            // Always request addresses — critical for Tor-only small networks
            {
                pfrom->PushMessage("getaddr");
                pfrom->fGetAddr = true;
            }
            addrman.Good(pfrom->addr);
        } else {
            if (((CNetAddr)pfrom->addr) == (CNetAddr)addrFrom)
            {
                addrman.Add(addrFrom, addrFrom);
                addrman.Good(addrFrom);
            }
            // Also request addresses from inbound peers (small network optimization)
            if (!pfrom->fGetAddr) {
                pfrom->PushMessage("getaddr");
                pfrom->fGetAddr = true;
            }
        }

        // Ask connected nodes for block updates.
        // During IBD, request blocks from every valid peer to maximize download
        // parallelism. Multiple peers sending overlapping inv ranges is harmless
        // (AlreadyHave filters duplicates) but ensures we discover and download
        // blocks from the fastest available source.
        // NOTE: nStartingHeight from version messages is unverified. Peers can
        // claim any height. During IBD we always ask all eligible peers rather
        // than filtering on a claim that may be wrong (a stunted node could be
        // reporting the full chain height while only serving the tail of its
        // own fork). Use nBestKnownHeight (updated from actual block responses)
        // for peer capability assessment instead.
        static int nAskedForBlocks = 0;
        bool fIBD = IsInitialBlockDownload();
        // During IBD: ask every non-client peer unconditionally to maximise
        // download sources. Post-IBD: use traditional height-check logic.
        // During IBD we need to ask EVERY peer for blocks, including OneShot peers
        // (those added via -addnode= and the hardcoded onion/i2p seed list). The previous
        // `!pfrom->fOneShot` clause prevents getblocks/getheaders from being sent to these
        // peers, which is exactly what fresh-from-genesis wallets need. Without this, a
        // clean datadir syncs the first ~2000-4000 headers from one peer via the
        // control-loop getheaders planner, then stalls because no version-handler
        // getblocks was ever issued to fan out block requests.
        bool fShouldAsk = !pfrom->fClient &&
            (fIBD ||
             pfrom->nStartingHeight > (nBestHeight - 144) ||
             pfrom->nStartingHeight > nBestHeight) &&
            (pfrom->nVersion < NOBLKS_VERSION_START ||
             pfrom->nVersion >= NOBLKS_VERSION_END) &&
             (fIBD || nAskedForBlocks < 1 || vNodes.size() <= 1 || pfrom->nStartingHeight > nBestHeight);
        printf("IBD-DIAG: version handler: peer=%s height=%d ourHeight=%d fClient=%d fOneShot=%d shouldAsk=%d nAskedForBlocks=%d IBD=%d\n",
            pfrom->addr.ToString().c_str(), pfrom->nStartingHeight, nBestHeight,
            pfrom->fClient, pfrom->fOneShot, fShouldAsk, nAskedForBlocks, fIBD);
        if (fShouldAsk)
        {
            nAskedForBlocks++;
            pfrom->PushGetBlocks(pindexBest, uint256(0));
            // During IBD, also send getheaders to scout the chain structure.
            // Headers are ~80 bytes each (vs full blocks at ~1-2KB for PoS),
            // so we learn about future blocks much faster. The headers handler
            // will AskFor each unknown block, pre-populating the download queue.
            if (fIBD)
                g_syncManager.RequestRefill(pfrom, g_syncManager.GetBestHeader(), 0, "version bootstrap");
            printf("IBD-DIAG: sent getblocks%s from height %d to peer %s\n",
                fIBD ? "+getheaders" : "", nBestHeight, pfrom->addr.ToString().c_str());
        }

        // Sync checkpoint relay disabled (master key removed in V5 fork).
        // Relaying stale checkpoints causes IBD nodes to request far-future blocks.

        pfrom->fSuccessfullyConnected = true;

        // Request header-based block announcements and compact block relay
        pfrom->PushMessage("sendheaders");
        pfrom->PushMessage("sendcmpct");

        // If this is an onion peer and we have a pending resolve, request their wallet address
        {
            std::string peerAddr = pfrom->addr.ToStringIP();
            if (peerAddr.find(".onion") != std::string::npos)
            {
                CTorV3Manager* torMgr = CTorV3Manager::GetInstance();
                if (torMgr)
                    torMgr->RequestWalletAddress(pfrom);
            }
        }

        printf("receive version message: version %d, blocks=%d, us=%s, them=%s, peer=%s\n", pfrom->nVersion, pfrom->nStartingHeight, addrMe.ToString().c_str(), addrFrom.ToString().c_str(), pfrom->addr.ToString().c_str());

        cPeerBlockCounts.input(pfrom->nStartingHeight);

    }


    else if (pfrom->nVersion == 0)
    {
        // Must have a version message before anything else
        pfrom->Misbehaving(1);
        return false;
    }


    else if (strCommand == "verack")
    {
        pfrom->SetRecvVersion(min(pfrom->nVersion, PROTOCOL_VERSION));
    }


    else if (strCommand == "sendheaders")
    {
        // Peer prefers block announcements via headers instead of inv.
        // When we have a new block, we'll send a "headers" message rather
        // than waiting for the inv->getdata round-trip, saving ~2-4s on Tor.
        pfrom->fPreferHeaders = true;
    }


    else if (strCommand == "sendcmpct")
    {
        // Peer supports BIP152 compact block relay.
        // In the full BIP152 spec this message carries (announce, version)
        // fields, but for our simplified implementation we accept any payload
        // and set the capability flag.  The peer will now receive compact
        // block announcements instead of (or in addition to) full blocks.
        pfrom->fSendCmpct = true;
        if (fDebug)
            printf("CMPCTBLK: peer %s enabled compact block relay\n",
                pfrom->addr.ToString().c_str());
    }


    else if (strCommand == "addr")
    {
        vector<CAddress> vAddr;
        vRecv >> vAddr;

        // Don't want addr from older versions unless seeding
        if (pfrom->nVersion < CADDR_TIME_VERSION && addrman.size() > 1000)
            return true;
        if (vAddr.size() > 1000)
        {
            pfrom->Misbehaving(20);
            return error("message addr size() = %" PRIszu "", vAddr.size());
        }

        // Store the new addresses
        vector<CAddress> vAddrOk;
        int64_t nNow = GetAdjustedTime();
        int64_t nSince = nNow - 10 * 60;
        for (CAddress& addr : vAddr)
        {
            if (fShutdown)
                return true;
            if (addr.nTime <= 100000000 || addr.nTime > nNow + 10 * 60)
                addr.nTime = nNow - 5 * 24 * 60 * 60;
            pfrom->AddAddressKnown(addr);
            bool fReachable = IsReachable(addr);
            if (addr.nTime > nSince && !pfrom->fGetAddr && vAddr.size() <= 10 && addr.IsRoutable())
            {
                // Relay to a limited number of other nodes
                {
                    LOCK(cs_vNodes);
                    // Use deterministic randomness to send to the same nodes for 24 hours
                    // at a time so the setAddrKnowns of the chosen nodes prevent repeats
                    static uint256 hashSalt;
                    if (hashSalt == 0)
                        hashSalt = GetRandHash();
                    uint64_t hashAddr = addr.GetHash();
                    uint256 hashRand = hashSalt ^ (hashAddr<<32) ^ ((GetTime()+hashAddr)/(24*60*60));
                    hashRand = Hash(BEGIN(hashRand), END(hashRand));
                    multimap<uint256, CNode*> mapMix;
                    for (CNode* pnode : vNodes)
                    {
                        if (pnode->nVersion < CADDR_TIME_VERSION)
                            continue;
                        unsigned int nPointer;
                        memcpy(&nPointer, &pnode, sizeof(nPointer));
                        uint256 hashKey = hashRand ^ nPointer;
                        hashKey = Hash(BEGIN(hashKey), END(hashKey));
                        mapMix.insert(make_pair(hashKey, pnode));
                    }
                    // Small network: relay to more peers so addresses propagate quickly
                    int nRelayNodes = fReachable ? (int)mapMix.size() : 1;
                    for (auto mi = mapMix.begin(); mi != mapMix.end() && nRelayNodes-- > 0; ++mi)
                        mi->second->PushAddress(addr);
                }
            }
            // Do not store addresses outside our network
            if (fReachable)
                vAddrOk.push_back(addr);
        }
        addrman.Add(vAddrOk, pfrom->addr, 2 * 60 * 60);
        if (vAddr.size() < 1000)
            pfrom->fGetAddr = false;
        if (pfrom->fOneShot)
            pfrom->fDisconnect = true;
    }

    else if (strCommand == "inv")
    {
        vector<CInv> vInv;
        vRecv >> vInv;
        if (vInv.size() > MAX_INV_SZ)
        {
            pfrom->Misbehaving(20);
            return error("message inv size() = %" PRIszu "", vInv.size());
        }

        // find last block in inv vector
        unsigned int nLastBlock = (unsigned int)(-1);
        int nBlockInv = 0, nTxInv = 0;
        for (unsigned int nInv = 0; nInv < vInv.size(); nInv++) {
            if (vInv[nInv].type == MSG_BLOCK) nBlockInv++;
            else nTxInv++;
            if (vInv[vInv.size() - 1 - nInv].type == MSG_BLOCK && nLastBlock == (unsigned int)(-1)) {
                nLastBlock = vInv.size() - 1 - nInv;
            }
        }
        printf("IBD-DIAG: inv received: %d blocks, %d tx from %s (our height=%d)\n",
            nBlockInv, nTxInv, pfrom->addr.ToString().c_str(), nBestHeight);

        auto txdb_holder = MakeChainDB("r"); CTxDBBase& txdb = *txdb_holder;
        int nNew = 0, nAlready = 0, nAboveBest = 0;
        int nFirstInvHeight = -1, nLastInvHeight = -1;
        for (unsigned int nInv = 0; nInv < vInv.size(); nInv++)
        {
            const CInv &inv = vInv[nInv];

            if (fShutdown)
                return true;
            pfrom->AddInventoryKnown(inv);

            bool fAlreadyHave = AlreadyHave(txdb, inv);
            if (inv.type == MSG_BLOCK) {
                if (fAlreadyHave) {
                    nAlready++;
                    if (auto mi = mapBlockIndex.find(inv.hash); mi != mapBlockIndex.end()) {
                        int h = mi->second->nHeight;
                        if (nFirstInvHeight == -1) nFirstInvHeight = h;
                        nLastInvHeight = h;
                        if (h > pfrom->nBestKnownHeight)
                            pfrom->nBestKnownHeight = h;
                        if (h > nBestHeight) nAboveBest++;
                    }
                } else {
                    nNew++;
                }
            }

            if (!fAlreadyHave)
                pfrom->AskFor(inv);
            else if (inv.type == MSG_BLOCK && mapOrphanBlocks.count(inv.hash)) {
                pfrom->PushGetBlocks(pindexBest, GetOrphanRoot(mapOrphanBlocks[inv.hash].get()));
            } else if (nInv == nLastBlock) {
                // Continuation: walk forward from the last inv block.
                // Don't jump to pindexBest — its CBlockLocator exponential
                // spacing can map back to the same old match point, looping.
                // Walking from the last inv block progresses linearly through
                // the "already have" zone until we reach new blocks.
                int nInvH = mapBlockIndex[inv.hash]->nHeight;
                if (nInvH > nHighestInvWalk) {
                    nHighestInvWalk = nInvH;
                    hashHighestInvWalk = inv.hash;
                }
                pfrom->pindexLastGetBlocksBegin = nullptr; // reset dedup
                pfrom->PushGetBlocks(mapBlockIndex[inv.hash], uint256(0));
                printf("SYNC-DIAG: inv walk-forward from %d (best=%d, walk=%d)\n",
                    nInvH, nBestHeight, nHighestInvWalk);
            }

            Inventory(inv.hash);
        }
        if (nBlockInv > 0) {
            printf("SYNC-DIAG: inv result: %d new, %d already have (%d above best=%d), range=%d..%d\n",
                nNew, nAlready, nAboveBest, nBestHeight, nFirstInvHeight, nLastInvHeight);
            if (nNew > 0 && nAlready > 0)
                printf("SYNC-DIAG: *** FORK POINT CROSSED *** - downloading %d new blocks from canonical chain\n", nNew);
        }
    }


    else if (strCommand == "getdata")
    {
        vector<CInv> vInv;
        vRecv >> vInv;
        if (vInv.size() > MAX_INV_SZ)
        {
            pfrom->Misbehaving(20);
            return error("message getdata size() = %" PRIszu "", vInv.size());
        }

        if (fDebugNet || (vInv.size() != 1))
            printf("received getdata (%" PRIszu " invsz)\n", vInv.size());

        for (const CInv& inv : vInv)
        {
            if (fShutdown)
                return true;
            if (fDebugNet || (vInv.size() == 1))
                printf("received getdata for: %s\n", inv.ToString().c_str());

            if (inv.type == MSG_BLOCK || inv.type == MSG_CMPCT_BLOCK)
            {
                // Send block from disk
                auto mi = mapBlockIndex.find(inv.hash);
                if (mi != mapBlockIndex.end())
                {
                    CBlock block;
                    block.ReadFromDisk(mi->second);

                    // BIP152: if the peer has negotiated compact block relay
                    // (fSendCmpct) and explicitly requested via MSG_CMPCT_BLOCK,
                    // respond with a compact block instead of a full block.
                    // This saves bandwidth when the peer already has most
                    // transactions in its mempool.
                    if (inv.type == MSG_CMPCT_BLOCK && pfrom->fSendCmpct)
                    {
                        SendCompactBlock(pfrom, block);
                    }
                    else
                    {
                        pfrom->PushMessage("block", block);
                    }

                    // Trigger them to send a getblocks request for the next batch of inventory
                    if (inv.hash == pfrom->hashContinue)
                    {
                        // Send the best block hash to trigger the next getblocks.
                        // Original code sent the last PoW block, but since PoW ended
                        // at block 9000, that always sent an ancient block causing
                        // thousands of redundant round-trips through known blocks.
                        vector<CInv> vInv;
                        vInv.push_back(CInv(MSG_BLOCK, hashBestChain));
                        pfrom->PushMessage("inv", vInv);
                        pfrom->hashContinue = 0;
                    }
                }
            }
            else if (inv.IsKnownType())
            {
                // Send stream from relay memory
                bool pushed = false;
                {
                    LOCK(cs_mapRelay);
                    if (auto mi = mapRelay.find(inv); mi != mapRelay.end()) {
                        pfrom->PushMessage(inv.GetCommand(), mi->second);
                        pushed = true;
                    }
                }
                if (!pushed && inv.type == MSG_TX) {
                    LOCK(mempool.cs);
                    if (mempool.exists(inv.hash)) {
                        CTransaction tx = mempool.lookup(inv.hash);
                        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
                        ss.reserve(1000);
                        ss << tx;
                        pfrom->PushMessage("tx", ss);
                    }
                }
            }

            // Track requests for our stuff
            Inventory(inv.hash);
        }
    }


    else if (strCommand == "getblocks")
    {
        CBlockLocator locator;
        uint256 hashStop;
        vRecv >> locator >> hashStop;

        // Find the last block the caller has in the main chain
        CBlockIndex* pindex = locator.GetBlockIndex();

        // Detect incompatible fork: peer sent a locator with entries but
        // GetBlockIndex() fell through to genesis (no locator hash matched
        // our main chain). If the peer's tip isn't our genesis,
        // they're on a completely different fork.
        //
        // triangles fix: instead of banning or disconnecting, always respond
        // with our main chain blocks so a fork node can learn the canonical
        // chain and reorganize. The fork node's client will automatically
        // reorg when it receives blocks that form a longer or higher-work chain.
        if (!locator.IsNull() && pindex == pindexGenesisBlock &&
            pindexGenesisBlock && locator.GetTipHash() != pindexGenesisBlock->GetBlockHash())
        {
            pfrom->nIncompatibleGetblocks++;
            // triangles: after many failed attempts, reset — the peer may now be
            // on the correct chain and we don't want to ban a node that's just
            // learning about the main chain from us.
            if (pfrom->nIncompatibleGetblocks > 10)
                pfrom->nIncompatibleGetblocks = 0;
            // triangles: NO return/ban here — fall through and serve main chain
            // blocks so the forking peer can reorg to our chain.
            printf("WARNING: peer %s getblocks locator has no common blocks — serving main chain from genesis (counter=%d, will reset after 10)\\n",
                pfrom->addr.ToString().c_str(), pfrom->nIncompatibleGetblocks);
            pindex = pindexGenesisBlock;
        }
        else if (pindex && pindex != pindexGenesisBlock)
        {
            // Peer matched a non-genesis block — they share our chain
            pfrom->nIncompatibleGetblocks = 0;
        }

        // Send the rest of the chain
        if (pindex)
            pindex = pindex->pnext;
        // Send larger batches when the requester is far behind (syncing).
        // The original check used our own IBD state, but we're the seed node
        // (fully synced), so it always returned 500. Check how far behind
        // the requester is instead.
        int nLimit = (pindex && pindexBest && pindexBest->nHeight - pindex->nHeight > 1000) ? 10000 : 500;
        printf("IBD-DIAG: getblocks request from peer %s: start=%d stop=%s limit=%d\n",
            pfrom->addr.ToString().c_str(), (pindex ? pindex->nHeight : -1),
            hashStop.ToString().substr(0,20).c_str(), nLimit);
        for (; pindex; pindex = pindex->pnext)
        {
            if (pindex->GetBlockHash() == hashStop)
            {
                printf("  getblocks stopping at %d %s\n", pindex->nHeight, pindex->GetBlockHash().ToString().substr(0,20).c_str());
                // triangles: tell downloading node about the latest block if it's
                // without risk being rejected due to stake connection check
                if (hashStop != hashBestChain && pindex->GetBlockTime() + nStakeMinAge > pindexBest->GetBlockTime())
                    pfrom->PushInventory(CInv(MSG_BLOCK, hashBestChain));
                break;
            }
            pfrom->PushInventory(CInv(MSG_BLOCK, pindex->GetBlockHash()));
            if (--nLimit <= 0)
            {
                // When this block is requested, we'll send an inv that'll make them
                // getblocks the next batch of inventory.
                printf("  getblocks stopping at limit %d %s\n", pindex->nHeight, pindex->GetBlockHash().ToString().substr(0,20).c_str());
                pfrom->hashContinue = pindex->GetBlockHash();
                break;
            }
        }
    }
    else if (strCommand == "getheaders")
    {
        CBlockLocator locator;
        uint256 hashStop;
        vRecv >> locator >> hashStop;

        CBlockIndex* pindex = nullptr;
        if (locator.IsNull())
        {
            // If locator is null, return the hashStop block
            auto mi = mapBlockIndex.find(hashStop);
            if (mi == mapBlockIndex.end())
                return true;
            pindex = mi->second;
        }
        else
        {
            // Find the last block the caller has in the main chain
            pindex = locator.GetBlockIndex();

            // triangles fix: handle broken pnext chain.
            // GetBlockIndex() returns pindexGenesisBlock when no locator
            // hash matches our main chain (peer is on a different fork or
            // a stale local state). pindexGenesisBlock->pnext is always
            // null, which would cause the for-loop below to send ZERO
            // headers, leaving the peer stuck (logged as "getheaders -1").
            //
            // Mirror the getblocks handler: if the locator matches nothing
            // on our main chain, serve our headers from genesis so the peer
            // can discover the canonical chain. Then fall back to a tip-
            // backwards walk if pnext is null for any other reason (this
            // happens when LoadBlockIndex() didn't fully heal pnext links,
            // or the chain was bootstrapped from a snapshot).
            //
            // fork-peer getheaders recovery (fix/consensus-convergence).
            //
            // A forked peer calls getheaders with a locator containing the
            // highest blocks it knows. If none of those hashes are in our
            // main chain, locator.GetBlockIndex() returns pindexGenesisBlock
            // and the for-loop below would send zero headers (the peer
            // already has genesis), leaving the forked peer stuck.
            //
            // Recovery rule:
            //   - If the locator contains pindexLastHardenedCheckpoint,
            //     serve headers starting after the checkpoint — the peer
            //     already has the checkpoint and needs canonical history
            //     forward.
            //   - Otherwise, serve from the last common ancestor (if any)
            //     of the locator against our main chain, falling back to
            //     pindexGenesisBlock so the peer can walk forward from
            //     scratch.
            //
            // We never re-anchor at pindexLastHardenedCheckpoint without
            // confirming the peer already knows it; otherwise we'd hand
            // them a header whose parent they don't have, which is the
            // inverse of the recovery path we want.
            if (!locator.IsNull() && pindex == pindexGenesisBlock &&
                pindexGenesisBlock && locator.GetTipHash() != pindexGenesisBlock->GetBlockHash())
            {
                bool fServed = false;
                if (pindexLastHardenedCheckpoint)
                {
                    if (locator.Has(pindexLastHardenedCheckpoint->GetBlockHash()))
                    {
                        printf("getheaders: peer locator contains hardened checkpoint %d — serving canonical headers from there\n",
                            pindexLastHardenedCheckpoint->nHeight);
                        pindex = pindexLastHardenedCheckpoint;
                        fServed = true;
                    }
                    else
                    {
                        printf("getheaders: peer locator lacks hardened checkpoint %d — falling back to last common ancestor\n",
                            pindexLastHardenedCheckpoint->nHeight);
                    }
                }
                if (!fServed)
                {
                    // Last-common-ancestor walk via the public locator API. We can't
                    // iterate locator.vHave from outside the class (it's
                    // protected); CBlockLocator::FindCommonAncestorInMainChain
                    // does the walk for us and returns the deepest block
                    // we have on the main chain that the peer also knows.
                    // Falling back to genesis when no overlap exists
                    // ensures the peer gets a recoverable header chain.
                    CBlockIndex* pCommon = locator.FindCommonAncestorInMainChain();
                    if (pCommon && pCommon != pindexLastHardenedCheckpoint)
                    {
                        printf("getheaders: serving canonical headers from last common ancestor %d (peer may be on a fork)\n",
                            pCommon->nHeight);
                        pindex = pCommon;
                    }
                    else
                    {
                        printf("getheaders: peer locator has no common blocks — serving headers from genesis (peer on a long fork)\n");
                        pindex = pindexGenesisBlock;
                    }
                }
            }

            if (pindex)
            {
                if (pindex->pnext)
                {
                    pindex = pindex->pnext;
                }
                else
                {
                    // pnext is null — fall back to walking from pindexBest
                    // backwards to find the block immediately after pindex
                    CBlockIndex* pWalk = pindexBest;
                    while (pWalk && pWalk->pprev != pindex)
                        pWalk = pWalk->pprev;
                    pindex = pWalk;  // null if pindex is already the tip
                }
            }
        }

        vector<CBlock> vHeaders;
        int nLimit = 2000;
        printf("getheaders %d to %s\n", (pindex ? pindex->nHeight : -1), hashStop.ToString().substr(0,20).c_str());
        for (; pindex; pindex = pindex->pnext)
        {
            vHeaders.push_back(pindex->GetBlockHeader());
            if (--nLimit <= 0 || pindex->GetBlockHash() == hashStop)
                break;
        }
        pfrom->PushMessage("headers", vHeaders);
    }

    else if (strCommand == "headers")
    {
        vector<CBlock> vHeaders;
        vRecv >> vHeaders;
        if (!g_syncManager.ProcessHeaders(pfrom, vHeaders))
            return false;
    }


    else if (strCommand == "tx")
    {
        vector<uint256> vWorkQueue;
        vector<uint256> vEraseQueue;
        CDataStream vMsg(vRecv);
        auto txdb_holder = MakeChainDB("r"); CTxDBBase& txdb = *txdb_holder;
        CTransaction tx;
        vRecv >> tx;

        CInv inv(MSG_TX, tx.GetHash());
        pfrom->AddInventoryKnown(inv);

        bool fMissingInputs = false;
        if (tx.AcceptToMemoryPool(txdb, true, &fMissingInputs))
        {
            SyncWithWallets(tx, nullptr, true);
            RelayTransaction(tx, inv.hash);
            mapAlreadyAskedFor.erase(inv);
            vWorkQueue.push_back(inv.hash);
            vEraseQueue.push_back(inv.hash);

            // Recursively process any orphan transactions that depended on this one
            for (unsigned int i = 0; i < vWorkQueue.size(); i++)
            {
                uint256 hashPrev = vWorkQueue[i];
                for (auto mi = mapOrphanTransactionsByPrev[hashPrev].begin();
                     mi != mapOrphanTransactionsByPrev[hashPrev].end();
                     ++mi)
                {
                    const uint256& orphanTxHash = *mi;
                    CTransaction& orphanTx = mapOrphanTransactions[orphanTxHash];
                    bool fMissingInputs2 = false;

                    if (orphanTx.AcceptToMemoryPool(txdb, true, &fMissingInputs2))
                    {
                        printf("   accepted orphan tx %s\n", orphanTxHash.ToString().substr(0,10).c_str());
                        SyncWithWallets(tx, nullptr, true);
                        RelayTransaction(orphanTx, orphanTxHash);
                        mapAlreadyAskedFor.erase(CInv(MSG_TX, orphanTxHash));
                        vWorkQueue.push_back(orphanTxHash);
                        vEraseQueue.push_back(orphanTxHash);
                    }
                    else if (!fMissingInputs2)
                    {
                        // invalid orphan
                        vEraseQueue.push_back(orphanTxHash);
                        printf("   removed invalid orphan tx %s\n", orphanTxHash.ToString().substr(0,10).c_str());
                    }
                }
            }

            for (uint256 hash : vEraseQueue)
                EraseOrphanTx(hash);
        }
        else if (fMissingInputs)
        {
            AddOrphanTx(tx);

            // DoS prevention: do not allow mapOrphanTransactions to grow unbounded
            unsigned int nEvicted = LimitOrphanTxSize(MAX_ORPHAN_TRANSACTIONS);
            if (nEvicted > 0)
                printf("mapOrphan overflow, removed %u tx\n", nEvicted);
        }
        if (tx.nDoS) pfrom->Misbehaving(tx.nDoS);
    }


    else if (strCommand == "block")
    {
        CBlock block;
        vRecv >> block;
        uint256 hashBlock = block.GetHash();

        // Log every block during IBD (with throttling after first 100)
        static int64_t nLastBlockLog = 0;
        static int nBlocksReceived = 0;
        nBlocksReceived++;
        bool fLogThis = (nBlocksReceived <= 20) || (nBestHeight % 500 == 0) || !IsInitialBlockDownload() || (GetTime() - nLastBlockLog >= 5);
        if (fLogThis) {
            printf("IBD-DIAG: block received #%d hash=%s from=%s ourHeight=%d\n",
                nBlocksReceived, hashBlock.ToString().substr(0,20).c_str(),
                pfrom->addr.ToString().c_str(), nBestHeight);
            nLastBlockLog = GetTime();
        }

        CInv inv(MSG_BLOCK, hashBlock);
        pfrom->AddInventoryKnown(inv);

        g_syncManager.TrackBlockDelivery(pfrom, hashBlock);

        if (ProcessBlock(pfrom, &block))
        {
            mapAlreadyAskedFor.erase(inv);

            if (IsInitialBlockDownload())
            {
                // Keep download window full after every accepted block
                g_syncManager.QueueBlocksParallel();

                static int nBlocksSinceRequest = 0;
                if (++nBlocksSinceRequest >= 500)
                {
                    nBlocksSinceRequest = 0;
                    // Pipeline refill: request from ALL connected full-node peers,
                    // not just the one that sent us this block. This spreads block
                    // download across multiple peers for better throughput.
                    // Also send getheaders to scout ahead faster than full blocks.
                    {
                        LOCK(cs_vNodes);
                        for (CNode* pnode : vNodes)
                        {
                            if (!pnode->fClient && pnode->nVersion != 0)
                            {
                                pnode->pindexLastGetBlocksBegin = nullptr;
                                pnode->PushGetBlocks(pindexBest, uint256(0));
                                pnode->pindexLastGetHeadersBegin = nullptr;
                                pnode->PushGetHeaders(pindexBest, uint256(0));
                            }
                        }
                    }
                    printf("IBD-DIAG: pipeline refill to all peers at height %d\n", nBestHeight);
                }
            }
        }
        else
        {
            printf("IBD-DIAG: ProcessBlock FAILED for block %s (height after prev=%d, DoS=%d)\n",
                hashBlock.ToString().substr(0,20).c_str(), nBestHeight, block.nDoS);
        }

        if (block.nDoS) {
            printf("IBD-DIAG: Misbehaving peer %s by %d\n",
                pfrom->addr.ToString().c_str(), block.nDoS);
            pfrom->Misbehaving(block.nDoS);
        }

        if (fSecMsgEnabled && !IsInitialBlockDownload())
            SecureMsgScanBlock(block);
    }


    else if (strCommand == "cmpctblock")
    {
        CCompactBlock cmpctblock;
        vRecv >> cmpctblock;

        // Delegate to the standalone ProcessCompactBlock() which handles:
        //   - mempool short-ID matching with collision detection
        //   - merkle root verification before acceptance
        //   - partial block storage + getblocktxn request on missing txs
        //   - DoS scoring for malformed messages
        ProcessCompactBlock(pfrom, cmpctblock);
    }


    else if (strCommand == "getblocktxn")
    {
        CBlockTxnRequest req;
        vRecv >> req;

        // Look up the block and send requested transactions
        auto mi = mapBlockIndex.find(req.blockhash);
        if (mi != mapBlockIndex.end())
        {
            CBlock block;
            if (block.ReadFromDisk(mi->second))
            {
                CBlockTxnResponse resp;
                resp.blockhash = req.blockhash;
                for (uint16_t idx : req.vIndex)
                {
                    if (idx < block.vtx.size())
                        resp.vTxn.push_back(block.vtx[idx]);
                }
                pfrom->PushMessage("blocktxn", resp);
            }
        }
    }


    else if (strCommand == "blocktxn")
    {
        CBlockTxnResponse resp;
        vRecv >> resp;

        // Find the partial block awaiting these transactions
        auto mi = mapPartialBlocks.find(resp.blockhash);
        if (mi == mapPartialBlocks.end())
            return true;  // no longer need it

        CPartialBlock& partial = mi->second;
        unsigned int nFilled = 0;
        auto itTxn = resp.vTxn.begin();
        for (uint16_t idx : partial.setMissing)
        {
            if (itTxn == resp.vTxn.end())
                break;
            if (idx < partial.vTxFilled.size())
            {
                partial.vTxFilled[idx] = *itTxn;
                nFilled++;
            }
            ++itTxn;
        }
        partial.setMissing.clear();  // all filled now

        // Reconstruct the complete block
        CBlock block;
        block.nVersion = partial.cmpctblock.nVersion;
        block.hashPrevBlock = partial.cmpctblock.hashPrevBlock;
        block.hashMerkleRoot = partial.cmpctblock.hashMerkleRoot;
        block.nTime = partial.cmpctblock.nTime;
        block.nBits = partial.cmpctblock.nBits;
        block.nNonce = partial.cmpctblock.nNonce;
        block.vchBlockSig = partial.cmpctblock.vchBlockSig;
        block.vtx = partial.vTxFilled;

        // Verify merkle root to detect corrupted or malicious blocktxn responses
        uint256 hashMerkleComputed = block.BuildMerkleTree();
        if (hashMerkleComputed != block.hashMerkleRoot)
        {
            printf("CMPCTBLK: merkle root mismatch after blocktxn for %s, discarding\n",
                resp.blockhash.ToString().substr(0,20).c_str());
            mapPartialBlocks.erase(mi);
            pfrom->AskFor(CInv(MSG_BLOCK, resp.blockhash));
            return true;
        }

        printf("CMPCTBLK: completed block %s with %d missing txs from blocktxn\n",
            resp.blockhash.ToString().substr(0,20).c_str(), nFilled);

        pfrom->nBlocksDelivered++;
        if (nBestHeight > pfrom->nBestKnownHeight)
            pfrom->nBestKnownHeight = nBestHeight;
        ProcessBlock(pfrom, &block);
        mapAlreadyAskedFor.erase(CInv(MSG_BLOCK, resp.blockhash));
        mapPartialBlocks.erase(mi);
    }


    else if (strCommand == "getaddr")
    {
        // Don't return addresses older than nCutOff timestamp
        int64_t nCutOff = GetTime() - (nNodeLifespan * 24 * 60 * 60);
        pfrom->vAddrToSend.clear();
        vector<CAddress> vAddr = addrman.GetAddr();
        for (const CAddress &addr : vAddr)
            if(addr.nTime > nCutOff)
                pfrom->PushAddress(addr);
    }


    else if (strCommand == "mempool")
    {
        std::vector<uint256> vtxid;
        mempool.queryHashes(vtxid);
        vector<CInv> vInv;
        for (unsigned int i = 0; i < vtxid.size(); i++) {
            CInv inv(MSG_TX, vtxid[i]);
            vInv.push_back(inv);
            if (i == (MAX_INV_SZ - 1))
                    break;
        }
        if (vInv.size() > 0)
            pfrom->PushMessage("inv", vInv);
    }


    else if (strCommand == "ping")
    {
        if (pfrom->nVersion > BIP0031_VERSION)
        {
            uint64_t nonce = 0;
            vRecv >> nonce;
            pfrom->PushMessage("pong", nonce);
        }
    }


    else if (strCommand == "pong")
    {
        if (pfrom->nVersion > BIP0031_VERSION)
        {
            uint64_t nonce = 0;
            vRecv >> nonce;
            // Only accept pong if it matches our outstanding ping nonce
            if (nonce != 0 && nonce == pfrom->nPingNonceSent) {
                int64_t nRtt = GetTimeMicros() - pfrom->nPingUsecStart;
                if (nRtt > 0) {
                    pfrom->nPingUsecTime = nRtt;
                    // Update rolling average block latency if not set
                    if (pfrom->nAvgBlockLatencyUs == 0)
                        pfrom->nAvgBlockLatencyUs = nRtt;
                    else
                        pfrom->nAvgBlockLatencyUs = (pfrom->nAvgBlockLatencyUs * 3 + nRtt) / 4;
                }
                pfrom->nPingNonceSent = 0;
                pfrom->nPingUsecStart = 0;
                pfrom->nPingRetryCount = 0;
                if (fDebug)
                    printf("pong from %s: %.1fms\n", pfrom->addr.ToString().c_str(), (double)nRtt / 1000.0);
            }
        }
    }


    else if (strCommand == "getsnap" || strCommand == "snap" ||
             strCommand == "getsnapchunk" || strCommand == "snapchunk")
    {
        SnapshotNet::ProcessSnapshotMessage(pfrom, strCommand, vRecv);
    }


    else if (strCommand == "getwalletaddr")
    {
        // Peer is requesting our TRI receiving address for onion resolution.
        // Respond with address + signature proving ownership.
        if (!pwalletMain)
            return true;

        // Get our onion address to sign
        CTorV3Manager* torMgr = CTorV3Manager::GetInstance();
        std::string ourOnion = torMgr ? torMgr->GetWalletOnionAddress() : "";
        if (ourOnion.empty())
        {
            CTorEmbedded* torEmbed = CTorEmbedded::GetInstance();
            if (torEmbed)
                ourOnion = torEmbed->GetOnionAddress();
        }
        if (ourOnion.empty())
            return true; // Can't prove identity without onion address

        // Get the default receiving address
        CPubKey pubKey;
        if (!pwalletMain->GetKeyFromPool(pubKey, true))
            return true;

        CTrianglesAddress addr(pubKey.GetID());
        std::string strAddr = addr.ToString();

        // Sign our onion address with the wallet key
        CKeyID keyID;
        addr.GetKeyID(keyID);
        CKey key;
        if (!pwalletMain->GetKey(keyID, key))
            return true;

        CDataStream ss(SER_GETHASH, 0);
        ss << strMessageMagic;
        ss << ourOnion;

        std::vector<unsigned char> vchSig;
        if (!key.SignCompact(Hash(ss.begin(), ss.end()), vchSig))
            return true;

        pfrom->PushMessage("walletaddr", strAddr, vchSig);
    }

    else if (strCommand == "walletaddr")
    {
        // Peer is responding with their TRI address + signature
        std::string triAddr;
        std::vector<unsigned char> vchSig;
        vRecv >> triAddr >> vchSig;

        CTorV3Manager* torMgr = CTorV3Manager::GetInstance();
        if (torMgr)
            torMgr->HandleWalletAddrResponse(pfrom, triAddr, vchSig);
    }

    else if (strCommand == "seeder")
    {
        // Receive seeder node announcement
        std::string onionAddress;
        int port;
        vRecv >> onionAddress >> port;

        std::string seederAddr = onionAddress + ":" + std::to_string(port);
        CTorV3Manager* torMgr = CTorV3Manager::GetInstance();
        if (torMgr && torMgr->IsTorEnabled())
        {
            torMgr->ConnectToSeederNode(seederAddr);
            torMgr->UpdateSeederLastSeen(seederAddr);
        }
    }

    else if (strCommand == "getseederlist")
    {
        // Peer is requesting our known seeder list
        CTorV3Manager* torMgr = CTorV3Manager::GetInstance();
        if (torMgr && torMgr->IsTorEnabled())
        {
            std::vector<std::string> seederList = torMgr->GetKnownSeederNodes();
            pfrom->PushMessage("seederlist", seederList);
        }
    }

    else if (strCommand == "seederlist")
    {
        // Receive seeder list from peer
        std::vector<std::string> seederList;
        vRecv >> seederList;

        CTorV3Manager* torMgr = CTorV3Manager::GetInstance();
        if (torMgr && torMgr->IsTorEnabled())
        {
            torMgr->HandleSeederListMessage(pfrom, seederList);
        }
    }

    else
    {
        if (fSecMsgEnabled)
            SecureMsgReceiveData(pfrom, strCommand, vRecv);

        // Ignore unknown commands for extensibility
    }


    // Update the last seen time for this node's address
    if (pfrom->fNetworkNode)
        if (strCommand == "version" || strCommand == "addr" || strCommand == "inv" || strCommand == "getdata" || strCommand == "ping" || strCommand == "pong")
            AddressCurrentlyConnected(pfrom->addr);


    return true;
}

bool ProcessMessages(CNode* pfrom)
{
    //if (fDebug)
    //    printf("ProcessMessages(%u bytes)\n", vRecv.size());

    //
    // Message format
    //  (4) message start
    //  (12) command
    //  (4) size
    //  (4) checksum
    //  (x) data
    //
    bool fOk = true;

    std::deque<CNetMessage>::iterator it = pfrom->vRecvMsg.begin();
    while (!pfrom->fDisconnect && it != pfrom->vRecvMsg.end()) {
        // Don't bother if send buffer is too full to respond anyway
        if (pfrom->nSendSize >= SendBufferSize())
            break;

        // get next message
        CNetMessage& msg = *it;

        //if (fDebug)
        //    printf("ProcessMessages(message %u msgsz, %zu bytes, complete:%s)\n",
        //            msg.hdr.nMessageSize, msg.vRecv.size(),
        //            msg.complete() ? "Y" : "N");

        // end, if an incomplete message is found
        if (!msg.complete())
            break;

        // at this point, any failure means we can delete the current message
        it++;

        // Scan for message start
        if (memcmp(msg.hdr.pchMessageStart, pchMessageStart, sizeof(pchMessageStart)) != 0) {
            printf("\n\nPROCESSMESSAGE: INVALID MESSAGESTART\n\n");
            fOk = false;
            break;
        }

        // Read header
        CMessageHeader& hdr = msg.hdr;
        if (!hdr.IsValid())
        {
            printf("\n\nPROCESSMESSAGE: ERRORS IN HEADER %s\n\n\n", hdr.GetCommand().c_str());
            continue;
        }
        string strCommand = hdr.GetCommand();

        // Message size
        unsigned int nMessageSize = hdr.nMessageSize;

        // Checksum
        CDataStream& vRecv = msg.vRecv;
        uint256 hash = Hash(vRecv.begin(), vRecv.begin() + nMessageSize);
        unsigned int nChecksum = 0;
        memcpy(&nChecksum, &hash, sizeof(nChecksum));
        if (nChecksum != hdr.nChecksum)
        {
            printf("ProcessMessages(%s, %u bytes) : CHECKSUM ERROR nChecksum=%08x hdr.nChecksum=%08x\n",
               strCommand.c_str(), nMessageSize, nChecksum, hdr.nChecksum);
            continue;
        }

        // Process message
        bool fRet = false;
        try
        {
            {
                LOCK(cs_main);
                fRet = ProcessMessage(pfrom, strCommand, vRecv);
            }
            if (fShutdown)
                return true;
        }
        catch (std::ios_base::failure& e)
        {
            if (strstr(e.what(), "end of data"))
            {
                // Allow exceptions from under-length message on vRecv
                printf("ProcessMessages(%s, %u bytes) : Exception '%s' caught, normally caused by a message being shorter than its stated length\n", strCommand.c_str(), nMessageSize, e.what());
            }
            else if (strstr(e.what(), "size too large"))
            {
                // Allow exceptions from over-long size
                printf("ProcessMessages(%s, %u bytes) : Exception '%s' caught\n", strCommand.c_str(), nMessageSize, e.what());
            }
            else
            {
                PrintExceptionContinue(&e, "ProcessMessages()");
            }
        }
        catch (std::exception& e) {
            PrintExceptionContinue(&e, "ProcessMessages()");
        } catch (...) {
            PrintExceptionContinue(nullptr, "ProcessMessages()");
        }

        if (!fRet && fDebug)
            printf("ProcessMessage(%s, %u bytes) FAILED\n", strCommand.c_str(), nMessageSize);
    }

    // In case the connection got shut down, its receive buffer was wiped
    if (!pfrom->fDisconnect)
        pfrom->vRecvMsg.erase(pfrom->vRecvMsg.begin(), it);

    return fOk;
}


bool SendMessages(CNode* pto, bool fSendTrickle)
{
    TRY_LOCK(cs_main, lockMain);
    if (lockMain) {
        // Don't send anything until we get their version message
        if (pto->nVersion == 0)
            return true;

        // Periodically clean up expired partial compact blocks (BIP152)
        CleanupPartialBlocks();

        // Keep-alive ping every 2 minutes (critical for Tor connections that
        // can be silently dropped). Also measures round-trip latency.
        {
            bool fPingNeeded = false;
            // Send ping every 2 minutes if no recent send activity
            if (pto->nLastSend && GetTime() - pto->nLastSend > 120 && pto->ssSend.empty())
                fPingNeeded = true;
            // Also ping if we haven't sent one in 2 minutes regardless
            if (pto->nPingUsecStart == 0 && GetTime() - pto->nTimeConnected > 120)
                fPingNeeded = true;
            if (pto->nPingUsecStart > 0 && GetTimeMicros() - pto->nPingUsecStart > 120 * 1000000)
                fPingNeeded = true; // outstanding ping timed out, retry

            if (fPingNeeded) {
                // Check for dead peer: 3 consecutive unanswered pings = disconnect
                if (pto->nPingNonceSent != 0 && pto->nPingUsecStart > 0) {
                    pto->nPingRetryCount++;
                    if (pto->nPingRetryCount >= 3) {
                        printf("ping timeout: %s (no pong for %d pings, %.1fs)\n",
                            pto->addr.ToString().c_str(), pto->nPingRetryCount,
                            (double)(GetTimeMicros() - pto->nPingUsecStart) / 1000000.0);
                        pto->fDisconnect = true;
                    }
                }
                if (!pto->fDisconnect) {
                    uint64_t nonce = 0;
                    while (nonce == 0)
                        RAND_bytes((unsigned char*)&nonce, sizeof(nonce));
                    pto->nPingNonceSent = nonce;
                    pto->nPingUsecStart = GetTimeMicros();
                    pto->PushMessage("ping", nonce);
                }
            }
        }

        // Resend wallet transactions that haven't gotten in a block yet
        ResendWalletTransactions();

        // Address refresh broadcast — every hour for small Tor-only networks
        // (was 24 hours, but small networks need faster address propagation)
        static int64_t nLastRebroadcast;
        if (!IsInitialBlockDownload() && (GetTime() - nLastRebroadcast > 60 * 60))
        {
            {
                LOCK(cs_vNodes);
                for (CNode* pnode : vNodes)
                {
                    // Periodically clear setAddrKnown to allow refresh broadcasts
                    if (nLastRebroadcast)
                        pnode->setAddrKnown.clear();

                    // Rebroadcast our address
                    if (!fNoListen)
                    {
                        CAddress addr = GetLocalAddress(&pnode->addr);
                        if (addr.IsRoutable())
                            pnode->PushAddress(addr);
                    }

                    // Periodically re-request addresses (every hour)
                    // Helps small networks discover all peers faster
                    if (!pnode->fGetAddr && pnode->fSuccessfullyConnected)
                    {
                        pnode->PushMessage("getaddr");
                        pnode->fGetAddr = true;
                    }
                }
            }
            nLastRebroadcast = GetTime();
        }

        //
        // Message: addr
        //
        if (fSendTrickle)
        {
            vector<CAddress> vAddr;
            vAddr.reserve(pto->vAddrToSend.size());
            for (const CAddress& addr : pto->vAddrToSend)
            {
                // returns true if wasn't already contained in the set
                if (pto->setAddrKnown.insert(addr).second)
                {
                    vAddr.push_back(addr);
                    // receiver rejects addr messages larger than 1000
                    if (vAddr.size() >= 1000)
                    {
                        pto->PushMessage("addr", vAddr);
                        vAddr.clear();
                    }
                }
            }
            pto->vAddrToSend.clear();
            if (!vAddr.empty())
                pto->PushMessage("addr", vAddr);
        }


        //
        // Message: inventory
        //
        vector<CInv> vInv;
        vector<CInv> vInvWait;
        {
            LOCK(pto->cs_inventory);
            vInv.reserve(pto->vInventoryToSend.size());
            vInvWait.reserve(pto->vInventoryToSend.size());
            for (const CInv& inv : pto->vInventoryToSend)
            {
                if (pto->setInventoryKnown.count(inv))
                    continue;

                // trickle out tx inv to protect privacy
                if (inv.type == MSG_TX && !fSendTrickle)
                {
                    // 1/4 of tx invs blast to all immediately
                    static uint256 hashSalt;
                    if (hashSalt == 0)
                        hashSalt = GetRandHash();
                    uint256 hashRand = inv.hash ^ hashSalt;
                    hashRand = Hash(BEGIN(hashRand), END(hashRand));
                    bool fTrickleWait = ((hashRand & 3) != 0);

                    // always trickle our own transactions
                    if (!fTrickleWait)
                    {
                        CWalletTx wtx;
                        if (GetTransaction(inv.hash, wtx))
                            if (wtx.fFromMe)
                                fTrickleWait = true;
                    }

                    if (fTrickleWait)
                    {
                        vInvWait.push_back(inv);
                        continue;
                    }
                }

                // returns true if wasn't already contained in the set
                if (pto->setInventoryKnown.insert(inv).second)
                {
                    vInv.push_back(inv);
                    if (vInv.size() >= 1000)
                    {
                        pto->PushMessage("inv", vInv);
                        vInv.clear();
                    }
                }
            }
            pto->vInventoryToSend = vInvWait;
        }
        if (!vInv.empty())
            pto->PushMessage("inv", vInv);


        //
        // Periodic chain-tip sync: every 45 seconds, ask each peer if they
        // have blocks we don't.  On a small Tor-only network, transient
        // partitions can cause forks that persist silently — this ensures
        // nodes discover the longer chain even without explicit announcement.
        //
        if (!IsInitialBlockDownload() && !pto->fClient && pindexBest &&
            GetTime() - pto->nLastTipCheck > 45)
        {
            pto->nLastTipCheck = GetTime();
            pto->pindexLastGetBlocksBegin = nullptr;  // reset dedup to force request
            pto->PushGetBlocks(pindexBest, uint256(0));
        }

        //
        // Slow-peer eviction: every 5 minutes during sync, identify the
        // outbound peer with the fewest blocks delivered and disconnect it
        // to free the slot for a potentially faster peer. This is critical
        // on Tor networks with high latency variance.
        //
        if (nBestHeight < GetNumBlocksOfPeers() && !pto->fClient && !pto->fInbound)
        {
            static int64_t nLastEvictionCheck = 0;
            if (GetTime() - nLastEvictionCheck > 5 * 60)
            {
                nLastEvictionCheck = GetTime();
                CNode* pWorst = nullptr;
                int nWorstBlocks = INT_MAX;
                int nOutbound = 0;
                {
                    LOCK(cs_vNodes);
                    for (CNode* pnode : vNodes)
                    {
                        if (pnode->fInbound || pnode->fDisconnect || pnode->fClient)
                            continue;
                        nOutbound++;
                        // Only consider peers connected for at least 3 minutes
                        if (GetTime() - pnode->nTimeConnected < 3 * 60)
                            continue;
                        if (pnode->nBlocksDelivered < nWorstBlocks)
                        {
                            nWorstBlocks = pnode->nBlocksDelivered;
                            pWorst = pnode;
                        }
                    }
                    // Only evict if we have at least 3 outbound peers and the worst
                    // peer has delivered significantly fewer blocks than average
                    if (pWorst && nOutbound >= 3 && nWorstBlocks == 0)
                    {
                        printf("PEER-EVICT: disconnecting slow peer %s (0 blocks delivered in %ds)\n",
                            pWorst->addr.ToString().c_str(),
                            (int)(GetTime() - pWorst->nTimeConnected));
                        pWorst->fDisconnect = true;
                    }
                }
            }
        }

        //
        // Stall detection: if we're still catching up and no new blocks for
        // a while, re-request. Active during IBD (5s timeout) and also
        // post-IBD when we're behind peers (30s timeout) to handle the case
        // where IBD flips to false during a transient download gap.
        //
        if (!pto->fClient && nBestHeight < GetNumBlocksOfPeers())
        {
            static int64_t nLastBlockReceived = 0;
            static int nLastHeight = 0;
            static int64_t nLastStallLog = 0;
            // Adaptive stall timeout: use peer's latency if known
            int nStallTimeout;
            if (pto->nAvgBlockLatencyUs > 0) {
                // 5x average latency, clamped to 5-60 seconds
                nStallTimeout = std::max(5, std::min(60, (int)(pto->nAvgBlockLatencyUs * 5 / 1000000)));
            } else {
                nStallTimeout = IsInitialBlockDownload() ? 10 : 30;
            }
            if (nBestHeight > nLastHeight) {
                nLastHeight = nBestHeight;
                nLastBlockReceived = GetTime();
            } else if (nLastBlockReceived > 0 && GetTime() - nLastBlockReceived > nStallTimeout) {
                if (GetTime() - nLastStallLog >= 15) { // log every 15s max
                    printf("SYNC-DIAG: STALL at height %d/%d for %ds (IBD=%d walk=%d), peer=%s askfor_queue=%d\n",
                        nBestHeight, GetNumBlocksOfPeers(),
                        (int)(GetTime() - nLastBlockReceived),
                        IsInitialBlockDownload(), nHighestInvWalk,
                        pto->addr.ToString().c_str(),
                        (int)pto->mapAskFor.size());
                    nLastStallLog = GetTime();
                }
                // During IBD, avoid falling back to legacy getblocks recovery
                // anchored at pindexBest or a stale inv walk point. That path can
                // repeatedly resolve the locator to the same low common ancestor
                // on a weak peer set, which looks like a sync "freeze" near an
                // early height even though the real bug is the recovery loop.
                // Keep stall recovery header-driven instead so the planner tip
                // advances from the newest known header state.
                if (IsInitialBlockDownload())
                {
                    pto->pindexLastGetHeadersBegin = nullptr;

                    uint256 hashLocatorTip = g_syncManager.GetBestHeader();
                    if (hashLocatorTip == 0 && nHighestInvWalk > nBestHeight &&
                        hashHighestInvWalk != 0 && mapBlockIndex.count(hashHighestInvWalk))
                    {
                        hashLocatorTip = hashHighestInvWalk;
                    }

                    unsigned int nRefilled = g_syncManager.RequestRefillAllPeers(
                        hashLocatorTip,
                        0,
                        "stall-recovery");
                    unsigned int nQueued = g_syncManager.QueueBlocksParallel();

                    printf("SYNC-DIAG: stall recovery used headers-first path (locator=%s, refillPeers=%u, queued=%u)\n",
                        hashLocatorTip.ToString().substr(0,20).c_str(),
                        nRefilled,
                        nQueued);
                }
                else
                {
                    // Outside IBD, preserve the older walk-forward getblocks
                    // behavior since we're no longer building out a header planner.
                    pto->pindexLastGetBlocksBegin = nullptr;
                    if (nHighestInvWalk > nBestHeight && hashHighestInvWalk != 0 &&
                        mapBlockIndex.count(hashHighestInvWalk))
                    {
                        pto->PushGetBlocks(mapBlockIndex[hashHighestInvWalk], uint256(0));
                        printf("SYNC-DIAG: stall re-request from walk=%d (not best=%d)\n",
                            nHighestInvWalk, nBestHeight);
                    }
                    else
                    {
                        pto->PushGetBlocks(pindexBest, uint256(0));
                    }

                    pto->pindexLastGetHeadersBegin = nullptr;
                    pto->PushGetHeaders(pindexBest, uint256(0));
                }
                nLastBlockReceived = GetTime();
            }
        }

        // Per-peer IBD getheaders heartbeat and block-planner cadence.
        // Logic lives in CSyncManager::Tick — see syncmanager.cpp.
        g_syncManager.Tick(pto, nHighestInvWalk, hashHighestInvWalk);

        //
        // Message: getdata
        //
        // Periodic IBD status
        if (IsInitialBlockDownload()) {
            static int64_t nLastStatus = 0;
            if (GetTime() - nLastStatus >= 15) {
                printf("IBD-DIAG: STATUS height=%d plannerHeight=%d plannerDepth=%u inflight=%u peers=%d askfor_queued=%d orphans=%d\n",
                    nBestHeight,
                    g_syncManager.GetPlannerHeight(),
                    g_syncManager.GetPlannerDepth(),
                    g_syncManager.CountInFlight(),
                    (int)vNodes.size(),
                    (int)pto->mapAskFor.size(),
                    (int)mapOrphanBlocks.size());
                nLastStatus = GetTime();
            }
        }

        vector<CInv> vGetData;
        int64_t nNow = GetTime() * 1000000;
        auto txdb_holder = MakeChainDB("r"); CTxDBBase& txdb = *txdb_holder;
        // During IBD, send larger getdata batches since PoS blocks are small
        // and the bottleneck is round-trip latency, not bandwidth.
        unsigned int nGetDataBatchSize = IsInitialBlockDownload() ? 4000 : 1000;
        while (!pto->mapAskFor.empty() && (*pto->mapAskFor.begin()).first <= nNow)
        {
            const CInv& inv = (*pto->mapAskFor.begin()).second;
            if (!AlreadyHave(txdb, inv))
            {
                if (fDebugNet)
                    printf("sending getdata: %s\n", inv.ToString().c_str());
                vGetData.push_back(inv);
                if (vGetData.size() >= nGetDataBatchSize)
                {
                    pto->PushMessage("getdata", vGetData);
                    vGetData.clear();
                }
                mapAlreadyAskedFor[inv] = nNow;
            }
            pto->mapAskFor.erase(pto->mapAskFor.begin());
        }
        if (!vGetData.empty())
            pto->PushMessage("getdata", vGetData);
        
        if (fSecMsgEnabled)
            SecureMsgSendData(pto, fSendTrickle); // should be in cs_main?
    }
    return true;
}
