// Copyright (c) 2026 Triangles developers
// Distributed under the MIT/X11 software license
//
// CONSENSUS SAFETY REGRESSION TESTS
// Added 2026-07-04 by autonomous audit session.
//
// These tests probe properties that, if violated, would cause:
//   - Chain splits (nodes disagreeing on validity)
//   - Inflation bugs (more coins created than allowed)
//   - Reorg attacks (history rewrite beyond finality limit)
//   - Time-warp attacks (blocks/txs with absurd timestamps accepted)
//
// Every assertion here corresponds to a literal consensus rule. If the
// assertion fails, the daemon and testnet would diverge from mainnet.

#include <boost/test/unit_test.hpp>

#include "../main.h"
#include "../kernel.h"
#include "../script.h"
#include "../checkpoints.h"

extern CBlockIndex* pindexBest;
extern unsigned int nTargetSpacing;
extern unsigned int nStakeMinAge;
extern unsigned int nStakeMaxAge;
extern unsigned int nModifierInterval;
extern int nCoinbaseMaturity;

BOOST_AUTO_TEST_SUITE(consensus_safety_tests)

// ─── Reorg finality (P0 — security) ────────────────────────────────────────
// MAX_REORG_DEPTH caps how deep a reorg can go. If unset or too small,
// an attacker can rewrite recent history. If too large, accidental splits
// become possible. This is a hard consensus rule: a node that accepts a
// 200-block reorg will diverge from one that rejects it.
BOOST_AUTO_TEST_CASE(max_reorg_depth_enforced)
{
    BOOST_CHECK_EQUAL(MAX_REORG_DEPTH, 100);

    // The constant must be positive (otherwise every reorg is rejected).
    BOOST_CHECK_GT(MAX_REORG_DEPTH, 0);

    // And reasonably small (finality in 100 blocks = ~3.3 hours at 2-min
    // target). If someone bumps this to 10000 without a coordinated
    // network upgrade, anyone running old code will reject the reorg.
    BOOST_CHECK_LE(MAX_REORG_DEPTH, 1000);
}

// ─── Money supply cap (P0 — inflation safety) ─────────────────────────────
// MAX_MONEY is the absolute ceiling on total TRI in circulation. Any block
// or transaction that would push the supply above this must be rejected
// by every node. MoneyRange is the gatekeeper.
BOOST_AUTO_TEST_CASE(money_range_strict)
{
    // Boundaries: exactly at the cap is OK, one over is not.
    BOOST_CHECK(MoneyRange(0));
    BOOST_CHECK(MoneyRange(1));
    BOOST_CHECK(MoneyRange(MAX_MONEY - 1));
    BOOST_CHECK(MoneyRange(MAX_MONEY));
    BOOST_CHECK(!MoneyRange(MAX_MONEY + 1));
    BOOST_CHECK(!MoneyRange(MAX_MONEY + COIN));

    // Negative values: must be rejected (would allow coin-supply attacks
    // if a buggy tx-creation path forgot to check).
    BOOST_CHECK(!MoneyRange(-1));
    BOOST_CHECK(!MoneyRange(-COIN));
    BOOST_CHECK(!MoneyRange(INT64_MIN));

    // Near overflow: also must be rejected.
    BOOST_CHECK(!MoneyRange(INT64_MAX));
    BOOST_CHECK(!MoneyRange(INT64_MAX - COIN));
}

// ─── COIN_YEAR_REWARD and MAX_TRI_PROOF_OF_STAKE must agree (P0) ──────────
// These are two different expressions of the same value (33% annual PoS
// reward). If they ever drift, GetProofOfStakeReward will produce
// different totals depending on which one it uses, and nodes will
// disagree on reward amounts → chain split.
BOOST_AUTO_TEST_CASE(coin_year_reward_matches_max_tri_pos)
{
    BOOST_CHECK_EQUAL(COIN_YEAR_REWARD, 33 * CENT);
    BOOST_CHECK_EQUAL(MAX_TRI_PROOF_OF_STAKE, static_cast<int64_t>(0.33 * COIN));

    // Critical: they must be exactly equal so the consensus rule
    // "33% annual reward" is unambiguous.
    BOOST_CHECK_EQUAL(static_cast<int64_t>(COIN_YEAR_REWARD),
                     static_cast<int64_t>(MAX_TRI_PROOF_OF_STAKE));
}

