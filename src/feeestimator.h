// Copyright (c) 2024 The Triangles developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// feeestimator.h — Mempool-aware fee estimation for the Triangles daemon.
//
// CFeeEstimator tracks fees paid by non-coinbase / non-coinstake transactions
// confirmed in the last N blocks (sliding window, default 100).  Transactions
// are bucketed by fee-per-KB and the estimator answers "what fee-per-KB is
// needed for confirmation within `confTarget` blocks?" using a decaying
// percentile approach.
//
// Design goals:
//   * Pure local computation — no external API calls (Tor-native compatible).
//   * Thread-safe via its own recursive mutex (called from validation and RPC
//     threads concurrently).
//   * Graceful fallback to MIN_TX_FEE during early chain / low volume.
//   * Does NOT affect consensus — only used for fee suggestions and wallet
//     default fee selection.  GetMinFee() keeps the flat fee as floor.

#ifndef TRIANGLES_FEEESTIMATOR_H
#define TRIANGLES_FEEESTIMATOR_H

#include "main.h"    // for int64_t, MIN_TX_FEE, MAX_BLOCK_SIZE
#include "sync.h"    // CCriticalSection / LOCK
#include "uint256.h"

#include <map>
#include <vector>
#include <cstdint>
#include <algorithm>

//
// Default configuration
//
static constexpr unsigned int FEE_ESTIMATOR_DEFAULT_WINDOW = 100;   // blocks
static constexpr unsigned int FEE_ESTIMATOR_DEFAULT_TARGET = 6;     // conf target
static constexpr unsigned int FEE_ESTIMATOR_MAX_TARGET     = 1008;  // ~1 week
static constexpr unsigned int FEE_ESTIMATOR_MIN_BLOCKS     = 10;    // need ≥ this many blocks of data
static constexpr unsigned int FEE_ESTIMATOR_MIN_TXS        = 50;    // need ≥ this many tx samples

//
// How many fee-per-KB buckets we track.  More buckets → finer granularity
// but needs more samples.  2^12 = 4096 is plenty for this coin's volume.
//
static constexpr unsigned int FEE_BUCKET_BITS = 12;
static constexpr unsigned int FEE_BUCKET_COUNT = (1u << FEE_BUCKET_BITS);   // 4096

// ─── Per-block record ──────────────────────────────────────────────
//   For each confirmed block we record one BlockFeeRecord containing
//   the sorted list of fee-per-KB values of all regular (non-coinbase,
//   non-coinstake) transactions.  This keeps memory bounded: a block can
//   have at most a few thousand txs.

struct BlockFeeRecord
{
    int nHeight;
    uint256 hashBlock;
    std::vector<int64_t> vecFeePerKB;   // sorted ascending

    BlockFeeRecord() : nHeight(0) {}
};

// ─── CFeeEstimator ─────────────────────────────────────────────────

class CFeeEstimator
{
public:
    CFeeEstimator();
    ~CFeeEstimator() = default;

    //
    // Called from ConnectBlock() for every regular transaction in a newly
    // connected block.  Multiple calls within the same block accumulate into
    // a single BlockFeeRecord that is sealed when EndBlock() is called.
    //
    void RecordTransaction(const uint256& hashBlock, int nHeight,
                           int64_t nFeePaid, unsigned int nTxSizeBytes);

    //
    // Seal the current block's accumulated fee data into the sliding window.
    // Called once at the end of ConnectBlock() (after all txs are processed).
    //
    void EndBlock(const uint256& hashBlock, int nHeight);

    //
    // Estimate the fee-per-KB (in satoshis) required for a transaction to be
    // confirmed within confTarget blocks.
    //
    // Returns 0 if insufficient data (caller should fall back to MIN_TX_FEE).
    //
    int64_t EstimateFee(unsigned int confTarget = FEE_ESTIMATOR_DEFAULT_TARGET) const;

    //
    // How many blocks of data do we currently have?
    //
    unsigned int GetBlockCount() const;

    //
    // How many total transaction samples do we have?
    //
    unsigned int GetTxCount() const;

    //
    // Trim the sliding window to the configured size.  Called internally.
    //
    void SetMaxBlocks(unsigned int nMaxBlocks);

private:
    // Recursive mutex — RecordTransaction may be called while another thread
    // calls EstimateFee.
    mutable CCriticalSection cs;

    unsigned int nMaxBlocks;   // sliding window size

    // Current accumulating block
    uint256 hashCurrentBlock;
    int nCurrentHeight;
    std::vector<int64_t> vecCurrentFees;

    // Sealed historical records keyed by height for O(log N) pruning
    std::map<int, BlockFeeRecord> mapBlocks;

    // Aggregate cached stats (updated on EndBlock / prune)
    unsigned int nTotalTxCount;
    mutable bool fCacheValid;
    mutable std::vector<int64_t> vecAllFees;   // cached merged+sorted

    void PruneOldBlocks(int nBestHeight);
    void RebuildCache() const;
};

// Global instance — defined in feeestimator.cpp, declared extern in main.h
extern CFeeEstimator feeEstimator;

#endif // TRIANGLES_FEEESTIMATOR_H
