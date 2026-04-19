// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "alert.h"
#include "checkpoints.h"
#include "db.h"
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
#include <algorithm>
#include <boost/algorithm/string/replace.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>


using namespace std;
using namespace boost;
namespace fs = boost::filesystem;

//
// Global state
//

CCriticalSection cs_setpwalletRegistered;
set<CWallet*> setpwalletRegistered;

CCriticalSection cs_main;

CTxMemPool mempool;
unsigned int nTransactionsUpdated = 0;
CCheckQueue<CScriptCheck>* pScriptCheckQueue = NULL;

map<uint256, CBlockIndex*> mapBlockIndex;
set<pair<COutPoint, unsigned int> > setStakeSeen;
//libzerocoin::Params* ZCParams;
//uint256 hashGenesisBlock = hashGenesisBlockOfficial;

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

CBlockIndex* pindexGenesisBlock = NULL;
int nBestHeight = -1;
int nHighestInvWalk = 0;         // height of walk-forward progress through already-have inv
uint256 hashHighestInvWalk = 0;  // hash of that block

uint256 nBestChainTrust = 0;
uint256 nBestInvalidTrust = 0;

uint256 hashBestChain = 0;
CBlockIndex* pindexBest = NULL;
bool fAddressIndex = false;
int64_t nTimeBestReceived = 0;

CMedianFilter<int> cPeerBlockCounts(5, 0); // Amount of blocks that other nodes claim to have

map<uint256, CBlock*> mapOrphanBlocks;
multimap<uint256, CBlock*> mapOrphanBlocksByPrev;
set<pair<COutPoint, unsigned int> > setStakeSeenOrphan;
//map<uint256, uint256> mapProofOfStake;

map<uint256, CTransaction> mapOrphanTransactions;
map<uint256, set<uint256> > mapOrphanTransactionsByPrev;

// Constant stuff for coinbase transactions we create:
CScript COINBASE_FLAGS;

const string strMessageMagic = "Triangles Signed Message:\n";

//double dHashesPerSec;
//int64_t nHPSTimerStart;

// Settings
int64_t nTransactionFee = MIN_TX_FEE;
int64_t nReserveBalance = 0;
int64_t nMinimumInputValue = 0;

extern enum Checkpoints::CPMode CheckpointsMode;

namespace
{

struct CHeaderSyncNode
{
    CBlock header;
    int nHeight;
    uint256 nChainTrust;
    bool fRequested;
    int64_t nLastRequestTime;
};

static std::map<uint256, CHeaderSyncNode> mapHeaderSync;
static uint256 hashBestHeaderSync = 0;
static CCriticalSection cs_PostIbdWork;
static bool fPostIbdWorkStarted = false;

static const unsigned int MAX_HEADER_SYNC_CACHE = 50000;
static const unsigned int HEADER_DOWNLOAD_WINDOW = 512;  // Increased from 128 for parallel downloads
static const unsigned int HEADER_DOWNLOAD_PER_PEER = 64;   // Max blocks to request from each peer
static const int64_t HEADER_REQUEST_TIMEOUT_MICROS = 30 * 1000000;
static const int64_t HEADER_REDUNDANT_REQUEST_MICROS = 10 * 1000000;  // Request from another peer after 10s

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
    }
    catch (std::exception& e)
    {
        PrintExceptionContinue(&e, "ThreadPostIbdWork()");
    }
    catch (...)
    {
        PrintExceptionContinue(NULL, "ThreadPostIbdWork()");
    }
}

static uint256 GetHeaderSyncTrust(unsigned int nBits)
{
    CBigNum bnTarget;
    bnTarget.SetCompact(nBits);

    if (bnTarget <= 0)
        return 0;

    return ((CBigNum(1) << 256) / (bnTarget + 1)).getuint256();
}

static bool GetKnownHeaderState(const uint256& hash, int& nHeight, uint256& nChainTrust)
{
    std::map<uint256, CBlockIndex*>::const_iterator miBlock = mapBlockIndex.find(hash);
    if (miBlock != mapBlockIndex.end())
    {
        nHeight = miBlock->second->nHeight;
        nChainTrust = miBlock->second->nChainTrust;
        return true;
    }

    std::map<uint256, CHeaderSyncNode>::const_iterator miHeader = mapHeaderSync.find(hash);
    if (miHeader != mapHeaderSync.end())
    {
        nHeight = miHeader->second.nHeight;
        nChainTrust = miHeader->second.nChainTrust;
        return true;
    }

    return false;
}

static bool GetHeaderSyncPrevHash(const uint256& hash, uint256& hashPrev)
{
    std::map<uint256, CHeaderSyncNode>::const_iterator miHeader = mapHeaderSync.find(hash);
    if (miHeader != mapHeaderSync.end())
    {
        hashPrev = miHeader->second.header.hashPrevBlock;
        return true;
    }

    std::map<uint256, CBlockIndex*>::const_iterator miBlock = mapBlockIndex.find(hash);
    if (miBlock != mapBlockIndex.end() && miBlock->second->pprev)
    {
        hashPrev = miBlock->second->pprev->GetBlockHash();
        return true;
    }

    return false;
}

static void RecomputeBestHeaderSync()
{
    hashBestHeaderSync = 0;
    uint256 nBestTrust = 0;

    for (std::map<uint256, CHeaderSyncNode>::const_iterator it = mapHeaderSync.begin(); it != mapHeaderSync.end(); ++it)
    {
        if (hashBestHeaderSync == 0 || it->second.nChainTrust > nBestTrust)
        {
            hashBestHeaderSync = it->first;
            nBestTrust = it->second.nChainTrust;
        }
    }
}

static void PruneHeaderSync()
{
    if (mapHeaderSync.size() <= MAX_HEADER_SYNC_CACHE)
        return;

    printf("IBD-DIAG: header sync cache exceeded %u entries, clearing planner state\n", MAX_HEADER_SYNC_CACHE);
    mapHeaderSync.clear();
    hashBestHeaderSync = 0;
}

static bool AddHeaderSyncNode(const CBlock& header, const uint256& hashHeader)
{
    if (mapBlockIndex.count(hashHeader) || mapHeaderSync.count(hashHeader))
        return true;

    if (!header.vtx.empty())
    {
        printf("IBD-DIAG: header rejected (has vtx) hash=%s\n", hashHeader.ToString().substr(0,20).c_str());
        return false;
    }

    if (header.GetBlockTime() > GetTime() + 15 * 60)
    {
        printf("IBD-DIAG: header rejected (future time) hash=%s time=%u\n",
            hashHeader.ToString().substr(0,20).c_str(), header.nTime);
        return false;
    }

    int nPrevHeight = -1;
    uint256 nPrevChainTrust = 0;
    if (!GetKnownHeaderState(header.hashPrevBlock, nPrevHeight, nPrevChainTrust))
    {
        printf("IBD-DIAG: header rejected (prev unknown) hash=%s prevHash=%s\n",
            hashHeader.ToString().substr(0,20).c_str(),
            header.hashPrevBlock.ToString().substr(0,20).c_str());
        return false;
    }

    const int nHeight = nPrevHeight + 1;
    if (nHeight <= CUTOFF_POW_BLOCK && !CheckProofOfWork(hashHeader, header.nBits))
    {
        printf("IBD-DIAG: header PoW FAILED at height %d hash=%s nBits=%08x prevHash=%s\n",
            nHeight, hashHeader.ToString().substr(0,20).c_str(), header.nBits,
            header.hashPrevBlock.ToString().substr(0,20).c_str());
        return false;
    }

    CHeaderSyncNode node;
    node.header = header;
    node.nHeight = nHeight;
    node.nChainTrust = nPrevChainTrust + GetHeaderSyncTrust(header.nBits);
    node.fRequested = false;
    node.nLastRequestTime = 0;

    mapHeaderSync.insert(std::make_pair(hashHeader, node));

    if (hashBestHeaderSync == 0 || node.nChainTrust > mapHeaderSync[hashBestHeaderSync].nChainTrust)
        hashBestHeaderSync = hashHeader;

    PruneHeaderSync();
    return true;
}

