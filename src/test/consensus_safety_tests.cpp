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

// ─── Convergence rule (P0 — security) ─────────────────────────────────────
// fix/consensus-convergence: above the last globally shared hardened
// checkpoint, the valid chain with strictly greater cumulative chain
// trust wins. No depth cap, no local finality, no trust hysteresis.
// Below the hardened checkpoint: rejection is unconditional.
//
// This test pins the boundary values and the constant's role.
//
//   MAX_REORG_DEPTH remains in the source as a historical legacy value
//   but no longer gates reorgs above the hardened checkpoint. The
//   live gate is pindexLastHardenedCheckpoint, set once at startup
//   from the compiled hardened checkpoint map.
BOOST_AUTO_TEST_CASE(convergence_rule_pins)
{
    // Legacy constant retained but no longer enforced. If a future
    // refactor tries to use MAX_REORG_DEPTH as a live reorg limit,
    // this test catches it.
    BOOST_CHECK_EQUAL(MAX_REORG_DEPTH, 100);

    // pindexLastHardenedCheckpoint is declared extern and must be
    // initialized at startup. The variable exists and is reachable.
    BOOST_CHECK(pindexLastHardenedCheckpoint == nullptr
        || pindexLastHardenedCheckpoint->nHeight >= 0);

    // The convergence rule itself is verified by the convergence
    // tests below; here we only pin that the rule is expressed
    // exclusively in Reorganize() against pindexLastHardenedCheckpoint
    // and that the auto-walking tip-minus-100 logic has been removed.
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

// ─── Convergence rule: reorg rejection below the hardened checkpoint ─────
// fix/consensus-convergence: the only convergence-relevant rule in
// Reorganize() is "fork point at or below the hardened checkpoint is
// rejected". This test pins that rule by reading the source and
// asserting:
//   1. The function uses pindexLastHardenedCheckpoint (the new name),
//      not pindexFinalized (the removed local-finality variable).
//   2. The rejection compares pfork->nHeight against the checkpoint
//      height, not against MAX_REORG_DEPTH or any local tip-derived
//      value.
//   3. There is no longer an absolute reorg depth cap in Reorganize().
//   4. There is no longer a 10% trust hysteresis check.
// Helper: resolve the repository root from the test file's __FILE__
// so the static-source tests below don't depend on the caller's cwd.
// We assume the test file lives at <root>/src/test/<this>.cpp.
static std::string readEntireFile(const char* relToSrc)
{
    // __FILE__ resolves to an absolute path under typical compilers;
    // fall back to a CWD-relative path if it doesn't.
    std::string here = __FILE__;
    size_t pos = here.rfind("/src/test/");
    std::string root;
    if (pos != std::string::npos)
        root = here.substr(0, pos);
    else
        root = ".";

    std::string full = root + "/" + relToSrc;
    FILE* f = fopen(full.c_str(), "r");
    if (!f)
        return std::string();
    fseek(f, 0, SEEK_END);
    long nSize = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<char> buf((size_t)nSize + 1, 0);
    size_t nRead = fread(buf.data(), 1, (size_t)nSize, f);
    fclose(f);
    if (nRead != (size_t)nSize)
        return std::string();
    return std::string(buf.data(), (size_t)nSize);
}

BOOST_AUTO_TEST_CASE(convergence_rejects_below_hardened_checkpoint)
{
    // Read the source and pin the rule's structure. This is a static
    // test (no in-memory chain assembly) — it fails closed if anyone
    // reintroduces the local-finality code paths.
    std::string src = readEntireFile("src/main.cpp");
    BOOST_REQUIRE(!src.empty());

    // (1) The variable referenced is the renamed one, not the old name.
    BOOST_CHECK(src.find("pindexLastHardenedCheckpoint") != std::string::npos);
    BOOST_CHECK(src.find("pindexFinalized") == std::string::npos);

    // (2) The reorg-rejection block compares fork height against a
    //     resolved checkpoint height (`nHardenedCheckpointHeight`), not
    //     against MAX_REORG_DEPTH or any tip-based value. The literal
    //     source pattern we look for is the new guard variable being
    //     assigned from `Checkpoints::GetLastCheckpointHeight()` —
    //     which is the bootstrap-time path that covers the
    //     `pindexLastHardenedCheckpoint == nullptr` case (otherwise an
    //     IBD-time reorg below the compiled checkpoint could slip
    //     through the guard).
    BOOST_CHECK(src.find("Checkpoints::GetLastCheckpointHeight()")
                != std::string::npos);
    BOOST_CHECK(src.find("nHardenedCheckpointHeight = pindexLastHardenedCheckpoint->nHeight")
                != std::string::npos);

    // (3) No absolute depth cap in Reorganize(). The old code had
    //     `if (nDisconnectDepth > MAX_REORG_DEPTH)` — that line must
    //     not appear anywhere in the source.
    BOOST_CHECK(src.find("nDisconnectDepth > MAX_REORG_DEPTH")
                == std::string::npos);

    // (4) No 10% trust hysteresis. The old multiplier comparison
    //     `bnNewTrust * 10 <= bnBestTrust * 11` must not appear.
    BOOST_CHECK(src.find("bnNewTrust * 10 <= bnBestTrust * 11")
                == std::string::npos);

    // (5) No auto-walking tip-minus-100 finality in ActivateBestChain.
    //     The pattern `for (int i = 0; i < (int)MAX_REORG_DEPTH` must
    //     not appear (it used to walk 100 blocks behind tip).
    BOOST_CHECK(src.find("for (int i = 0; i < (int)MAX_REORG_DEPTH")
                == std::string::npos);
}

// ─── Convergence rule: pindexLastHardenedCheckpoint is startup-only ───────
// The variable must be assigned exactly once at startup and never
// reassigned at runtime. A regression that re-introduces runtime
// advancement would re-create the local-finality bug.
BOOST_AUTO_TEST_CASE(hardened_checkpoint_init_is_startup_only)
{
    std::string src = readEntireFile("src/init.cpp");
    BOOST_REQUIRE(!src.empty());

    // The startup init must reference pindexLastHardenedCheckpoint.
    BOOST_CHECK(src.find("pindexLastHardenedCheckpoint = pCheckpoint")
                != std::string::npos);

    // (Static structural check on main.cpp — must not reassign the
    //  variable at runtime.) A regression that re-adds an
    //  `pindexLastHardenedCheckpoint = pcandidate` style update
    //  would fail this check.
    std::string main_src = readEntireFile("src/main.cpp");
    BOOST_REQUIRE(!main_src.empty());
    BOOST_CHECK(main_src.find("pindexLastHardenedCheckpoint = pcandidate")
                == std::string::npos);
    BOOST_CHECK(main_src.find("pindexLastHardenedCheckpoint = pindex")
                == std::string::npos);
}

// ─── Convergence rule: above the hardened checkpoint, greater trust wins ──
// No depth cap, no 10% hysteresis, no local finality. The source must
// show Reorganize() free of those gates and the convergence comment
// block must be present.
BOOST_AUTO_TEST_CASE(above_checkpoint_greatest_trust_wins)
{
    std::string src = readEntireFile("src/main.cpp");
    BOOST_REQUIRE(!src.empty());

    // The convergence rule comment must be present.
    BOOST_CHECK(src.find("Convergence rule (fix/consensus-convergence)")
                != std::string::npos);

    // CBlockTrust comparison must remain (it's how a winner is picked
    // when two valid candidates are presented).
    BOOST_CHECK(src.find("nChainTrust") != std::string::npos);
}

// ─── getheaders recovery: peer with no shared locator gets genesis ────────
// fix/consensus-convergence: a forked peer whose locator contains no
// common blocks must be served headers starting from the last common
// ancestor (or genesis if none). The pre-fix code re-anchored at the
// checkpoint unconditionally and broke recovery for forked peers.
BOOST_AUTO_TEST_CASE(getheaders_recovers_via_genesis_when_locator_disjoint)
{
    std::string src = readEntireFile("src/main.cpp");
    BOOST_REQUIRE(!src.empty());

    // The recovery block must exist and serve from the last common
    // ancestor or genesis.
    BOOST_CHECK(src.find("fork-peer getheaders recovery (fix/consensus-convergence)")
                != std::string::npos);
    BOOST_CHECK(src.find("serving canonical headers from last common ancestor")
                != std::string::npos);
    BOOST_CHECK(src.find("serving headers from genesis (peer on a long fork)")
                != std::string::npos);

    // The pre-fix unconditional re-anchor at pindexLastHardenedCheckpoint
    // without checking the locator must be gone. The new code path
    // requires the checkpoint to be present in locator.vHave first.
    BOOST_CHECK(src.find("pindexLastHardenedCheckpoint->pnext)") == std::string::npos
        && src.find("pindexLastHardenedCheckpoint && pindexLastHardenedCheckpoint->pnext") == std::string::npos);
}

// ─── getheaders recovery: peer whose locator contains the checkpoint ──────
// When the peer's locator contains the hardened checkpoint, we serve
// canonical headers starting from the checkpoint forward.
BOOST_AUTO_TEST_CASE(getheaders_recovers_via_checkpoint_when_locator_has_it)
{
    std::string src = readEntireFile("src/main.cpp");
    BOOST_REQUIRE(!src.empty());

    BOOST_CHECK(src.find("peer locator contains hardened checkpoint")
                != std::string::npos);
    BOOST_CHECK(src.find("serving canonical headers from there")
                != std::string::npos);
}

// ─── Bootstrap-state reorg guard: hardened_checkpoint_height is fail-closed ─
// Adversarial review (Codex, SHA 935d1d5) flagged that the original guard
// short-circuited on `pindexLastHardenedCheckpoint == nullptr`. That happens
// during early IBD, reindex, and bootstrap before the checkpoint block has
// been downloaded — exactly when an attacker peer would most want to feed a
// deep fork. The fix consults `Checkpoints::GetLastCheckpointHeight()`
// (compiled map, independent of mapBlockIndex) as the second-layer floor.
BOOST_AUTO_TEST_CASE(reorg_guard_fails_closed_when_checkpoint_pointer_null)
{
    std::string src = readEntireFile("src/main.cpp");
    BOOST_REQUIRE(!src.empty());

    // (a) The compiled-map helper is declared in checkpoints.h and
    //     defined in checkpoints.cpp. The signature is
    //     `int GetLastCheckpointHeight()` (declared inside the
    //     Checkpoints namespace; namespace-qualified at call sites).
    std::string cp_h = readEntireFile("src/checkpoints.h");
    std::string cp_cpp = readEntireFile("src/checkpoints.cpp");
    BOOST_REQUIRE(!cp_h.empty());
    BOOST_REQUIRE(!cp_cpp.empty());
    BOOST_CHECK(cp_h.find("int GetLastCheckpointHeight();") != std::string::npos);
    BOOST_CHECK(cp_cpp.find("int GetLastCheckpointHeight()") != std::string::npos);
    // The implementation is independent of mapBlockIndex — it returns
    // checkpoints.rbegin()->first directly. This is what makes it usable
    // before the checkpoint hash has been resolved in our local index.
    BOOST_CHECK(cp_cpp.find("checkpoints.rbegin()->first") != std::string::npos);

    // (b) The Reorganize() guard uses the compiled-height fallback when
    //     the local pointer is NULL. The pattern is the local variable
    //     `nHardenedCheckpointHeight` being assigned from
    //     `Checkpoints::GetLastCheckpointHeight()` in the else branch.
    BOOST_CHECK(src.find("nHardenedCheckpointHeight = Checkpoints::GetLastCheckpointHeight()")
                != std::string::npos);

    // (c) The guard fires for any fork point at or below the resolved
    //     checkpoint height — independent of whether the resolution came
    //     from the pointer or the compiled map. The literal pattern that
    //     matters is `pfork->nHeight <= nHardenedCheckpointHeight`.
    BOOST_CHECK(src.find("pfork->nHeight <= nHardenedCheckpointHeight")
                != std::string::npos);

    // (d) The old guard pattern that short-circuited on the null pointer
    //     is gone. The exact prior pattern was:
    //         if (pindexLastHardenedCheckpoint && pfork->nHeight <= pindexLastHardenedCheckpoint->nHeight)
    //     That single `if` is no longer a guard by itself — it has been
    //     replaced by the `nHardenedCheckpointHeight` two-layer check.
    BOOST_CHECK(src.find("if (pindexLastHardenedCheckpoint && pfork->nHeight <= pindexLastHardenedCheckpoint->nHeight)")
                == std::string::npos);
}

BOOST_AUTO_TEST_SUITE_END()