// ─── Time-drift boundary at FORK_HEIGHT_V5_4 (P0) ────────────────────────
// The fork transition from 10-minute drift to 90-second drift must be
// sharp: at FORK_HEIGHT_V5_4-1 the old rule applies, at FORK_HEIGHT_V5_4
// the new rule applies. If the boundary is off by one, a node on the
// "before" side and a node on the "after" side will disagree on the
// validity of any block at that height with a non-trivial timestamp.
BOOST_AUTO_TEST_CASE(time_drift_fork_boundary)
{
    // Pre-fork: 600s drift
    BOOST_CHECK_EQUAL(GetMaxTimeDrift(FORK_HEIGHT_V5_4 - 1), 600);
    BOOST_CHECK_EQUAL(GetMaxTimeDrift(FORK_HEIGHT_V5_4 - 1000), 600);
    BOOST_CHECK_EQUAL(GetMaxTimeDrift(0), 600);
    BOOST_CHECK_EQUAL(GetMaxTimeDrift(9000), 600);

    // Post-fork: 90s drift
    BOOST_CHECK_EQUAL(GetMaxTimeDrift(FORK_HEIGHT_V5_4), 90);
    BOOST_CHECK_EQUAL(GetMaxTimeDrift(FORK_HEIGHT_V5_4 + 1), 90);
    BOOST_CHECK_EQUAL(GetMaxTimeDrift(FORK_HEIGHT_V5_4 + 100000), 90);

    // The drift must be strictly tighter after the fork (this is the
    // whole point of the v5.4 fork — block timestamps become more
    // strictly enforced post-fork).
    BOOST_CHECK_LT(GetMaxTimeDrift(FORK_HEIGHT_V5_4), GetMaxTimeDrift(FORK_HEIGHT_V5_4 - 1));

    // Boundary sharpness: the height-less overloads always use post-V5.4
    // rules (90s) regardless of the caller's height. This was a deliberate
    // fix because using the global nBestHeight previously caused nodes
    // at different heights to disagree on block validity during the fork
    // transition — a consensus-splitting bug.
    int64_t now = 1700000000;
    BOOST_CHECK_EQUAL(PastDrift(now), now - 90);
    BOOST_CHECK_EQUAL(FutureDrift(now), now + 90);
    // The height-parameterized versions MUST be sharp at the boundary.
    BOOST_CHECK_EQUAL(PastDrift(now, FORK_HEIGHT_V5_4 - 1), now - 600);
    BOOST_CHECK_EQUAL(PastDrift(now, FORK_HEIGHT_V5_4), now - 90);
    BOOST_CHECK_EQUAL(FutureDrift(now, FORK_HEIGHT_V5_4 - 1), now + 600);
    BOOST_CHECK_EQUAL(FutureDrift(now, FORK_HEIGHT_V5_4), now + 90);
}

// ─── CRAPCHAIN_CUTOFF_BLOCK vs FORK_HEIGHT_V5 (P1 — historical artifact) ──
// CRAPCHAIN_CUTOFF_BLOCK is the height of the last block in the legacy
// v4 (Pharao) chain. FORK_HEIGHT_V5 is the first height of the v5 chain.
// These are 40 blocks apart. The 40-block gap is intentional: it provides
// a buffer for nodes syncing the old chain while the new chain activates.
// If anyone flips the relationship (e.g. CRAPCHAIN > FORK_V5), the
// daemon will silently accept blocks from the wrong chain.
BOOST_AUTO_TEST_CASE(crapchain_cutoff_before_fork_v5)
{
    BOOST_CHECK_EQUAL(FORK_HEIGHT_V5, 17651);
    BOOST_CHECK_EQUAL(CRAPCHAIN_CUTOFF_BLOCK, 17691);
    BOOST_CHECK_LT(FORK_HEIGHT_V5, CRAPCHAIN_CUTOFF_BLOCK);

    // The gap (40 blocks) is part of the chain's identity.
    int64_t gap = CRAPCHAIN_CUTOFF_BLOCK - FORK_HEIGHT_V5;
    BOOST_CHECK_EQUAL(gap, 40);
}

