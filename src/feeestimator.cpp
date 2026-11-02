// Copyright (c) 2024 The Triangles developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// feeestimator.cpp — implementation of CFeeEstimator.
//
// Sliding-window percentile fee estimator.
//
// Algorithm:
//   1. For each connected block, RecordTransaction() is called for every
//      regular (non-coinbase, non-coinstake) tx with its fee and size.
//   2. EndBlock() seals the per-block record into the sliding window map.
//   3. EstimateFee() merges all per-block fee-per-KB samples from the last
//      nMaxBlocks blocks into one sorted vector, then picks the percentile
//      corresponding to the confirmation target (higher target → lower
//      percentile → lower fee).
//   4. If there are fewer than FEE_ESTIMATOR_MIN_BLOCKS blocks or
//      FEE_ESTIMATOR_MIN_TXS transactions in the window, returns 0 to signal
//      the caller to fall back to MIN_TX_FEE.

#include "feeestimator.h"

#include <algorithm>
#include <numeric>

// ─── Global instance ───────────────────────────────────────────────
CFeeEstimator feeEstimator;

// ─── Constructor ───────────────────────────────────────────────────
CFeeEstimator::CFeeEstimator()
    : nMaxBlocks(FEE_ESTIMATOR_DEFAULT_WINDOW)
    , nCurrentHeight(0)
    , nTotalTxCount(0)
    , fCacheValid(false)
{
}

// ─── RecordTransaction ─────────────────────────────────────────────
void CFeeEstimator::RecordTransaction(const uint256& hashBlock, int nHeight,
                                      int64_t nFeePaid, unsigned int nTxSizeBytes)
{
    if (nTxSizeBytes == 0)
        return;

    LOCK(cs);

    // If this is a new block, start accumulating
    if (hashCurrentBlock != hashBlock)
    {
        hashCurrentBlock = hashBlock;
        nCurrentHeight   = nHeight;
        vecCurrentFees.clear();
    }

    // Compute fee-per-KB (rounded up to avoid 0 for small fees)
    int64_t nFeePerKB;
    if (nFeePaid <= 0)
        nFeePerKB = 0;
    else
        nFeePerKB = (nFeePaid * 1000 + nTxSizeBytes - 1) / nTxSizeBytes;  // ceil

    vecCurrentFees.push_back(nFeePerKB);
}

// ─── EndBlock ──────────────────────────────────────────────────────
void CFeeEstimator::EndBlock(const uint256& hashBlock, int nHeight)
{
    LOCK(cs);

    // Only seal if the accumulated block matches
    if (hashCurrentBlock != hashBlock)
        return;  // nothing to seal (block had no regular txs recorded)

    BlockFeeRecord record;
    record.nHeight     = nHeight;
    record.hashBlock   = hashBlock;
    record.vecFeePerKB = std::move(vecCurrentFees);

    // Sort the per-block fees ascending
    std::sort(record.vecFeePerKB.begin(), record.vecFeePerKB.end());

    // Update total tx count
    nTotalTxCount += (unsigned int)record.vecFeePerKB.size();

    mapBlocks[nHeight] = std::move(record);

    // Reset accumulator
    hashCurrentBlock = uint256();
    nCurrentHeight = 0;
    vecCurrentFees.clear();

    // Prune old blocks
    PruneOldBlocks(nHeight);

    fCacheValid = false;
}

// ─── PruneOldBlocks ────────────────────────────────────────────────
void CFeeEstimator::PruneOldBlocks(int nBestHeight)
{
    // We hold cs already (called from EndBlock or SetMaxBlocks)

    int nMinHeight = nBestHeight - (int)nMaxBlocks + 1;
    if (nMinHeight < 0)
        nMinHeight = 0;

    auto it = mapBlocks.begin();
    while (it != mapBlocks.end())
    {
        if (it->first < nMinHeight)
        {
            nTotalTxCount -= (unsigned int)it->second.vecFeePerKB.size();
            it = mapBlocks.erase(it);
        }
        else
            ++it;
    }
}

// ─── RebuildCache ──────────────────────────────────────────────────
void CFeeEstimator::RebuildCache() const
{
    // We hold cs already

    vecAllFees.clear();
    vecAllFees.reserve(nTotalTxCount);

    for (const auto& [height, record] : mapBlocks)
    {
        vecAllFees.insert(vecAllFees.end(),
                          record.vecFeePerKB.begin(),
                          record.vecFeePerKB.end());
    }

    std::sort(vecAllFees.begin(), vecAllFees.end());
    fCacheValid = true;
}

// ─── EstimateFee ───────────────────────────────────────────────────
int64_t CFeeEstimator::EstimateFee(unsigned int confTarget) const
{
    LOCK(cs);

    if (confTarget == 0)
        confTarget = FEE_ESTIMATOR_DEFAULT_TARGET;
    if (confTarget > FEE_ESTIMATOR_MAX_TARGET)
        confTarget = FEE_ESTIMATOR_MAX_TARGET;

    // Need enough data
    if (mapBlocks.size() < FEE_ESTIMATOR_MIN_BLOCKS)
        return 0;

    if (!fCacheValid)
        RebuildCache();

    if (vecAllFees.size() < FEE_ESTIMATOR_MIN_TXS)
        return 0;

    //
    // Percentile selection:
    //
    //   confTarget = 1   →  90th percentile (need to be in the top 10% of fees)
    //   confTarget = 2   →  80th percentile
    //   confTarget = 6   →  30th percentile
    //   confTarget = 10+ →  floor (minimum observed fee)
    //
    // The idea: a higher confirmation target means you can afford to wait, so
    // a lower fee percentile suffices.  We map the target to a percentile via:
    //
    //   percentile = max(10, 100 - confTarget * 10)
    //
    // so confTarget=1 → 90%, confTarget=6 → 40%, confTarget=10+ → 10%.
    //
    unsigned int percentile;
    if (confTarget <= 1)
        percentile = 90;
    else
    {
        int pct = 100 - (int)confTarget * 10;
        if (pct < 10)
            pct = 10;
        percentile = (unsigned int)pct;
    }

    // Index into the sorted vector
    // e.g. percentile=90, size=100 → index 89 (0-based)
    size_t nIdx = (vecAllFees.size() * percentile) / 100;
    if (nIdx >= vecAllFees.size())
        nIdx = vecAllFees.size() - 1;

    int64_t nResult = vecAllFees[nIdx];

    // Never return 0 — if the percentile resolves to 0 (many zero-fee txs),
    // return MIN_TX_FEE as a sensible floor.
    if (nResult < MIN_TX_FEE)
        nResult = MIN_TX_FEE;

    return nResult;
}

// ─── GetBlockCount ─────────────────────────────────────────────────
unsigned int CFeeEstimator::GetBlockCount() const
{
    LOCK(cs);
    return (unsigned int)mapBlocks.size();
}

// ─── GetTxCount ────────────────────────────────────────────────────
unsigned int CFeeEstimator::GetTxCount() const
{
    LOCK(cs);
    return nTotalTxCount;
}

// ─── SetMaxBlocks ──────────────────────────────────────────────────
void CFeeEstimator::SetMaxBlocks(unsigned int nMax)
{
    LOCK(cs);
    nMaxBlocks = nMax;

    // Re-prune if we already have data
    if (!mapBlocks.empty())
    {
        int nBestHeight = mapBlocks.rbegin()->first;
        PruneOldBlocks(nBestHeight);
        fCacheValid = false;
    }
}