static CBlockLocator BuildHeaderSyncLocator(uint256 hashTip)
{
    if (hashTip == 0)
        return CBlockLocator(pindexBest);

    std::vector<uint256> vHave;
    int nStep = 1;

    while (hashTip != 0)
    {
        vHave.push_back(hashTip);

        for (int i = 0; i < nStep && hashTip != 0; ++i)
        {
            uint256 hashPrev = 0;
            if (!GetHeaderSyncPrevHash(hashTip, hashPrev))
                hashTip = 0;
            else
                hashTip = hashPrev;
        }

        if (vHave.size() > 10)
            nStep *= 2;
    }

    vHave.push_back(!fTestNet ? hashGenesisBlockOfficial : hashGenesisBlockTestNet);
    return CBlockLocator(vHave);
}

static std::vector<uint256> GetHeaderSyncDownloadPath(uint256 hashTip)
{
    std::vector<uint256> vPath;

    while (hashTip != 0 && !mapBlockIndex.count(hashTip))
    {
        std::map<uint256, CHeaderSyncNode>::const_iterator mi = mapHeaderSync.find(hashTip);
        if (mi == mapHeaderSync.end())
            break;

        vPath.push_back(hashTip);
        hashTip = mi->second.header.hashPrevBlock;
    }

    std::reverse(vPath.begin(), vPath.end());
    return vPath;
}

static unsigned int CountHeaderSyncInFlight()
{
    const int64_t nNow = GetTime() * 1000000;
    unsigned int nInFlight = 0;
    for (std::map<uint256, CHeaderSyncNode>::const_iterator it = mapHeaderSync.begin(); it != mapHeaderSync.end(); ++it)
    {
        if (it->second.fRequested && nNow - it->second.nLastRequestTime < HEADER_REQUEST_TIMEOUT_MICROS)
            ++nInFlight;
    }
    return nInFlight;
}

static unsigned int QueueHeaderSyncBlocks(CNode* pfrom, unsigned int nWindow)
{
    if (!pfrom || hashBestHeaderSync == 0)
        return 0;

    const std::vector<uint256> vPath = GetHeaderSyncDownloadPath(hashBestHeaderSync);
    if (vPath.empty())
        return 0;

    const int64_t nNow = GetTime() * 1000000;
    unsigned int nInFlight = CountHeaderSyncInFlight();
    unsigned int nQueued = 0;

    for (std::vector<uint256>::const_iterator it = vPath.begin(); it != vPath.end(); ++it)
    {
        if (nInFlight + nQueued >= nWindow)
            break;

        std::map<uint256, CHeaderSyncNode>::iterator mi = mapHeaderSync.find(*it);
        if (mi == mapHeaderSync.end())
            continue;

        if (mi->second.fRequested && nNow - mi->second.nLastRequestTime < HEADER_REQUEST_TIMEOUT_MICROS)
            continue;

        pfrom->AskFor(CInv(MSG_BLOCK, *it));
        mi->second.fRequested = true;
        mi->second.nLastRequestTime = nNow;
        ++nQueued;
    }

    return nQueued;
}

static void MarkHeaderSyncBlockAccepted(const uint256& hashBlock)
{
    std::map<uint256, CHeaderSyncNode>::iterator mi = mapHeaderSync.find(hashBlock);
    if (mi == mapHeaderSync.end())
        return;

    mapHeaderSync.erase(mi);
    if (hashBestHeaderSync == hashBlock)
        RecomputeBestHeaderSync();
}

static void ContinueHeaderSync(CNode* pfrom, const uint256& hashTip)
{
    if (!pfrom || hashTip == 0)
        return;

    CBlockLocator locator = BuildHeaderSyncLocator(hashTip);
    if (locator.IsNull())
        return;

    pfrom->PushMessage("getheaders", locator, uint256(0));
}

// Parallel block downloading: distribute blocks across all available peers
static unsigned int QueueHeaderSyncBlocksParallel(unsigned int nWindow)
{
    if (hashBestHeaderSync == 0)
        return 0;

    const std::vector<uint256> vPath = GetHeaderSyncDownloadPath(hashBestHeaderSync);
    if (vPath.empty())
        return 0;

    // Collect eligible peers
    std::vector<CNode*> vEligiblePeers;
    {
        LOCK(cs_vNodes);
        for (CNode* pnode : vNodes)
        {
            if (!pnode->fClient && pnode->nVersion != 0 && !pnode->fDisconnect)
                vEligiblePeers.push_back(pnode);
        }
    }

    if (vEligiblePeers.empty())
        return 0;

    const int64_t nNow = GetTime() * 1000000;
    unsigned int nInFlight = CountHeaderSyncInFlight();
    unsigned int nQueued = 0;
    unsigned int nPeerIndex = 0;

    // Distribute blocks across peers in round-robin fashion
    for (std::vector<uint256>::const_iterator it = vPath.begin(); it != vPath.end(); ++it)
    {
        if (nInFlight + nQueued >= nWindow)
            break;

        std::map<uint256, CHeaderSyncNode>::iterator mi = mapHeaderSync.find(*it);
        if (mi == mapHeaderSync.end())
            continue;

        // Check if already requested recently
        bool fNeedsRequest = false;
        if (!mi->second.fRequested)
        {
            // Never requested - request now
            fNeedsRequest = true;
        }
        else if (nNow - mi->second.nLastRequestTime >= HEADER_REQUEST_TIMEOUT_MICROS)
        {
            // Timeout expired - retry
            fNeedsRequest = true;
        }
        else if (nNow - mi->second.nLastRequestTime >= HEADER_REDUNDANT_REQUEST_MICROS)
        {
            // Redundant request: ask another peer if original is slow
            // This creates parallel downloads for slow blocks
            fNeedsRequest = true;
        }

        if (!fNeedsRequest)
            continue;

        // Round-robin across peers to distribute load
        CNode* pnode = vEligiblePeers[nPeerIndex % vEligiblePeers.size()];
        pnode->AskFor(CInv(MSG_BLOCK, *it));

        // Update tracking (only on first request, not redundant)
        if (!mi->second.fRequested || nNow - mi->second.nLastRequestTime >= HEADER_REQUEST_TIMEOUT_MICROS)
        {
            mi->second.fRequested = true;
            mi->second.nLastRequestTime = nNow;
        }

        ++nQueued;
        ++nPeerIndex;
    }

    if (nQueued > 0)
        printf("IBD-DIAG: parallel queue distributed %u blocks across %zu peers (window=%u, inflight=%u)\n",
            nQueued, vEligiblePeers.size(), nWindow, nInFlight);

    return nQueued;
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
    for (CWallet* pwallet : setpwalletRegistered)
        if (pwallet->IsFromMe(tx))
            return true;
    return false;
}

// get the wallet transaction with the given hash (if it exists)
bool static GetTransaction(const uint256& hashTx, CWalletTx& wtx)
{
    for (CWallet* pwallet : setpwalletRegistered)
        if (pwallet->GetTransaction(hashTx,wtx))
            return true;
    return false;
}

// erases transaction with the given hash from all wallets
void static EraseFromWallets(uint256 hash)
{
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
            for (CWallet* pwallet : setpwalletRegistered)
                if (pwallet->IsFromMe(tx))
                    pwallet->DisableTransaction(tx);
        }
        return;
    }

    for (CWallet* pwallet : setpwalletRegistered)
        pwallet->AddToWalletIfInvolvingMe(tx, pblock, fUpdate);
}

// notify wallets about a new best chain
void static SetBestChain(const CBlockLocator& loc)
{
    for (CWallet* pwallet : setpwalletRegistered)
        pwallet->SetBestChain(loc);
}