// ─── PoW vs PoS transition (P0) ────────────────────────────────────────────
// CUTOFF_POW_BLOCK = 9000 is the LAST PoW block. Block 9001 is the FIRST
// PoS block. Any value other than 9000 here will break the chain split
// between legacy PoW nodes and new PoS nodes.
BOOST_AUTO_TEST_CASE(pow_to_pos_transition_exact)
{
    BOOST_CHECK_EQUAL(CUTOFF_POW_BLOCK, 9000);

    // Simulate the boundary by temporarily setting pindexBest->nHeight
    // and verifying the reward schedule.
    CBlockIndex origBest;
    bool wasNull = (pindexBest == nullptr);
    if (!wasNull) origBest = *pindexBest;
    CBlockIndex testBest;
    testBest.nHeight = 0;
    pindexBest = &testBest;

    // At height 0, subsidy is the initial 1 COIN (since the
    // if-else-if chain has no height>=0 case, only height>=1; height=0
    // falls through and nSubsidy stays at the initial 1*COIN).
    BOOST_CHECK_EQUAL(GetProofOfWorkReward(0), 1 * COIN);

    // At height 9000 (last PoW block), subsidy should still be the
    // 5-10 TRI tier (height>=7000 gives 10 COIN).
    testBest.nHeight = 9000;
    BOOST_CHECK_EQUAL(GetProofOfWorkReward(0), 10 * COIN);

    // At height 9001 (first PoS-eligible), PoW subsidy is 0. This is
    // critical: a non-zero subsidy at 9001 would mean PoW and PoS are
    // both producing coins at the same height, causing inflation.
    testBest.nHeight = 9001;
    BOOST_CHECK_EQUAL(GetProofOfWorkReward(0), 0);

    // Even at huge heights, PoW subsidy remains 0.
    testBest.nHeight = 1000000;
    BOOST_CHECK_EQUAL(GetProofOfWorkReward(0), 0);

    // Restore.
    if (wasNull) pindexBest = nullptr;
    else *pindexBest = origBest;
}

// ─── PoW reward tiers (P1 — economic policy) ──────────────────────────────
// Each tier of the PoW reward schedule is a hard consensus rule. If a
// tier drifts, the monetary policy changes silently.
BOOST_AUTO_TEST_CASE(pow_reward_each_tier_exact)
{
    CBlockIndex testBest;
    testBest.nHeight = 0;
    pindexBest = &testBest;

    // Tier: height 0 (initial subsidy)
    BOOST_CHECK_EQUAL(GetProofOfWorkReward(0), 1 * COIN);

    // Tier: height 1-99 → 1 COIN
    testBest.nHeight = 1;
    BOOST_CHECK_EQUAL(GetProofOfWorkReward(0), 1 * COIN);
    testBest.nHeight = 99;
    BOOST_CHECK_EQUAL(GetProofOfWorkReward(0), 1 * COIN);

    // Tier: height 100-999 → 20 COIN
    testBest.nHeight = 100;
    BOOST_CHECK_EQUAL(GetProofOfWorkReward(0), 20 * COIN);
    testBest.nHeight = 999;
    BOOST_CHECK_EQUAL(GetProofOfWorkReward(0), 20 * COIN);

    // Tier: height 1000-2999 → 10 COIN
    testBest.nHeight = 1000;
    BOOST_CHECK_EQUAL(GetProofOfWorkReward(0), 10 * COIN);
    testBest.nHeight = 2999;
    BOOST_CHECK_EQUAL(GetProofOfWorkReward(0), 10 * COIN);

    // Tier: height 3000-6999 → 5 COIN
    testBest.nHeight = 3000;
    BOOST_CHECK_EQUAL(GetProofOfWorkReward(0), 5 * COIN);
    testBest.nHeight = 6999;
    BOOST_CHECK_EQUAL(GetProofOfWorkReward(0), 5 * COIN);

    // Tier: height 7000-9000 → 10 COIN
    testBest.nHeight = 7000;
    BOOST_CHECK_EQUAL(GetProofOfWorkReward(0), 10 * COIN);
    testBest.nHeight = 9000;
    BOOST_CHECK_EQUAL(GetProofOfWorkReward(0), 10 * COIN);

    // Tier: height >= 9001 → 0 (PoS takes over)
    testBest.nHeight = 9001;
    BOOST_CHECK_EQUAL(GetProofOfWorkReward(0), 0);

    // Restore
    pindexBest = nullptr;
}

