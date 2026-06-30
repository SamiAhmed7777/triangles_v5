// Copyright (c) 2026 The Triangles developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "syncmanager.h"

#include "bignum.h"
#include "checkpoints.h"
#include "main.h"
#include "net.h"
#include "util.h"

#include <algorithm>
#include <map>

struct CSyncManager::HeaderNode
{
    CBlock header;
    int nHeight;
    uint256 nChainTrust;
    bool fRequested;
    int64_t nLastRequestTime;
    int64_t nFirstRequestTime;
    int64_t nInsertTime;
    // Phase 1.5: track which peer this header was last requested from. Used to
    // compute per-peer inflight for the cap. Not a hard ownership — the block
    // can be re-requested from a different peer if this one stalls.
    CNode* pnodeLastRequest = nullptr;
};

namespace
{
static const unsigned int MAX_HEADER_SYNC_CACHE = 100000;
static const size_t HEADER_REDUNDANT_PEER_THRESHOLD = 4;
static const int64_t HEADER_REQUEST_TIMEOUT_MICROS = 60 * 1000000;
static const int64_t HEADER_REDUNDANT_REQUEST_MICROS = 5 * 1000000;
static const int64_t HEADER_SYNC_TTL_MICROS = 15 * 60 * 1000000;
// Backpressure ceiling: how far (in blocks) the header front is allowed to
// run ahead of the connected chain tip before we stop fetching MORE headers
// and let the block planner catch up. Comfortly below MAX_HEADER_SYNC_CACHE
// so the cache never overflows in normal from-zero sync.
static const int HEADER_FRONT_MAX_AHEAD = 32768;

std::map<uint256, CSyncManager::HeaderNode> mapHeaders;
uint256 hashBestHeader = 0;
int64_t nLastNewHeaderTime = 0;

// O(1) in-flight counter — replaces the O(n) scan in CountInFlight().
// Incremented when fRequested transitions false→true; decremented when an
// entry with fRequested==true is erased from mapHeaders.
static size_t g_nInFlight = 0;
}

CSyncManager g_syncManager;

bool CSyncManager::HaveHeader(const uint256& hash) const
{
    return mapHeaders.count(hash) != 0;
}

uint256 CSyncManager::GetBestHeader() const
{
    return hashBestHeader;
}

std::size_t CSyncManager::GetHeaderCount() const
{
    return mapHeaders.size();
}

uint256 CSyncManager::GetHeaderTrust(unsigned int nBits) const
{
    CBigNum bnTarget;
    bnTarget.SetCompact(nBits);

    if (bnTarget <= 0)
        return 0;

    return ((CBigNum(1) << 256) / (bnTarget + 1)).getuint256();
}

bool CSyncManager::GetKnownHeaderState(const uint256& hash, int& nHeight, uint256& nChainTrust) const
{
    std::map<uint256, CBlockIndex*>::const_iterator miBlock = mapBlockIndex.find(hash);
    if (miBlock != mapBlockIndex.end())
    {
        nHeight = miBlock->second->nHeight;
        nChainTrust = miBlock->second->nChainTrust;
        return true;
    }

    std::map<uint256, HeaderNode>::const_iterator miHeader = mapHeaders.find(hash);
    if (miHeader != mapHeaders.end())
    {
        nHeight = miHeader->second.nHeight;
        nChainTrust = miHeader->second.nChainTrust;
        return true;
    }

    return false;
}

