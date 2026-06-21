// Copyright (c) 2026 The Triangles developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef TRIANGLES_SYNCMANAGER_H
#define TRIANGLES_SYNCMANAGER_H

#include "uint256.h"

#include <cstddef>
#include <cstdint>
#include <vector>

class CBlock;
class CInv;
class CNode;

class CSyncManager
{
public:
    struct HeaderNode;

    static constexpr unsigned int HEADER_DOWNLOAD_WINDOW = 1024;
    static constexpr unsigned int HEADER_SYNC_LOW_WATER = HEADER_DOWNLOAD_WINDOW / 4;
    static constexpr unsigned int HEADER_SYNC_TARGET_INFLIGHT = HEADER_DOWNLOAD_WINDOW / 2;
    static constexpr int64_t HEADER_SYNC_REFILL_MIN_INTERVAL_SECONDS = 5;
    static constexpr int64_t HEADER_SYNC_CONTROL_INTERVAL_SECONDS = 5;
    static constexpr int64_t HEADER_SYNC_WATCHDOG_SECONDS = 25;

    bool HaveHeader(const uint256& hash) const;
    uint256 GetBestHeader() const;
    std::size_t GetHeaderCount() const;
    unsigned int CountInFlight() const;
    unsigned int GetPlannerDepth() const;
    int GetPlannerHeight() const;
    int64_t GetRequestTime(const uint256& hashBlock) const;

    bool RequestRefill(CNode* pfrom, uint256 hashTip, int64_t nMinIntervalSeconds, const char* pszReason);
    unsigned int RequestRefillAllPeers(uint256 hashTip, int64_t nMinIntervalSeconds, const char* pszReason);
    unsigned int QueueBlocksParallel(unsigned int nWindow = HEADER_DOWNLOAD_WINDOW);
    bool ProcessHeaders(CNode* pfrom, const std::vector<CBlock>& vHeaders);
    void BlockAccepted(const uint256& hashBlock);
    void TrackBlockDelivery(CNode* pfrom, const uint256& hashBlock);
    void Tick(CNode* pto, int nHighestInvWalk, const uint256& hashHighestInvWalk);

private:
    uint256 GetHeaderTrust(unsigned int nBits) const;
    bool GetKnownHeaderState(const uint256& hash, int& nHeight, uint256& nChainTrust) const;
    bool GetPrevHash(const uint256& hash, uint256& hashPrev) const;
    void RecomputeBestHeader();
    void PruneHeaders();
    bool AddHeaderNode(const CBlock& header, const uint256& hashHeader);
    std::vector<uint256> GetDownloadPath(uint256 hashTip) const;
    bool PathReachesChain(const std::vector<uint256>& vPath) const;
    void ContinueHeaders(CNode* pfrom, const uint256& hashTip);
};

extern CSyncManager g_syncManager;

#endif // TRIANGLES_SYNCMANAGER_H