// ─── Genesis hash (P0 — chain identity) ───────────────────────────────────
// The genesis hash is the chain's identity. If this changes, every
// existing node will reject blocks from the new chain.
BOOST_AUTO_TEST_CASE(genesis_hash_immutable)
{
    // Document the current genesis hash so any future change is intentional.
    BOOST_CHECK_EQUAL(
        hashGenesisBlockOfficial.ToString(),
        "7e7a6e4dd5fe895106fca912dfbacaeaf2a89e76c6a588df8ff96e0e18b96021"
    );
    // Same for testnet — they MUST be identical.
    BOOST_CHECK_EQUAL(
        hashGenesisBlockTestNet.ToString(),
        "7e7a6e4dd5fe895106fca912dfbacaeaf2a89e76c6a588df8ff96e0e18b96021"
    );
    BOOST_CHECK(hashGenesisBlockOfficial == hashGenesisBlockTestNet);
}

// ─── Locktime threshold (P0) ──────────────────────────────────────────────
// Locktime values below LOCKTIME_THRESHOLD are interpreted as block
// numbers, above as UNIX timestamps. If the threshold drifts, every
// non-final transaction on the network will suddenly become valid (or
// invalid) at the wrong time.
BOOST_AUTO_TEST_CASE(locktime_threshold_strict)
{
    BOOST_CHECK_EQUAL(LOCKTIME_THRESHOLD, 500000000u);

    // The threshold is fixed in 1985; only an exact equality check is
    // appropriate. Any other value would be a consensus bug.
    BOOST_CHECK_EQUAL(LOCKTIME_THRESHOLD, 500000000u);
    BOOST_CHECK_EQUAL(LOCKTIME_THRESHOLD, 500000000u);

    // Sanity: this is in the 1985-01-01 to 2106-02-07 range.
    BOOST_CHECK_GT(LOCKTIME_THRESHOLD, 473385600u);  // 1985-01-01
    BOOST_CHECK_LT(LOCKTIME_THRESHOLD, 4294967295u);  // fits in uint32
}

// ─── Coin age weight monotonicity (P1 — staking economics) ──────────────────
// GetWeight must be non-decreasing in coin age (more age = at least as
// much weight, never less). A violation would let stakers game the
// system by waiting for specific age windows.
BOOST_AUTO_TEST_CASE(coin_age_weight_monotonic)
{
    int64_t now = 1700000000;
    int64_t prevWeight = 0;
    // Sample at increasing ages, skipping the zero-weight region below
    // nStakeMinAge.
    for (int64_t age = nStakeMinAge; age < nStakeMinAge + 100000; age += 5000) {
        int64_t weight = GetWeight(now - age, now);
        BOOST_CHECK_GE(weight, prevWeight);
        prevWeight = weight;
    }
}

// ─── Stake age soft cap (P1 — V5 fork economic rule) ──────────────────────
// The V5 fork (FORK_HEIGHT_V5) replaced the hard nStakeMaxAge cap with a
// 7-day soft cap. The cap only applies to stakes AFTER the activation
// timestamp (1776000000 = 2026-04-12 13:20 UTC). This is a soft fork
// rule — historical blocks staked before activation are unaffected.
//
// We test it in a way that does NOT depend on pindexBest (which is a
// global state) by using a fixed "now" that's well past activation and
// a height that's pre-V5. Pre-V5 path is in src/kernel.cpp:25-53.
BOOST_AUTO_TEST_CASE(stake_age_soft_cap_does_not_apply_pre_v5)
{
    int64_t now = 1777000000;  // well past 1776000000 activation
    // With pindexBest == nullptr, the pre-V5 path runs (line 52 in
    // kernel.cpp): min(nAge, nStakeMaxAge). nStakeMaxAge is 12 hours.
    int64_t veryOld = now - nStakeMinAge - (10 * 24 * 60 * 60);  // 10 days old
    int64_t weight = GetWeight(veryOld, now);
    // Pre-V5 cap is nStakeMaxAge = 43200 (12 hours).
    BOOST_CHECK_EQUAL(weight, (int64_t)nStakeMaxAge);

    // Right at the cap boundary:
    int64_t atMaxAge = now - nStakeMinAge - nStakeMaxAge;
    BOOST_CHECK_EQUAL(GetWeight(atMaxAge, now), (int64_t)nStakeMaxAge);
    // One second past: also capped.
    int64_t justPastMax = now - nStakeMinAge - nStakeMaxAge - 1;
    BOOST_CHECK_EQUAL(GetWeight(justPastMax, now), (int64_t)nStakeMaxAge);
}