bool CSyncManager::GetPrevHash(const uint256& hash, uint256& hashPrev) const
{
    std::map<uint256, HeaderNode>::const_iterator miHeader = mapHeaders.find(hash);
    if (miHeader != mapHeaders.end())
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

void CSyncManager::RecomputeBestHeader()
{
    hashBestHeader = 0;
    uint256 nBestTrust = 0;

    for (std::map<uint256, HeaderNode>::const_iterator it = mapHeaders.begin(); it != mapHeaders.end(); ++it)
    {
        if (hashBestHeader == 0 || it->second.nChainTrust > nBestTrust)
        {
            hashBestHeader = it->first;
            nBestTrust = it->second.nChainTrust;
        }
    }
}

void CSyncManager::PruneHeaders()
{
    const int64_t nNow = GetTime() * 1000000;

    // Never evict headers in the live sync window. Fix 2's backpressure caps the
    // header front at nBestHeight + HEADER_FRONT_MAX_AHEAD, so protecting that
    // whole span means the entire in-flight header chain is safe: no mid-chain
    // hole can form between the connected tip and the front during normal sync.
    // Evicting any of these would sever GetDownloadPath() from the connected
    // chain and freeze sync. (The hard cap below is the memory safety valve;
    // Fix 3's bridge-repair is the backstop for restart/reorg edge cases.)
    const int nProtectFloor = nBestHeight + HEADER_FRONT_MAX_AHEAD;

    // TTL pass: evict aged headers, but only ABOVE the protected floor.
    if (mapHeaders.size() > MAX_HEADER_SYNC_CACHE / 2)
    {
        unsigned int nEvicted = 0;
        for (std::map<uint256, HeaderNode>::iterator it = mapHeaders.begin(); it != mapHeaders.end(); )
        {
            if (it->second.nHeight > nProtectFloor &&
                nNow - it->second.nInsertTime >= HEADER_SYNC_TTL_MICROS)
            {
                if (it->second.fRequested)
                    --g_nInFlight;
                it = mapHeaders.erase(it);
                ++nEvicted;
            }
            else
                ++it;
        }
        if (nEvicted > 0)
        {
            printf("IBD-DIAG: TTL-evicted %u stale sync headers above floor %d, %u remain\n",
                nEvicted, nProtectFloor, (unsigned int)mapHeaders.size());
            RecomputeBestHeader();
        }
    }

    // Hard cap (last-resort safety valve): evict the HIGHEST-height headers
    // first — the ones furthest ahead of the tip — never the bridge zone.
    // Lowering the sync target temporarily is fine; we re-extend it once blocks
    // catch up. Losing a bridge header is not fine: it stalls forever.
    if (mapHeaders.size() > MAX_HEADER_SYNC_CACHE)
    {
        printf("IBD-DIAG: sync header cache exceeded %u entries, evicting from the front (floor=%d)\n",
            MAX_HEADER_SYNC_CACHE, nProtectFloor);

        std::vector<std::map<uint256, HeaderNode>::iterator> vEvictable;
        for (std::map<uint256, HeaderNode>::iterator it = mapHeaders.begin(); it != mapHeaders.end(); ++it)
            if (it->second.nHeight > nProtectFloor)
                vEvictable.push_back(it);

        std::sort(vEvictable.begin(), vEvictable.end(),
            [](const std::map<uint256, HeaderNode>::iterator& a,
               const std::map<uint256, HeaderNode>::iterator& b) {
                return a->second.nHeight > b->second.nHeight;
            });

        const size_t nTarget = (size_t)MAX_HEADER_SYNC_CACHE * 3 / 4;
        size_t i = 0;
        while (mapHeaders.size() > nTarget && i < vEvictable.size())
        {
            if (vEvictable[i]->second.fRequested)
                --g_nInFlight;
            mapHeaders.erase(vEvictable[i++]);
        }

        RecomputeBestHeader();
    }
}

bool CSyncManager::AddHeaderNode(const CBlock& header, const uint256& hashHeader)
{
    if (mapBlockIndex.count(hashHeader) || mapHeaders.count(hashHeader))
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
    // Only check PoW on actual PoW headers (nNonce != 0).
    // Triangles is a hybrid PoW/PoS coin — PoS blocks (nonce=0) can appear
    // even within the 0-9000 PoW range.  Checking PoW on a PoS header
    // rejects valid blocks and severs the header chain during IBD.
    if (nHeight <= CUTOFF_POW_BLOCK && header.nNonce != 0 && !CheckProofOfWork(hashHeader, header.nBits))
    {
        printf("IBD-DIAG: header PoW FAILED at height %d hash=%s nBits=%08x prevHash=%s\n",
            nHeight, hashHeader.ToString().substr(0,20).c_str(), header.nBits,
            header.hashPrevBlock.ToString().substr(0,20).c_str());
        return false;
    }

    HeaderNode node;
    node.header = header;
    node.nHeight = nHeight;
    node.nChainTrust = nPrevChainTrust + GetHeaderTrust(header.nBits);
    node.fRequested = false;
    node.nLastRequestTime = 0;
    node.nFirstRequestTime = 0;
    node.nInsertTime = GetTime() * 1000000;

    mapHeaders.insert({hashHeader, node});

    if (hashBestHeader == 0 || node.nChainTrust > mapHeaders[hashBestHeader].nChainTrust)
        hashBestHeader = hashHeader;

    PruneHeaders();
    return true;
}

std::vector<uint256> CSyncManager::GetDownloadPath(uint256 hashTip) const
{
    std::vector<uint256> vPath;

    while (hashTip != 0 && !mapBlockIndex.count(hashTip))
    {
        std::map<uint256, HeaderNode>::const_iterator mi = mapHeaders.find(hashTip);
        if (mi == mapHeaders.end())
            break;

        vPath.push_back(hashTip);
        hashTip = mi->second.header.hashPrevBlock;
    }

    std::reverse(vPath.begin(), vPath.end());
    return vPath;
}

// A download path is "anchored" when its lowest header's parent is a block we
// already have in the active chain (mapBlockIndex). If it isn't, a bridging
// header was lost and requesting these blocks would only create orphans —
// Tick() rebuilds the bridge with a getheaders anchored at pindexBest.
bool CSyncManager::PathReachesChain(const std::vector<uint256>& vPath) const
{
    if (vPath.empty())
        return true; // nothing queued == nothing to bridge
    std::map<uint256, HeaderNode>::const_iterator mi = mapHeaders.find(vPath.front());
    if (mi == mapHeaders.end())
        return false;
    return mapBlockIndex.count(mi->second.header.hashPrevBlock) != 0;
}

unsigned int CSyncManager::CountInFlight() const
{
    return (unsigned int)g_nInFlight;
}

unsigned int CSyncManager::GetPlannerDepth() const
{
    if (hashBestHeader == 0)
        return 0;

    return (unsigned int)GetDownloadPath(hashBestHeader).size();
}

int CSyncManager::GetPlannerHeight() const
{
    if (hashBestHeader == 0)
        return pindexBest ? pindexBest->nHeight : -1;

    std::map<uint256, HeaderNode>::const_iterator mi = mapHeaders.find(hashBestHeader);
    if (mi == mapHeaders.end())
        return pindexBest ? pindexBest->nHeight : -1;

    return mi->second.nHeight;
}

int64_t CSyncManager::GetRequestTime(const uint256& hashBlock) const
{
    std::map<uint256, HeaderNode>::const_iterator mi = mapHeaders.find(hashBlock);
    if (mi == mapHeaders.end())
        return 0;
    return mi->second.nFirstRequestTime;
}

void CSyncManager::BlockAccepted(const uint256& hashBlock)
{
    std::map<uint256, HeaderNode>::iterator mi = mapHeaders.find(hashBlock);
    if (mi == mapHeaders.end())
        return;

    if (mi->second.fRequested)
        --g_nInFlight;
    mapHeaders.erase(mi);
    if (hashBestHeader == hashBlock)
        RecomputeBestHeader();
}

void CSyncManager::ContinueHeaders(CNode* pfrom, const uint256& hashTip)
{
    if (!pfrom || hashTip == 0)
        return;

    // Backpressure: don't extend the header front when it is already far ahead
    // of the connected block tip. Otherwise headers race past the block planner
    // and the bridge headers age out / get evicted before their blocks arrive.
    if (hashBestHeader != 0 &&
        GetPlannerHeight() - nBestHeight > HEADER_FRONT_MAX_AHEAD)
        return;

    std::vector<uint256> vHave;
    uint256 hashWalk = hashTip;
    int nStep = 1;

    while (hashWalk != 0)
    {
        vHave.push_back(hashWalk);

        for (int i = 0; i < nStep && hashWalk != 0; ++i)
        {
            uint256 hashPrev = 0;
            if (!GetPrevHash(hashWalk, hashPrev))
                hashWalk = 0;
            else
                hashWalk = hashPrev;
        }

        if (vHave.size() > 10)
            nStep *= 2;
    }

    vHave.push_back(!fTestNet ? hashGenesisBlockOfficial : hashGenesisBlockTestNet);
    pfrom->PushMessage("getheaders", CBlockLocator(vHave), uint256(0));
}

bool CSyncManager::RequestRefill(CNode* pfrom, uint256 hashTip, int64_t nMinIntervalSeconds, const char* pszReason)
{
    if (!pfrom || pfrom->fClient || pfrom->nVersion == 0 || !IsInitialBlockDownload())
        return false;

    const int64_t nNowSec = GetTime();
    if (nMinIntervalSeconds > 0 &&
        nNowSec - pfrom->nLastIbdHeaderRequest < nMinIntervalSeconds)
        return false;

    // Backpressure: stop pulling new headers once the front is far enough ahead
    // of the connected tip; let block download drain first (see ContinueHeaders).
    if (hashBestHeader != 0 &&
        GetPlannerHeight() - nBestHeight > HEADER_FRONT_MAX_AHEAD)
        return false;

    uint256 hashLocatorTip = hashTip;
    if (hashLocatorTip == 0 ||
        (!mapBlockIndex.count(hashLocatorTip) && !mapHeaders.count(hashLocatorTip)))
    {
        hashLocatorTip = hashBestHeader;
    }

    if (hashLocatorTip != 0 && (!pindexBest || hashLocatorTip != pindexBest->GetBlockHash()))
    {
        ContinueHeaders(pfrom, hashLocatorTip);
    }
    else
    {
        if (!pindexBest)
            return false;

        pfrom->pindexLastGetHeadersBegin = NULL;
        pfrom->PushGetHeaders(pindexBest, uint256(0));
        hashLocatorTip = pindexBest->GetBlockHash();
    }

    pfrom->nLastIbdHeaderRequest = nNowSec;
    printf("IBD-DIAG: %s getheaders to peer=%s locator=%s plannerDepth=%u inflight=%u\n",
        pszReason, pfrom->addr.ToString().c_str(),
        hashLocatorTip.ToString().substr(0,20).c_str(),
        GetPlannerDepth(), CountInFlight());
    return true;
}

unsigned int CSyncManager::RequestRefillAllPeers(uint256 hashTip, int64_t nMinIntervalSeconds, const char* pszReason)
{
    std::vector<CNode*> vEligiblePeers;
    {
        LOCK(cs_vNodes);
        for (CNode* pnode : vNodes)
        {
            if (!pnode->fClient && pnode->nVersion != 0 && !pnode->fDisconnect)
                vEligiblePeers.push_back(pnode);
        }
    }

    unsigned int nRequested = 0;
    for (CNode* pnode : vEligiblePeers)
    {
        if (RequestRefill(pnode, hashTip, nMinIntervalSeconds, pszReason))
            ++nRequested;
    }

    return nRequested;
}

unsigned int CSyncManager::QueueBlocksParallel(unsigned int nWindow)
{
    if (hashBestHeader == 0)
        return 0;

    const std::vector<uint256> vPath = GetDownloadPath(hashBestHeader);
    if (vPath.empty())
        return 0;

    // If the path doesn't connect back to the active chain, requesting these
    // blocks just fills the orphan pool. Bail and let Tick() repair the bridge.
    if (!PathReachesChain(vPath))
    {
        printf("IBD-DIAG: download path not anchored to chain (front=%s) — deferring to bridge repair\n",
            vPath.front().ToString().substr(0,20).c_str());
        return 0;
    }

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
    unsigned int nInFlight = CountInFlight();
    unsigned int nQueued = 0;
    unsigned int nPeerIndex = 0;

    // Option C: sort eligible peers by reliability score, not just blocks delivered.
    // A peer that's delivered 100 blocks but disconnected 20 times is less reliable
    // than a peer that's delivered 50 blocks with 0 disconnects. The score captures
    // both. We also drop peers with score <= 0 (effectively banned from sync).
    for (CNode* pnode : vEligiblePeers) {
        pnode->RecomputeReliabilityScore();
    }
    vEligiblePeers.erase(
        std::remove_if(vEligiblePeers.begin(), vEligiblePeers.end(),
            [](const CNode* p) { return p->nReliabilityScore <= 0; }),
        vEligiblePeers.end());

    std::sort(vEligiblePeers.begin(), vEligiblePeers.end(),
        [](const CNode* a, const CNode* b) {
            // Sort by reliability score (primary), then signed-peer bonus (signed > unsigned),
            // then blocks delivered (tiebreaker).
            if (a->nReliabilityScore != b->nReliabilityScore)
                return a->nReliabilityScore > b->nReliabilityScore;
            if (a->nSignedPeerBonus != b->nSignedPeerBonus)
                return a->nSignedPeerBonus > b->nSignedPeerBonus;
            return a->nBlocksDelivered > b->nBlocksDelivered;
        });

    std::vector<CNode*> vWeightedPeers;
    for (size_t i = 0; i < vEligiblePeers.size(); i++)
    {
        int nWeight = (i == 0) ? 3 : (i == 1) ? 2 : 1;
        for (int w = 0; w < nWeight; w++)
            vWeightedPeers.push_back(vEligiblePeers[i]);
    }

    int64_t nAdaptiveTimeout = HEADER_REQUEST_TIMEOUT_MICROS;
    {
        int64_t nTotalLatency = 0;
        int nPeersWithLatency = 0;
        for (const CNode* pnode : vEligiblePeers)
        {
            if (pnode->nAvgBlockLatencyUs > 0)
            {
                nTotalLatency += pnode->nAvgBlockLatencyUs;
                ++nPeersWithLatency;
            }
        }
        if (nPeersWithLatency > 0)
        {
            int64_t nAvgLatency = nTotalLatency / nPeersWithLatency;
            nAdaptiveTimeout = std::max((int64_t)(10 * 1000000),
                               std::min((int64_t)(60 * 1000000), nAvgLatency * 5));
        }
    }

    // Phase 1.5: per-peer inflight counting via HeaderNode.pnodeLastRequest.
    // Skip a peer if they're at their share of the global window. This caps the
    // damage a single .onion peer can do if they're feeding low-quality blocks.
    const unsigned int nPerPeerCap = HEADER_DOWNLOAD_WINDOW / std::max(1u, (unsigned int)vEligiblePeers.size()) + 1;
    std::map<const CNode*, unsigned int> mapPeerInflight;
    {
        const int64_t nNowInflight = GetTime() * 1000000;
        for (const auto& kv : mapHeaders) {
            if (kv.second.fRequested &&
                (nNowInflight - kv.second.nLastRequestTime) < HEADER_REQUEST_TIMEOUT_MICROS &&
                kv.second.pnodeLastRequest != nullptr) {
                ++mapPeerInflight[kv.second.pnodeLastRequest];
            }
        }
    }

    for (std::vector<uint256>::const_iterator it = vPath.begin(); it != vPath.end(); ++it)
    {
        if (nInFlight + nQueued >= nWindow)
            break;

        std::map<uint256, HeaderNode>::iterator mi = mapHeaders.find(*it);
        if (mi == mapHeaders.end())
            continue;

        bool fNeedsRequest = false;
        if (!mi->second.fRequested)
            fNeedsRequest = true;
        else if (nNow - mi->second.nLastRequestTime >= nAdaptiveTimeout)
            fNeedsRequest = true;
        else if (nNow - mi->second.nLastRequestTime >= HEADER_REDUNDANT_REQUEST_MICROS)
            fNeedsRequest = true;

        if (!fNeedsRequest)
            continue;

        // Phase 1.5: skip peers that are at their share of the global window. We
        // try the weighted peer first, and if they're capped, fall back to any
        // other eligible peer that's under the cap. This ensures one peer can't
        // claim the whole window even if they're the highest-weighted.
        CNode* pnode = nullptr;
        for (size_t tryIdx = 0; tryIdx < vWeightedPeers.size(); ++tryIdx) {
            CNode* candidate = vWeightedPeers[(nPeerIndex + tryIdx) % vWeightedPeers.size()];
            unsigned int candidateInflight = mapPeerInflight.count(candidate) ? mapPeerInflight[candidate] : 0;
            if (candidateInflight < nPerPeerCap) {
                pnode = candidate;
                nPeerIndex = (nPeerIndex + tryIdx) % vWeightedPeers.size();
                break;
            }
        }
        if (!pnode) {
            // All peers at cap — skip this block for now, it'll be retried later
            continue;
        }

        pnode->AskFor(CInv(MSG_BLOCK, *it));

        // Phase 1.5: record which peer this block was requested from for the
        // per-peer inflight count
        mi->second.pnodeLastRequest = pnode;
        mapPeerInflight[pnode] = (mapPeerInflight.count(pnode) ? mapPeerInflight[pnode] : 0) + 1;

        if (IsInitialBlockDownload() &&
            vWeightedPeers.size() >= 2 &&
            vWeightedPeers.size() < HEADER_REDUNDANT_PEER_THRESHOLD &&
            !mi->second.fRequested)
        {
            CNode* pnode2 = vWeightedPeers[(nPeerIndex + 1) % vWeightedPeers.size()];
            if (pnode2 != pnode)
                pnode2->AskFor(CInv(MSG_BLOCK, *it));
        }

        if (!mi->second.fRequested || nNow - mi->second.nLastRequestTime >= HEADER_REQUEST_TIMEOUT_MICROS)
        {
            if (!mi->second.fRequested)
            {
                mi->second.nFirstRequestTime = nNow;
                ++g_nInFlight;
            }
            mi->second.fRequested = true;
            mi->second.nLastRequestTime = nNow;
        }

        ++nQueued;
        ++nPeerIndex;
    }

    if (nQueued > 0)
        printf("IBD-DIAG: sync manager queued %u blocks across %zu peers (window=%u, inflight=%u)\n",
            nQueued, vEligiblePeers.size(), nWindow, nInFlight);

    return nQueued;
}

bool CSyncManager::ProcessHeaders(CNode* pfrom, const std::vector<CBlock>& vHeaders)
{
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
        if (mapBlockIndex.count(hashHeader) || mapHeaders.count(hashHeader))
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
            std::map<uint256, CBlockIndex*>::iterator miPrev = mapBlockIndex.find(header.hashPrevBlock);
            if (miPrev == mapBlockIndex.end() && !mapHeaders.count(header.hashPrevBlock))
                break;
        }

        if (!AddHeaderNode(header, hashHeader))
        {
            pfrom->Misbehaving(20);
            return error("invalid header sequence");
        }

        hashChainTip = hashHeader;
        nNewHeaders++;
    }

    int nRequested = 0;
    if (hashBestHeader != 0)
        nRequested = QueueBlocksParallel(HEADER_DOWNLOAD_WINDOW);

    if (nNewHeaders > 0)
        nLastNewHeaderTime = GetTime();

    if (nNewHeaders > 0 || nRequested > 0)
        printf("IBD-DIAG: accepted %d new headers, queued %d blocks from %zu headers (peer=%s bestHeader=%s)\n",
            nNewHeaders, nRequested, vHeaders.size(), pfrom->addr.ToString().c_str(),
            hashBestHeader.ToString().substr(0,20).c_str());

    if (vHeaders.size() >= 2000)
    {
        if (IsInitialBlockDownload() && hashChainTip != 0)
            ContinueHeaders(pfrom, hashChainTip);
        else
            pfrom->PushGetBlocks(pindexBest, uint256(0));
    }
    else if (IsInitialBlockDownload() && nNewHeaders > 0 && hashChainTip != 0)
    {
        ContinueHeaders(pfrom, hashChainTip);
    }
    else if (IsInitialBlockDownload())
    {
        const unsigned int nPlannerDepth = GetPlannerDepth();
        if (nPlannerDepth <= HEADER_SYNC_LOW_WATER)
            RequestRefill(
                pfrom, (hashChainTip != 0) ? hashChainTip : hashBestHeader,
                HEADER_SYNC_REFILL_MIN_INTERVAL_SECONDS,
                (nPlannerDepth == 0) ? "headers planner empty" : "headers planner low-water");
    }

    return true;
}