static bool UpdateAddressIndexSyncState(CTxDB& txdb, const CBlockIndex* pindexNew)
{
    if (!fAddressIndex || pindexNew == NULL)
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
    for (CWallet* pwallet : setpwalletRegistered)
        pwallet->UpdatedTransaction(hashTx);
}

// dump all wallets
void static PrintWallets(const CBlock& block)
{
    for (CWallet* pwallet : setpwalletRegistered)
        pwallet->PrintWallet(block);
}

// notify wallets about an incoming inventory (for request counts)
void static Inventory(const uint256& hash)
{
    for (CWallet* pwallet : setpwalletRegistered)
        pwallet->Inventory(hash);
}

// ask wallets to resend their transactions
void ResendWalletTransactions(bool fForce)
{
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
        map<uint256, CTransaction>::iterator it = mapOrphanTransactions.lower_bound(randomhash);
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

bool CTransaction::ReadFromDisk(CTxDB& txdb, COutPoint prevout, CTxIndex& txindexRet)
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

bool CTransaction::ReadFromDisk(CTxDB& txdb, COutPoint prevout)
{
    CTxIndex txindex;
    return ReadFromDisk(txdb, prevout, txindex);
}

bool CTransaction::ReadFromDisk(COutPoint prevout)
{
    CTxDB txdb("r");
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
        MapPrevTx::const_iterator mi = mapInputs.find(vin[i].prevout);
        if (mi == mapInputs.end())
            return false;
        const CUtxoEntry& entry = mi->second;

        vector<vector<unsigned char> > vSolutions;
        txnouttype whichType;
        // get the scriptPubKey corresponding to this input:
        const CScript& prevScript = entry.scriptPubKey;
        if (!Solver(prevScript, whichType, vSolutions))
            return false;
        int nArgsExpected = ScriptSigArgsExpected(whichType, vSolutions);
        if (nArgsExpected < 0)
            return false;

        // Transactions with extra stuff in their scriptSigs are
        // non-standard. Note that this EvalScript() call will
        // be quick, because if there are any operations
        // beside "push data" in the scriptSig the
        // IsStandard() call returns false
        vector<vector<unsigned char> > stack;
        if (!EvalScript(stack, vin[i].scriptSig, *this, i, 0))
            return false;

        if (whichType == TX_SCRIPTHASH)
        {
            if (stack.empty())
                return false;
            CScript subscript(stack.back().begin(), stack.back().end());
            vector<vector<unsigned char> > vSolutions2;
            txnouttype whichType2;
            if (!Solver(subscript, whichType2, vSolutions2))
                return false;
            if (whichType2 == TX_SCRIPTHASH)
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
        if (pblock == NULL)
        {
            // Load the block this tx is in
            CTxIndex txindex;
            if (!CTxDB("r").ReadTxIndex(GetHash(), txindex))
                return 0;
            if (!blockTmp.ReadFromDisk(txindex.pos.nFile, txindex.pos.nBlockPos))
                return 0;
            pblock = &blockTmp;
        }

        // Update the tx's hashBlock
        hashBlock = pblock->GetHash();

        // Locate the transaction
        for (nIndex = 0; nIndex < (int)pblock->vtx.size(); nIndex++)
            if (pblock->vtx[nIndex] == *(CTransaction*)this)
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
    map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.find(hashBlock);
    if (mi == mapBlockIndex.end())
        return 0;
    CBlockIndex* pindex = (*mi).second;
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
    for (unsigned int i = 0; i < vout.size(); i++)
    {
        const CTxOut& txout = vout[i];
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

int64_t CTransaction::GetMinFee(unsigned int nBlockSize, enum GetMinFee_mode mode, unsigned int nBytes) const
{
    // Base fee is either MIN_TX_FEE or MIN_RELAY_TX_FEE
    int64_t nBaseFee = (mode == GMF_RELAY) ? MIN_RELAY_TX_FEE : MIN_TX_FEE;

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


bool CTxMemPool::accept(CTxDB& txdb, CTransaction &tx, bool fCheckInputs,
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
    CTransaction* ptxOld = NULL;
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
        int64_t txMinFee = tx.GetMinFee(1000, GMF_RELAY, nSize);
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

bool CTransaction::AcceptToMemoryPool(CTxDB& txdb, bool fCheckInputs, bool* pfMissingInputs)
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
                    std::map<COutPoint, CInPoint>::iterator it = mapNextTx.find(COutPoint(hash, i));
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
        std::map<COutPoint, CInPoint>::iterator it = mapNextTx.find(txin.prevout);
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
    for (map<uint256, CTransaction>::iterator mi = mapTx.begin(); mi != mapTx.end(); ++mi)
        vtxid.push_back((*mi).first);
}




int CMerkleTx::GetDepthInMainChainINTERNAL(CBlockIndex* &pindexRet) const
{
    if (hashBlock == 0 || nIndex == -1)
        return 0;

    // Find the block it claims to be in
    map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.find(hashBlock);
    if (mi == mapBlockIndex.end())
        return 0;
    CBlockIndex* pindex = (*mi).second;
    if (!pindex || !pindex->IsInMainChain())
        return 0;

    // Make sure the merkle branch connects to this block
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


bool CMerkleTx::AcceptToMemoryPool(CTxDB& txdb, bool fCheckInputs)
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
    CTxDB txdb("r");
    return AcceptToMemoryPool(txdb);
}



bool CWalletTx::AcceptWalletTransaction(CTxDB& txdb, bool fCheckInputs)
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
    CTxDB txdb("r");
    return AcceptWalletTransaction(txdb);
}

int CTxIndex::GetDepthInMainChain() const
{
    // Read block header
    CBlock block;
    if (!block.ReadFromDisk(pos.nFile, pos.nBlockPos, false))
        return 0;
    // Find the block in the index
    map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.find(block.GetHash());
    if (mi == mapBlockIndex.end())
        return 0;
    CBlockIndex* pindex = (*mi).second;
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
        CTxDB txdb("r");
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
        pblock = mapOrphanBlocks[pblock->hashPrevBlock];
    return pblock->GetHash();
}

// triangles: find block wanted by given orphan block
uint256 WantedByOrphan(const CBlock* pblockOrphan)
{
    // Work back to the first block in the orphan chain
    while (mapOrphanBlocks.count(pblockOrphan->hashPrevBlock))
        pblockOrphan = mapOrphanBlocks[pblockOrphan->hashPrevBlock];
    return pblockOrphan->hashPrevBlock;
}

// Evict excess orphan blocks when limit is exceeded
// Returns number of orphans evicted
unsigned int LimitOrphanBlocks(unsigned int nMaxOrphans)
{
    unsigned int nEvicted = 0;
    while (mapOrphanBlocks.size() > nMaxOrphans)
    {
        // Evict a random orphan
        uint256 randomhash = GetRandHash();
        auto it = mapOrphanBlocks.lower_bound(randomhash);
        if (it == mapOrphanBlocks.end())
            it = mapOrphanBlocks.begin();

        if (it == mapOrphanBlocks.end())
            break;  // No orphans to evict

        CBlock* pblockEvict = it->second;
        uint256 evictHash = it->first;

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
        delete pblockEvict;
        mapOrphanBlocks.erase(evictHash);
        nEvicted++;
    }

    if (nEvicted > 0)
        printf("LimitOrphanBlocks: evicted %u orphan(s), %u remain\n",
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

    int64_t nSubsidy = nCoinAge * nRewardCoinYear / 365 / COIN;


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

    if (pindexLast == NULL)
        return bnTargetLimit.GetCompact(); // genesis block

    const CBlockIndex* pindexPrev = GetLastBlockIndex(pindexLast, fProofOfStake);
    if (pindexPrev == NULL)
        return bnTargetLimit.GetCompact(); // no previous block of this type
    if (pindexPrev->pprev == NULL)
        return bnTargetLimit.GetCompact(); // first block
    const CBlockIndex* pindexPrevPrev = GetLastBlockIndex(pindexPrev->pprev, fProofOfStake);
    if (pindexPrevPrev == NULL)
        return bnTargetLimit.GetCompact(); // no second previous block of this type
    if (pindexPrevPrev->pprev == NULL)
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
    if (pindexLast != NULL && pindexLast->nHeight + 1 == FORK_HEIGHT_V5 && fProofOfStake)
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

bool IsInitialBlockDownload()
{
    if (pindexBest == NULL || nBestHeight < Checkpoints::GetTotalBlocksEstimate())
        return true;
    static int64_t nLastUpdate;
    static CBlockIndex* pindexLastBest;
    if (pindexBest != pindexLastBest)
    {
        pindexLastBest = pindexBest;
        nLastUpdate = GetTime();
    }
    return (GetTime() - nLastUpdate < 10 &&
            pindexBest->GetBlockTime() < GetTime() - 24 * 60 * 60);
}

void static InvalidChainFound(CBlockIndex* pindexNew)
{
    if (pindexNew->nChainTrust > nBestInvalidTrust)
    {
        nBestInvalidTrust = pindexNew->nChainTrust;
        CTxDB().WriteBestInvalidTrust(CBigNum(nBestInvalidTrust));
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











bool CTransaction::DisconnectInputs(CTxDB& txdb)
{
    // Remove transaction position index entry.
    // UTXO undo (restoring spent outputs, removing created outputs) is
    // handled by DisconnectBlock's UTXO section.
    txdb.EraseTxIndex(*this);

    return true;
}


bool CTransaction::FetchInputs(CTxDB& txdb, const MapPrevTx& mapPendingUtxos,
                               bool fBlock, bool fMiner, MapPrevTx& inputsRet, bool& fInvalid)
{
    // FetchInputs can return false either because we just haven't seen some inputs
    // (in which case the transaction should be stored as an orphan)
    // or because the transaction is malformed (in which case the transaction should
    // be dropped).  If tx is definitely invalid, fInvalid will be set to true.
    fInvalid = false;

    if (IsCoinBase())
        return true; // Coinbase transactions have no inputs to fetch.

    for (unsigned int i = 0; i < vin.size(); i++)
    {
        COutPoint prevout = vin[i].prevout;
        if (inputsRet.count(prevout))
            continue; // Got it already

        // Check pending UTXOs from earlier transactions in the same block
        MapPrevTx::const_iterator mi = mapPendingUtxos.find(prevout);
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
                            std::map<uint256, CBlockIndex*>::iterator bmi = mapBlockIndex.find(blockHeader.GetHash());
                            if (bmi != mapBlockIndex.end())
                                backfill.nHeight = bmi->second->nHeight;
                        }

                        // Check if this output was already spent (vSpent in old format)
                        if (prevout.n < txindex.vSpent.size() && !txindex.vSpent[prevout.n].IsNull())
                        {
                            // Already spent — don't return it as available
                        }
                        else
                        {
                            // Backfill to UTXO DB for future lookups
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
    for (unsigned int i = 0; i < vin.size(); i++)
    {
        MapPrevTx::const_iterator mi = inputs.find(vin[i].prevout);
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
    for (unsigned int i = 0; i < vin.size(); i++)
    {
        MapPrevTx::const_iterator mi = inputs.find(vin[i].prevout);
        if (mi == inputs.end())
            continue;
        const CScript& scriptPubKey = mi->second.scriptPubKey;
        if (scriptPubKey.IsPayToScriptHash())
            nSigOps += scriptPubKey.GetSigOpCount(vin[i].scriptSig);
    }
    return nSigOps;
}

bool CTransaction::ConnectInputs(CTxDB& txdb, const MapPrevTx& inputs,
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
        for (unsigned int i = 0; i < vin.size(); i++)
        {
            COutPoint prevout = vin[i].prevout;
            MapPrevTx::const_iterator mi = inputs.find(prevout);
            if (mi == inputs.end())
                return DoS(100, error("ConnectInputs() : %s input %s:%d not found", GetHash().ToString().substr(0,10).c_str(), prevout.hash.ToString().substr(0,10).c_str(), prevout.n));
            const CUtxoEntry& entry = mi->second;

            // If prev is coinbase or coinstake, check that it's matured
            if (entry.fCoinBase || entry.fCoinStake)
            {
                if (pindexBlock->nHeight - entry.nHeight < nCoinbaseMaturity)
                    return error("ConnectInputs() : tried to spend %s at depth %d", entry.fCoinBase ? "coinbase" : "coinstake", pindexBlock->nHeight - entry.nHeight);
            }

            // triangles: check transaction timestamp
            if (entry.nTxTime > nTime)
                return DoS(100, error("ConnectInputs() : transaction timestamp earlier than input transaction"));

            // Check for negative or overflow input values
            nValueIn += entry.nValue;
            if (!MoneyRange(entry.nValue) || !MoneyRange(nValueIn))
                return DoS(100, error("ConnectInputs() : txin values out of range"));
        }

        // The first loop above does all the inexpensive checks.
        // Only if ALL inputs pass do we perform expensive ECDSA signature checks.
        // Helps prevent CPU exhaustion attacks.
        for (unsigned int i = 0; i < vin.size(); i++)
        {
            COutPoint prevout = vin[i].prevout;
            const CUtxoEntry& entry = inputs.find(prevout)->second;

            // Skip ECDSA signature verification when connecting blocks (fBlock=true)
            // before the last blockchain checkpoint. This is safe because block merkle hashes are
            // still computed and checked, and any change will be caught at the next checkpoint.
            if (!(fBlock && (nBestHeight < Checkpoints::GetTotalBlocksEstimate())))
            {
                if (pvChecks)
                {
                    pvChecks->push_back(CScriptCheck(entry.scriptPubKey, vin[i].scriptSig, *this, i, 0));
                }
                else
                {
                    // Verify signature using scriptPubKey from UTXO entry
                    if (!VerifyScript(vin[i].scriptSig, entry.scriptPubKey, *this, i, 0))
                        return DoS(100, error("ConnectInputs() : %s VerifySignature failed", GetHash().ToString().substr(0,10).c_str()));
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

bool CBlock::DisconnectBlock(CTxDB& txdb, CBlockIndex* pindex)
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

bool CBlock::ConnectBlock(CTxDB& txdb, CBlockIndex* pindex, bool fJustCheck)
{
    // Check it again in case a previous version let a bad block in, but skip BlockSig checking
    if (!CheckBlock(!fJustCheck, !fJustCheck, false))
        return false;

    // Determine if this block is covered by the hardcoded checkpoint.
    // Below checkpoint: skip all input validation, FetchInputs, ConnectInputs,
    // and wallet sync. The checkpoint hash guarantees chain integrity for these blocks.
    bool fAssumeValid = (pindex->nHeight <= Checkpoints::GetTotalBlocksEstimate());
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
    CCheckQueueControl<CScriptCheck> scriptcheckcontrol(pScriptCheckQueue);
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
                    MapPrevTx::iterator it = mapPendingUtxos.find(txin.prevout);
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
                                  pScriptCheckQueue ? &vChecks : NULL))
                return false;
            if (pScriptCheckQueue && vChecks.size() >= 128)
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

            if (nStakeReward > nCalculatedStakeReward)
                return DoS(100, error("ConnectBlock() : coinstake pays too much(actual=%" PRId64 " vs calculated=%" PRId64 ")", nStakeReward, nCalculatedStakeReward));
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
    for (map<uint256, CTxIndex>::iterator mi = mapQueuedChanges.begin(); mi != mapQueuedChanges.end(); ++mi)
    {
        if (!txdb.UpdateTxIndex((*mi).first, (*mi).second))
            return error("ConnectBlock() : UpdateTxIndex failed");
    }

    // Write UTXO database entries: add new outputs, erase spent inputs.
    // Runs for both fAssumeValid (fast) and full validation paths.
    for (unsigned int i = 0; i < vtx.size(); i++)
    {
        const CTransaction& tx = vtx[i];
        uint256 hashTx = tx.GetHash();

        // Add new outputs to UTXO set
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

        // Erase spent inputs from UTXO set
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
                                // Remove spent UTXO
                                txdb.EraseAddressUtxo(nType, hashBytes, txin.prevout.hash, txin.prevout.n);
                                // Decrease balance
                                int64_t nBalance = 0;
                                txdb.ReadAddressBalance(nType, hashBytes, nBalance);
                                nBalance -= prevout.nValue;
                                txdb.WriteAddressBalance(nType, hashBytes, nBalance);
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
                    // Increase balance
                    int64_t nBalance = 0;
                    txdb.ReadAddressBalance(nType, hashBytes, nBalance);
                    nBalance += txout.nValue;
                    txdb.WriteAddressBalance(nType, hashBytes, nBalance);
                    // Record tx in address history
                    txdb.WriteAddressTxId(nType, hashBytes, pindex->nHeight, i, txhash);
                }
            }
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

bool static Reorganize(CTxDB& txdb, CBlockIndex* pindexNew)
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
    for (unsigned int i = 0; i < vConnect.size(); i++)
    {
        CBlockIndex* pindex = vConnect[i];
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
            pindex->pprev->pnext = NULL;

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
bool CBlock::SetBestChainInner(CTxDB& txdb, CBlockIndex *pindexNew)
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

bool CBlock::SetBestChain(CTxDB& txdb, CBlockIndex* pindexNew)
{
    uint256 hash = GetHash();

    if (!txdb.TxnBegin())
        return error("SetBestChain() : TxnBegin failed");

    if (pindexGenesisBlock == NULL && hash == (!fTestNet ? hashGenesisBlockOfficial : hashGenesisBlockTestNet))
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
    pblockindexFBBHLast = NULL;
    nBestHeight = pindexBest->nHeight;
    nBestChainTrust = pindexNew->nChainTrust;
    nTimeBestReceived = GetTime();
    nTransactionsUpdated++;

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
        for (int i = 0; i < 100 && pindex != NULL; i++)
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

    std::string strCmd = GetArg("-blocknotify", "");

    if (!fIsInitialDownload && !strCmd.empty())
    {
        boost::replace_all(strCmd, "%s", hashBestChain.GetHex());
        boost::thread t(runCommand, strCmd); // thread runs free
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

            if (fStartPostIbdWork && !NewThread(ThreadPostIbdWork, NULL))
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
bool CTransaction::GetCoinAge(CTxDB& txdb, uint64_t& nCoinAge) const
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

    CTxDB txdb("r");
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
    map<uint256, CBlockIndex*>::iterator miPrev = mapBlockIndex.find(hashPrevBlock);
    if (miPrev != mapBlockIndex.end())
    {
        pindexNew->pprev = (*miPrev).second;
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
    map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.insert(make_pair(hash, pindexNew)).first;
    if (pindexNew->IsProofOfStake())
        setStakeSeen.insert(make_pair(pindexNew->prevoutStake, pindexNew->nStakeTime));
    pindexNew->phashBlock = &((*mi).first);

    // Write to disk block index
    CTxDB txdb;
    if (!txdb.TxnBegin())
        return false;
    txdb.WriteBlockIndex(CDiskBlockIndex(pindexNew));

    // New best — keep the batch open so SetBestChain can add ConnectBlock
    // writes to the same transaction, cutting the per-block commit count in half.
    //
    // Chain selection rules:
    //   1. Strictly greater trust always wins (normal case).
    //   2. Equal trust: deterministic hash tiebreaker — lower tip hash wins.
    //      This ensures all nodes converge on the same chain even when two
    //      forks have identical cumulative difficulty (common in PoS).
    //      Rate-limited to one equal-trust reorg per 10 minutes to prevent
    //      oscillation from Tor-latency-induced competing announcements.
    bool fNewBest = false;
    static int64_t nLastEqualTrustReorg = 0;
    if (pindexNew->nChainTrust > nBestChainTrust)
        fNewBest = true;
    else if (pindexNew->nChainTrust == nBestChainTrust && pindexBest &&
             pindexNew->GetBlockHash() < pindexBest->GetBlockHash() &&
             GetTime() - nLastEqualTrustReorg > 10 * 60)
    {
        fNewBest = true;
        nLastEqualTrustReorg = GetTime();
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
    if (GetBlockTime() > FutureDrift((int64_t)vtx[0].nTime))
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
    map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.find(hashPrevBlock);
    if (mi == mapBlockIndex.end())
        return DoS(10, error("AcceptBlock() : prev block not found"));
    CBlockIndex* pindexPrev = (*mi).second;
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
        // Skip expensive PoS kernel verification for blocks covered by hardcoded checkpoint.
        // The checkpoint at height 2,186,940 already guarantees chain integrity.
        if (nHeight > Checkpoints::GetTotalBlocksEstimate())
        {
            if (!CheckProofOfStake(vtx[1], nBits, hashProofOfStake, targetProofOfStake))
            {
                printf("WARNING: ProcessBlock(): check proof-of-stake failed for block %s\n", hash.ToString().c_str());
                return false; // do not error here as we expect this during initial block download
            }
        }
    }

    // Sync checkpoint enforcement is disabled:
    // - Master key was removed in V5 fork, no new sync checkpoints will be broadcast
    // - Hardcoded checkpoints already guarantee chain integrity
    // - The persisted hashSyncCheckpoint in LevelDB blocks IBD from progressing

    // Enforce rule that the coinbase starts with serialized block height
    CScript expect = CScript() << nHeight;
    if (vtx[0].vin[0].scriptSig.size() < expect.size() ||
        !std::equal(expect.begin(), expect.end(), vtx[0].vin[0].scriptSig.begin()))
        return DoS(100, error("AcceptBlock() : block height mismatch in coinbase"));

    // Write block to history file
    if (!CheckDiskSpace(::GetSerializeSize(*this, SER_DISK, CLIENT_VERSION)))
        return error("AcceptBlock() : out of disk space");
    unsigned int nFile = -1;
    unsigned int nBlockPos = 0;
    if (!WriteToDisk(nFile, nBlockPos))
        return error("AcceptBlock() : WriteToDisk failed");
    if (!AddToBlockIndex(nFile, nBlockPos, hashProofOfStake))
        return error("AcceptBlock() : AddToBlockIndex failed");

    // Relay inventory, but don't relay old inventory during initial block download
    int nBlockEstimate = Checkpoints::GetTotalBlocksEstimate();
    if (hashBestChain == hash)
    {
        LOCK(cs_vNodes);
        for (CNode* pnode : vNodes)
            if (nBestHeight > (pnode->nStartingHeight != -1 ? pnode->nStartingHeight - 2000 : nBlockEstimate))
                pnode->PushInventory(CInv(MSG_BLOCK, hash));
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
    for (unsigned int i = 0; i < nToCheck && nFound < nRequired && pstart != NULL; i++)
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
    if (pblock->IsProofOfStake() && setStakeSeen.count(pblock->GetProofOfStake()) && !mapOrphanBlocksByPrev.count(hash))
        return error("ProcessBlock() : duplicate proof-of-stake (%s, %d) for block %s", pblock->GetProofOfStake().first.ToString().c_str(), pblock->GetProofOfStake().second, hash.ToString().c_str());

    // Preliminary checks
    // Skip block signature verification during initial block download (below checkpoint).
    // The hardcoded checkpoint guarantees historical chain integrity.
    if (!pblock->CheckBlock(true, true, !IsInitialBlockDownload()))
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

        if (bnRequired != 0 && bnNewBlock > bnRequired)
        {
            if (pfrom)
                pfrom->Misbehaving(100);
            return error("ProcessBlock() : block with too little %s", pblock->IsProofOfStake()? "proof-of-stake" : "proof-of-work");
        }
    }

    // If don't already have its previous block, shunt it off to holding area until we get it
    if (!mapBlockIndex.count(pblock->hashPrevBlock))
    {
        printf("ProcessBlock: ORPHAN BLOCK, prev=%s\n", pblock->hashPrevBlock.ToString().substr(0,20).c_str());
        CBlock* pblock2 = new CBlock(*pblock);
        // triangles: check proof-of-stake
        if (pblock2->IsProofOfStake())
        {
            // Limited duplicity on stake: prevents block flood attack
            // Duplicate stake allowed only when there is orphan child block
            if (setStakeSeenOrphan.count(pblock2->GetProofOfStake()) && !mapOrphanBlocksByPrev.count(hash))
                return error("ProcessBlock() : duplicate proof-of-stake (%s, %d) for orphan block %s", pblock2->GetProofOfStake().first.ToString().c_str(), pblock2->GetProofOfStake().second, hash.ToString().c_str());
            else
                setStakeSeenOrphan.insert(pblock2->GetProofOfStake());
        }
        mapOrphanBlocks.insert(make_pair(hash, pblock2));
        mapOrphanBlocksByPrev.insert(make_pair(pblock2->hashPrevBlock, pblock2));

        // Limit orphan blocks to prevent memory exhaustion.
        // Allow more orphans during IBD so out-of-order blocks from parallel
        // downloads don't get evicted and re-requested.
        unsigned int nMaxOrphans = IsInitialBlockDownload() ? MAX_ORPHAN_BLOCKS_IBD : MAX_ORPHAN_BLOCKS;
        LimitOrphanBlocks(nMaxOrphans);

        // Ask this guy to fill in what we're missing
        if (pfrom && pindexBest)
        {
            pfrom->PushGetBlocks(pindexBest, GetOrphanRoot(pblock2));
            // triangles: getblocks may not obtain the ancestor block rejected
            // earlier by duplicate-stake check so we ask for it again directly
            if (!IsInitialBlockDownload())
                pfrom->AskFor(CInv(MSG_BLOCK, WantedByOrphan(pblock2)));
        }
        return true;
    }

    // Store to disk
    if (!pblock->AcceptBlock())
        return error("ProcessBlock() : AcceptBlock FAILED");

    MarkHeaderSyncBlockAccepted(hash);

    // Recursively process any orphan blocks that depended on this one
    vector<uint256> vWorkQueue;
    vWorkQueue.push_back(hash);
    for (unsigned int i = 0; i < vWorkQueue.size(); i++)
    {
        uint256 hashPrev = vWorkQueue[i];
        for (multimap<uint256, CBlock*>::iterator mi = mapOrphanBlocksByPrev.lower_bound(hashPrev);
             mi != mapOrphanBlocksByPrev.upper_bound(hashPrev);
             ++mi)
        {
            CBlock* pblockOrphan = (*mi).second;
            if (pblockOrphan->AcceptBlock())
            {
                vWorkQueue.push_back(pblockOrphan->GetHash());
                MarkHeaderSyncBlockAccepted(pblockOrphan->GetHash());
            }
            mapOrphanBlocks.erase(pblockOrphan->GetHash());
            setStakeSeenOrphan.erase(pblockOrphan->GetProofOfStake());
            delete pblockOrphan;
        }
        mapOrphanBlocksByPrev.erase(hashPrev);
    }

    if (nBestHeight % 5000 == 0 || !IsInitialBlockDownload())
        printf("ProcessBlock: ACCEPTED block %d\n", nBestHeight);

    if (hashBestHeaderSync != 0)
    {
        // Use parallel queue to distribute across all peers
        const unsigned int nQueued = QueueHeaderSyncBlocksParallel(HEADER_DOWNLOAD_WINDOW);
        if (nQueued > 0)
            printf("IBD-DIAG: queued %u more blocks from header planner after accepting %s\n",
                nQueued, hash.ToString().substr(0,20).c_str());
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
    txnouttype whichType;

    const CTxOut& txout = vtx[1].vout[1];

    if (!Solver(txout.scriptPubKey, whichType, vSolutions))
        return false;

    if (whichType == TX_PUBKEY)
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
        return NULL;
    FILE* file = fopen(BlockFilePath(nFile).string().c_str(), pszMode);
    if (!file)
        return NULL;
    if (nBlockPos != 0 && !strchr(pszMode, 'a') && !strchr(pszMode, 'w'))
    {
        if (fseek(file, nBlockPos, SEEK_SET) != 0)
        {
            fclose(file);
            return NULL;
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
            return NULL;
        if (fseek(file, 0, SEEK_END) != 0)
            return NULL;
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
    CTxDB txdb("cr+");
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
            return error("LoadBlockIndex() : failed to reset sync-checkpoint");
    }

    return true;
}



void PrintBlockTree()
{
    // pre-compute tree structure
    map<CBlockIndex*, vector<CBlockIndex*> > mapNext;
    for (map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.begin(); mi != mapBlockIndex.end(); ++mi)
    {
        CBlockIndex* pindex = (*mi).second;
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
                    else if (ProcessBlock(NULL,&block))
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

        CTxDB txdb;
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
            map<uint256, CBlockIndex*>::iterator miPrev = mapBlockIndex.find(block.hashPrevBlock);
            if (miPrev != mapBlockIndex.end())
            {
                pindexNew->pprev = (*miPrev).second;
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
            map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.insert(make_pair(hash, pindexNew)).first;
            pindexNew->phashBlock = &((*mi).first);

            // Link pnext for previous block
            if (pindexNew->pprev)
                pindexNew->pprev->pnext = pindexNew;

            // Build tx index + UTXO entries, tracking money supply
            int64_t nBlockValueIn = 0;
            int64_t nBlockValueOut = 0;
            int64_t nFees = 0;
            unsigned int nTxPos = nBlockPos + ::GetSerializeSize(CBlock(), SER_DISK, CLIENT_VERSION)
                                - (2 * GetSizeOfCompactSize(0)) + GetSizeOfCompactSize(block.vtx.size());
            for (unsigned int i = 0; i < block.vtx.size(); i++)
            {
                const CTransaction& tx = block.vtx[i];
                uint256 hashTx = tx.GetHash();
                CDiskTxPos posThisTx(1, nBlockPos, nTxPos);
                txdb.UpdateTxIndex(hashTx, CTxIndex(posThisTx, tx.vout.size()));
                nTxPos += ::GetSerializeSize(tx, SER_DISK, CLIENT_VERSION);

                int64_t nTxValueOut = tx.GetValueOut();
                nBlockValueOut += nTxValueOut;

                // UTXO entries — read input values before erasing for money supply
                if (!tx.IsCoinBase())
                {
                    int64_t nTxValueIn = 0;
                    for (const CTxIn& txin : tx.vin)
                    {
                        CUtxoEntry utxo;
                        if (txdb.ReadUtxo(txin.prevout.hash, txin.prevout.n, utxo))
                            nTxValueIn += utxo.nValue;
                        txdb.EraseUtxo(txin.prevout.hash, txin.prevout.n);
                    }
                    nBlockValueIn += nTxValueIn;
                    if (!tx.IsCoinStake())
                        nFees += nTxValueIn - nTxValueOut;
                }
                for (unsigned int k = 0; k < tx.vout.size(); k++)
                {
                    if (!tx.vout[k].IsEmpty())
                    {
                        CUtxoEntry utxo;
                        utxo.nValue = tx.vout[k].nValue;
                        utxo.nHeight = pindexNew->nHeight;
                        utxo.scriptPubKey = tx.vout[k].scriptPubKey;
                        utxo.fCoinBase = tx.IsCoinBase();
                        utxo.fCoinStake = tx.IsCoinStake();
                        utxo.nTxTime = tx.nTime;
                        txdb.WriteUtxo(hashTx, k, utxo);
                    }
                }
            }

            // Money supply tracking — matches ConnectBlock formula
            pindexNew->nMint = nBlockValueOut - nBlockValueIn + nFees;
            pindexNew->nMoneySupply = (pindexNew->pprev ? pindexNew->pprev->nMoneySupply : 0) + nBlockValueOut - nBlockValueIn;

            // Write block index to batch
            txdb.WriteBlockIndex(CDiskBlockIndex(pindexNew));

            // Update best chain
            if (pindexNew->nChainTrust > nBestChainTrust)
            {
                hashBestChain = hash;
                pindexBest = pindexNew;
                pblockindexFBBHLast = NULL;
                nBestHeight = pindexNew->nHeight;
                nBestChainTrust = pindexNew->nChainTrust;
                nTimeBestReceived = GetTime();
            }

            // Set genesis block
            if (pindexGenesisBlock == NULL && pindexNew->nHeight == 0)
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

//////////////////////////////////////////////////////////////////////////////
//
// CAlert
//

extern map<uint256, CAlert> mapAlerts;
extern CCriticalSection cs_mapAlerts;

string GetWarnings(string strFor)
{
    int nPriority = 0;
    string strStatusBar;
    string strRPC;

    if (GetBoolArg("-testsafemode"))
        strRPC = "test";

    // Misc warnings like out of disk space and clock is wrong
    if (strMiscWarning != "")
    {
        nPriority = 1000;
        strStatusBar = strMiscWarning;
    }

    // triangles: if detected invalid checkpoint enter safe mode
    if (Checkpoints::hashInvalidCheckpoint != 0)
    {
        nPriority = 3000;
        strStatusBar = strRPC = _("WARNING: Invalid checkpoint found! Displayed transactions may not be correct! You may need to upgrade, or notify developers.");
    }

    // Alerts
    {
        LOCK(cs_mapAlerts);
        for (auto& item : mapAlerts)
        {
            const CAlert& alert = item.second;
            if (alert.AppliesToMe() && alert.nPriority > nPriority)
            {
                nPriority = alert.nPriority;
                strStatusBar = alert.strStatusBar;
                if (nPriority > 1000)
                    strRPC = strStatusBar;  // triangles: safe mode for high alert
            }
        }
    }

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


bool static AlreadyHave(CTxDB& txdb, const CInv& inv)
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
    static map<CService, CPubKey> mapReuseKey;
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

            // Get recent addresses
            if (pfrom->fOneShot || pfrom->nVersion >= CADDR_TIME_VERSION || addrman.size() < 1000)
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
        }

        // Ask connected nodes for block updates.
        // During IBD, request blocks from every valid peer to maximize download
        // parallelism. Multiple peers sending overlapping inv ranges is harmless
        // (AlreadyHave filters duplicates) but ensures we discover and download
        // blocks from the fastest available source.
        static int nAskedForBlocks = 0;
        bool fIBD = IsInitialBlockDownload();
        bool fBehindPeer = (pfrom->nStartingHeight > nBestHeight);
        bool fShouldAsk = !pfrom->fClient && !pfrom->fOneShot &&
            (pfrom->nStartingHeight > (nBestHeight - 144)) &&
            (pfrom->nVersion < NOBLKS_VERSION_START ||
             pfrom->nVersion >= NOBLKS_VERSION_END) &&
             (fIBD || nAskedForBlocks < 1 || vNodes.size() <= 1 || fBehindPeer);
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
                pfrom->PushGetHeaders(pindexBest, uint256(0));
            printf("IBD-DIAG: sent getblocks%s from height %d to peer %s\n",
                fIBD ? "+getheaders" : "", nBestHeight, pfrom->addr.ToString().c_str());
        }

        // Relay alerts
        {
            LOCK(cs_mapAlerts);
            for (auto& item : mapAlerts)
                item.second.RelayTo(pfrom);
        }

        // Sync checkpoint relay disabled (master key removed in V5 fork).
        // Relaying stale checkpoints causes IBD nodes to request far-future blocks.

        pfrom->fSuccessfullyConnected = true;

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
                    for (multimap<uint256, CNode*>::iterator mi = mapMix.begin(); mi != mapMix.end() && nRelayNodes-- > 0; ++mi)
                        ((*mi).second)->PushAddress(addr);
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

        CTxDB txdb("r");
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
                    std::map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.find(inv.hash);
                    if (mi != mapBlockIndex.end()) {
                        int h = mi->second->nHeight;
                        if (nFirstInvHeight == -1) nFirstInvHeight = h;
                        nLastInvHeight = h;
                        if (h > nBestHeight) nAboveBest++;
                    }
                } else {
                    nNew++;
                }
            }

            if (!fAlreadyHave)
                pfrom->AskFor(inv);
            else if (inv.type == MSG_BLOCK && mapOrphanBlocks.count(inv.hash)) {
                pfrom->PushGetBlocks(pindexBest, GetOrphanRoot(mapOrphanBlocks[inv.hash]));
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
                pfrom->pindexLastGetBlocksBegin = NULL; // reset dedup
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

            if (inv.type == MSG_BLOCK)
            {
                // Send block from disk
                map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.find(inv.hash);
                if (mi != mapBlockIndex.end())
                {
                    CBlock block;
                    block.ReadFromDisk((*mi).second);
                    pfrom->PushMessage("block", block);

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
                    map<CInv, CDataStream>::iterator mi = mapRelay.find(inv);
                    if (mi != mapRelay.end()) {
                        pfrom->PushMessage(inv.GetCommand(), (*mi).second);
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
    else if (strCommand == "checkpoint")
    {
        // Sync checkpoint system disabled (master key removed in V5 fork).
        // Ignore checkpoint messages — processing them during IBD causes the
        // node to request a single far-future block instead of syncing sequentially.
    }

    else if (strCommand == "getheaders")
    {
        CBlockLocator locator;
        uint256 hashStop;
        vRecv >> locator >> hashStop;

        CBlockIndex* pindex = NULL;
        if (locator.IsNull())
        {
            // If locator is null, return the hashStop block
            map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.find(hashStop);
            if (mi == mapBlockIndex.end())
                return true;
            pindex = (*mi).second;
        }
        else
        {
            // Find the last block the caller has in the main chain
            pindex = locator.GetBlockIndex();
            if (pindex)
                pindex = pindex->pnext;
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
        if (vHeaders.size() > 2000)
        {
            pfrom->Misbehaving(20);
            return error("message headers size() = %" PRIszu "", vHeaders.size());
        }

        uint256 hashChainTip = 0;
        int nNewHeaders = 0;
        for (const CBlock& header : vHeaders)
        {
            if (!header.vtx.empty())
            {
                pfrom->Misbehaving(20);
                return error("headers message includes transactions");
            }

            const uint256 hashHeader = header.GetHash();
            if (mapBlockIndex.count(hashHeader) || mapHeaderSync.count(hashHeader))
            {
                hashChainTip = hashHeader;
                continue;
            }

            if (hashChainTip != 0)
            {
                if (header.hashPrevBlock != hashChainTip)
                {
                    pfrom->Misbehaving(20);
                    return error("non-continuous headers sequence");
                }
            }
            else
            {
                map<uint256, CBlockIndex*>::iterator miPrev = mapBlockIndex.find(header.hashPrevBlock);
                if (miPrev == mapBlockIndex.end() && !mapHeaderSync.count(header.hashPrevBlock))
                    break;
            }

            if (!AddHeaderSyncNode(header, hashHeader))
            {
                pfrom->Misbehaving(20);
                return error("invalid header sequence");
            }

            hashChainTip = hashHeader;
            nNewHeaders++;
        }

        int nRequested = 0;
        if (hashBestHeaderSync != 0)
            nRequested = QueueHeaderSyncBlocksParallel(HEADER_DOWNLOAD_WINDOW);

        if (nNewHeaders > 0 || nRequested > 0)
            printf("IBD-DIAG: accepted %d new headers, queued %d blocks from %zu headers (peer=%s bestHeader=%s)\n",
                nNewHeaders, nRequested, vHeaders.size(), pfrom->addr.ToString().c_str(),
                hashBestHeaderSync.ToString().substr(0,20).c_str());

        // If we received a full batch, continue fetching headers.
        // During IBD, prefer getheaders over getblocks since headers are ~80 bytes
        // vs full blocks, letting us discover the chain structure faster.
        if (vHeaders.size() >= 2000)
        {
            if (IsInitialBlockDownload() && hashChainTip != 0)
                ContinueHeaderSync(pfrom, hashChainTip);
            else
                pfrom->PushGetBlocks(pindexBest, uint256(0));
        }
    }


    else if (strCommand == "tx")
    {
        vector<uint256> vWorkQueue;
        vector<uint256> vEraseQueue;
        CDataStream vMsg(vRecv);
        CTxDB txdb("r");
        CTransaction tx;
        vRecv >> tx;

        CInv inv(MSG_TX, tx.GetHash());
        pfrom->AddInventoryKnown(inv);

        bool fMissingInputs = false;
        if (tx.AcceptToMemoryPool(txdb, true, &fMissingInputs))
        {
            SyncWithWallets(tx, NULL, true);
            RelayTransaction(tx, inv.hash);
            mapAlreadyAskedFor.erase(inv);
            vWorkQueue.push_back(inv.hash);
            vEraseQueue.push_back(inv.hash);

            // Recursively process any orphan transactions that depended on this one
            for (unsigned int i = 0; i < vWorkQueue.size(); i++)
            {
                uint256 hashPrev = vWorkQueue[i];
                for (set<uint256>::iterator mi = mapOrphanTransactionsByPrev[hashPrev].begin();
                     mi != mapOrphanTransactionsByPrev[hashPrev].end();
                     ++mi)
                {
                    const uint256& orphanTxHash = *mi;
                    CTransaction& orphanTx = mapOrphanTransactions[orphanTxHash];
                    bool fMissingInputs2 = false;

                    if (orphanTx.AcceptToMemoryPool(txdb, true, &fMissingInputs2))
                    {
                        printf("   accepted orphan tx %s\n", orphanTxHash.ToString().substr(0,10).c_str());
                        SyncWithWallets(tx, NULL, true);
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

        if (ProcessBlock(pfrom, &block))
        {
            mapAlreadyAskedFor.erase(inv);

            if (IsInitialBlockDownload())
            {
                static int nBlocksSinceRequest = 0;
                if (++nBlocksSinceRequest >= 5000)
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
                                pnode->pindexLastGetBlocksBegin = NULL;
                                pnode->PushGetBlocks(pindexBest, uint256(0));
                                pnode->pindexLastGetHeadersBegin = NULL;
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


    else if (strCommand == "checkorder")
    {
        uint256 hashReply;
        vRecv >> hashReply;

        if (!GetBoolArg("-allowreceivebyip"))
        {
            pfrom->PushMessage("reply", hashReply, (int)2, string(""));
            return true;
        }

        CWalletTx order;
        vRecv >> order;

        /// we have a chance to check the order here

        // Keep giving the same key to the same ip until they use it
        if (!mapReuseKey.count(pfrom->addr))
            pwalletMain->GetKeyFromPool(mapReuseKey[pfrom->addr], true);

        // Send back approval of order and pubkey to use
        CScript scriptPubKey;
        scriptPubKey << mapReuseKey[pfrom->addr] << OP_CHECKSIG;
        pfrom->PushMessage("reply", hashReply, (int)0, scriptPubKey);
    }


    else if (strCommand == "reply")
    {
        uint256 hashReply;
        vRecv >> hashReply;

        CRequestTracker tracker;
        {
            LOCK(pfrom->cs_mapRequests);
            map<uint256, CRequestTracker>::iterator mi = pfrom->mapRequests.find(hashReply);
            if (mi != pfrom->mapRequests.end())
            {
                tracker = (*mi).second;
                pfrom->mapRequests.erase(mi);
            }
        }
        if (!tracker.IsNull())
            tracker.fn(tracker.param1, vRecv);
    }


    else if (strCommand == "ping")
    {
        if (pfrom->nVersion > BIP0031_VERSION)
        {
            uint64_t nonce = 0;
            vRecv >> nonce;
            // Echo the message back with the nonce. This allows for two useful features:
            //
            // 1) A remote node can quickly check if the connection is operational
            // 2) Remote nodes can measure the latency of the network thread. If this node
            //    is overloaded it won't respond to pings quickly and the remote node can
            //    avoid sending us more work, like chain download requests.
            //
            // The nonce stops the remote getting confused between different pings: without
            // it, if the remote node sends a ping once per second and this node takes 5
            // seconds to respond to each, the 5th ping the remote sends would appear to
            // return very quickly.
            pfrom->PushMessage("pong", nonce);
        }
    }


    else if (strCommand == "alert")
    {
        CAlert alert;
        vRecv >> alert;

        uint256 alertHash = alert.GetHash();
        if (pfrom->setKnown.count(alertHash) == 0)
        {
            if (alert.ProcessAlert())
            {
                // Relay
                pfrom->setKnown.insert(alertHash);
                {
                    LOCK(cs_vNodes);
                    for (CNode* pnode : vNodes)
                        alert.RelayTo(pnode);
                }
            }
            else {
                // Small DoS penalty so peers that send us lots of
                // duplicate/expired/invalid-signature/whatever alerts
                // eventually get banned.
                // This isn't a Misbehaving(100) (immediate ban) because the
                // peer might be an older or different implementation with
                // a different signature key, etc.
                pfrom->Misbehaving(10);
            }
        }
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
        if (strCommand == "version" || strCommand == "addr" || strCommand == "inv" || strCommand == "getdata" || strCommand == "ping")
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
            PrintExceptionContinue(NULL, "ProcessMessages()");
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

        // Keep-alive ping. We send a nonce of zero because we don't use it anywhere
        // right now.
        if (pto->nLastSend && GetTime() - pto->nLastSend > 30 * 60 && pto->ssSend.empty()) {
            uint64_t nonce = 0;
            if (pto->nVersion > BIP0031_VERSION)
                pto->PushMessage("ping", nonce);
            else
                pto->PushMessage("ping");
        }

        // Resend wallet transactions that haven't gotten in a block yet
        ResendWalletTransactions();

        // Address refresh broadcast
        static int64_t nLastRebroadcast;
        if (!IsInitialBlockDownload() && (GetTime() - nLastRebroadcast > 24 * 60 * 60))
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
            int nStallTimeout = IsInitialBlockDownload() ? 5 : 15;
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
                // Use the walk-forward progress point if available, to avoid
                // restarting from pindexBest (which hits the CBlockLocator
                // exponential gap and starts the walk-forward from scratch).
                pto->pindexLastGetBlocksBegin = NULL;
                if (nHighestInvWalk > nBestHeight && hashHighestInvWalk != 0 &&
                    mapBlockIndex.count(hashHighestInvWalk))
                {
                    pto->PushGetBlocks(mapBlockIndex[hashHighestInvWalk], uint256(0));
                    printf("SYNC-DIAG: stall re-request from walk=%d (not best=%d)\n",
                        nHighestInvWalk, nBestHeight);
                } else {
                    pto->PushGetBlocks(pindexBest, uint256(0));
                }
                nLastBlockReceived = GetTime();
            }
        }

        //
        // Message: getdata
        //
        // Periodic IBD status
        if (IsInitialBlockDownload()) {
            static int64_t nLastStatus = 0;
            if (GetTime() - nLastStatus >= 15) {
                printf("IBD-DIAG: STATUS height=%d peers=%d askfor_queued=%d orphans=%d\n",
                    nBestHeight, (int)vNodes.size(), (int)pto->mapAskFor.size(), (int)mapOrphanBlocks.size());
                nLastStatus = GetTime();
            }
        }

        vector<CInv> vGetData;
        int64_t nNow = GetTime() * 1000000;
        CTxDB txdb("r");
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