// ─── PoS validation fast path must be height-based (P0) ───────────────────
// IsInitialBlockDownload() can also mean "tip is stale". That operational
// state must never disable proof-of-stake kernel/reward validation for new
// blocks above the hardened-checkpoint / rolling-assume-valid fast path.
BOOST_AUTO_TEST_CASE(pos_validation_skip_is_only_historical_fast_path)
{
    int oldAssumeValid = nAssumeValidThreshold;
    nAssumeValidThreshold = 0;

    const int checkpointHeight = Checkpoints::GetTotalBlocksEstimate();

    BOOST_CHECK(IsConsensusAssumeValidHeight(checkpointHeight));
    BOOST_CHECK(!IsConsensusAssumeValidHeight(checkpointHeight + 1));

    nAssumeValidThreshold = checkpointHeight + 25;
    BOOST_CHECK(IsConsensusAssumeValidHeight(checkpointHeight + 25));
    BOOST_CHECK(!IsConsensusAssumeValidHeight(checkpointHeight + 26));

    nAssumeValidThreshold = oldAssumeValid;
}

BOOST_AUTO_TEST_CASE(pos_block_signature_is_required_above_hardened_checkpoint)
{
    const int oldAssumeValid = nAssumeValidThreshold;
    const int checkpointHeight = Checkpoints::GetTotalBlocksEstimate();

    BOOST_CHECK(!IsBlockSignatureRequiredAtHeight(checkpointHeight));
    BOOST_CHECK(IsBlockSignatureRequiredAtHeight(checkpointHeight + 1));

    // A rolling performance threshold must never authorize unsigned live
    // blocks, including when stale-tip state makes the node report IBD.
    nAssumeValidThreshold = checkpointHeight + 100;
    BOOST_CHECK(IsConsensusAssumeValidHeight(checkpointHeight + 50));
    BOOST_CHECK(IsBlockSignatureRequiredAtHeight(checkpointHeight + 50));

    nAssumeValidThreshold = oldAssumeValid;
}

// ─── Orphan block cap (P1 — DoS) ──────────────────────────────────────────
// The cap on stored orphan blocks prevents an attacker from filling
// memory with garbage. If too low, legitimate orphans are dropped. If
// too high, a DoS vector opens.
BOOST_AUTO_TEST_CASE(orphan_block_caps_reasonable)
{
    BOOST_CHECK_GT(MAX_ORPHAN_BLOCKS, 0);
    BOOST_CHECK_GT(MAX_ORPHAN_BLOCKS_IBD, MAX_ORPHAN_BLOCKS);
    // IBD cap is typically ~2x normal to handle burst arrivals during
    // initial sync.
    BOOST_CHECK_LE(MAX_ORPHAN_BLOCKS_IBD, MAX_ORPHAN_BLOCKS * 4);
}

// ─── Fee constants (P2 — economic policy) ─────────────────────────────────
// Fees below MIN_TX_FEE must be rejected (DoS protection). MIN_RELAY_TX_FEE
// can be ≤ MIN_TX_FEE (relay tolerance is looser than mining tolerance).
BOOST_AUTO_TEST_CASE(fee_constants)
{
    BOOST_CHECK_GT(MIN_TX_FEE, 0);
    BOOST_CHECK_GT(MIN_RELAY_TX_FEE, 0);
    BOOST_CHECK_LE(MIN_RELAY_TX_FEE, MIN_TX_FEE * 100);  // sanity bound
    BOOST_CHECK_EQUAL(MIN_TX_FEE, CENT / 100);
    BOOST_CHECK_EQUAL(MIN_RELAY_TX_FEE, CENT / 100);
}

// ─── Block target spacing (P0) ────────────────────────────────────────────
// 120 seconds is the chain's identity. If it changes, every difficulty
// retarget computation will diverge → chain split.
BOOST_AUTO_TEST_CASE(target_spacing_immutable)
{
    BOOST_CHECK_EQUAL(nTargetSpacing, 120u);
    // 120s target = 2 min per block = 30 blocks/hour = 720 blocks/day
    // = 262800 blocks/year (720 * 365).
    int64_t blocksPerHour = 3600 / nTargetSpacing;   // 3600s/hr / 120s/block
    int64_t blocksPerDay = blocksPerHour * 24;
    int64_t blocksPerYear = blocksPerDay * 365;
    BOOST_CHECK_EQUAL(blocksPerHour, 30);
    BOOST_CHECK_EQUAL(blocksPerDay, 720);
    BOOST_CHECK_EQUAL(blocksPerYear, 262800);
}

BOOST_AUTO_TEST_SUITE_END()