void CSyncManager::TrackBlockDelivery(CNode* pfrom, const uint256& hashBlock)
{
    if (!pfrom)
        return;

    pfrom->nBlocksDelivered++;
    // Option C: reward the peer for delivering a block. Capped at +200 by
    // RecomputeReliabilityScore. Also recompute to apply any flapping penalty
    // that may have accumulated since the last recompute.
    pfrom->nReliabilityScore = std::min(500, pfrom->nReliabilityScore + 5);
    if (nBestHeight > pfrom->nBestKnownHeight)
        pfrom->nBestKnownHeight = nBestHeight;

    int64_t nRequestTime = GetRequestTime(hashBlock);
    if (nRequestTime > 0)
    {
        int64_t nLatency = GetTime() * 1000000 - nRequestTime;
        if (nLatency > 0)
        {
            if (pfrom->nAvgBlockLatencyUs == 0)
                pfrom->nAvgBlockLatencyUs = nLatency;
            else
                pfrom->nAvgBlockLatencyUs = (pfrom->nAvgBlockLatencyUs * 7 + nLatency) / 8;
        }
    }
}

void CSyncManager::Tick(CNode* pto, int nHighestInvWalk, const uint256& hashHighestInvWalk)
{
    if (!pto || pto->fClient || pto->nVersion == 0 || !IsInitialBlockDownload())
        return;

    const int64_t nNowSec = GetTime();
    const unsigned int nPlannerDepth = GetPlannerDepth();
    const unsigned int nInFlight = CountInFlight();
    static int64_t nLastHeaderPlannerControl = 0;
    static int64_t nLastHeaderWatchdog = 0;
    static int64_t nLastBlockPlannerControl = 0;

    if (nLastNewHeaderTime == 0)
        nLastNewHeaderTime = nNowSec;

    if (nNowSec - nLastHeaderPlannerControl >= HEADER_SYNC_CONTROL_INTERVAL_SECONDS &&
        nPlannerDepth < HEADER_SYNC_LOW_WATER &&
        nInFlight < HEADER_SYNC_TARGET_INFLIGHT)
    {
        const unsigned int nRefilled = RequestRefillAllPeers(
            hashBestHeader, HEADER_SYNC_REFILL_MIN_INTERVAL_SECONDS,
            "control-loop");
        if (nRefilled > 0)
            printf("IBD-DIAG: control-loop refill from %u peers (plannerDepth=%u inflight=%u target=%u)\n",
                nRefilled, nPlannerDepth, nInFlight, HEADER_SYNC_TARGET_INFLIGHT);
        nLastHeaderPlannerControl = nNowSec;
    }

    // Pipeline refill: when the in-flight block window has drained (inflight==0)
    // and the planner still has headers ahead of the connected tip, kick a fresh
    // getheaders round on every eligible peer so the next batch of blocks is
    // requested BEFORE the current download finishes. Closes the "Tor pipe
    // empty" gaps that stall throughput between burst windows.
    if (nInFlight < (unsigned int)(HEADER_DOWNLOAD_WINDOW / 16) &&
        nPlannerDepth > 0)
    {
        const unsigned int nPipeline = RequestRefillAllPeers(
            hashBestHeader, HEADER_SYNC_REFILL_MIN_INTERVAL_SECONDS,
            "pipeline-prefetch");
        if (nPipeline > 0)
            printf("IBD-DIAG: pipeline-prefetch refill %u (plannerDepth=%u inflight=%u)\n",
                nPipeline, nPlannerDepth, nInFlight);
    }

    if (nNowSec - nLastHeaderWatchdog >= HEADER_SYNC_CONTROL_INTERVAL_SECONDS &&
        nNowSec - nLastNewHeaderTime >= HEADER_SYNC_WATCHDOG_SECONDS)
    {
        const unsigned int nRefilled = RequestRefillAllPeers(
            hashBestHeader, HEADER_SYNC_REFILL_MIN_INTERVAL_SECONDS,
            "headers-watchdog");
        if (nRefilled > 0)
            printf("IBD-DIAG: headers watchdog refill from %u peers after %llds without new headers (plannerDepth=%u inflight=%u)\n",
                nRefilled,
                (long long)(nNowSec - nLastNewHeaderTime),
                nPlannerDepth,
                nInFlight);
        nLastHeaderWatchdog = nNowSec;
    }

    const int64_t nMinInterval = (mapHeaders.size() < HEADER_DOWNLOAD_WINDOW) ? 15 : 60;
    if (nNowSec - pto->nLastIbdHeaderRequest >= nMinInterval)
        RequestRefill(pto, hashBestHeader, nMinInterval, "heartbeat");

    // Bridge repair: we have a best header, but the download path can't reach
    // the connected chain — a linking header was lost (TTL/eviction/hole). Ask
    // this peer for headers with a locator anchored at the REAL chain tip so it
    // resends the headers directly above pindexBest and re-links the path.
    if (hashBestHeader != 0 && pindexBest &&
        !PathReachesChain(GetDownloadPath(hashBestHeader)) &&
        nNowSec - pto->nLastIbdHeaderRequest >= HEADER_SYNC_REFILL_MIN_INTERVAL_SECONDS)
    {
        pto->pindexLastGetHeadersBegin = NULL;
        pto->PushGetHeaders(pindexBest, uint256(0));
        pto->nLastIbdHeaderRequest = nNowSec;
        printf("IBD-DIAG: bridge-repair getheaders from connected tip height=%d peer=%s\n",
            pindexBest->nHeight, pto->addr.ToString().c_str());
    }

    if (nNowSec - nLastBlockPlannerControl >= HEADER_SYNC_CONTROL_INTERVAL_SECONDS &&
        hashBestHeader != 0 &&
        nPlannerDepth > 0)
    {
        const unsigned int nRequeued = QueueBlocksParallel(HEADER_DOWNLOAD_WINDOW);
        if (nRequeued > 0)
            printf("IBD-DIAG: block-planner control queued %u block requests (plannerDepth=%u inflight=%u)\n",
                nRequeued, nPlannerDepth, nInFlight);
        nLastBlockPlannerControl = nNowSec;
    }

    if (hashBestHeader == 0 && nHighestInvWalk > nBestHeight &&
        hashHighestInvWalk != 0 && mapBlockIndex.count(hashHighestInvWalk))
    {
        RequestRefill(pto, hashHighestInvWalk, HEADER_SYNC_REFILL_MIN_INTERVAL_SECONDS, "inv-walk bridge");
    }
}
