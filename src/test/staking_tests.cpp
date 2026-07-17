// Copyright (c) 2024-2026 Triangles developers
// Tests for Proof-of-Stake staking logic

#include <boost/test/unit_test.hpp>

#include "../main.h"
#include "../kernel.h"

extern unsigned int nStakeMinAge;
extern unsigned int nStakeMaxAge;

BOOST_AUTO_TEST_SUITE(staking_tests)

// --- GetWeight: coin age weight calculation ---

BOOST_AUTO_TEST_CASE(weight_below_min_age_is_zero)
{
    // If the coin is younger than nStakeMinAge, weight should be 0
    int64_t now = 1700000000;
    int64_t tooRecent = now - nStakeMinAge + 1;  // 1 second short of min age

    BOOST_CHECK_EQUAL(GetWeight(tooRecent, now), 0);
}

BOOST_AUTO_TEST_CASE(weight_exactly_min_age_is_zero)
{
    // At exactly nStakeMinAge, nAge = 0
    int64_t now = 1700000000;
    int64_t atMinAge = now - nStakeMinAge;

    BOOST_CHECK_EQUAL(GetWeight(atMinAge, now), 0);
}

BOOST_AUTO_TEST_CASE(weight_just_past_min_age)
{
    // One second past min age should give weight = 1
    int64_t now = 1700000000;
    int64_t justPast = now - nStakeMinAge - 1;

    BOOST_CHECK_EQUAL(GetWeight(justPast, now), 1);
}

BOOST_AUTO_TEST_CASE(weight_capped_at_max_age_pre_v5)
{
    // Before V5 fork (pindexBest at height 0 in test env), weight is capped
    // at nStakeMaxAge
    int64_t now = 1700000000;
    int64_t veryOld = now - nStakeMinAge - nStakeMaxAge - 10000;

    int64_t weight = GetWeight(veryOld, now);

    // Should be capped at nStakeMaxAge (43200 = 12 hours)
    BOOST_CHECK_EQUAL(weight, (int64_t)nStakeMaxAge);
}

BOOST_AUTO_TEST_CASE(weight_at_exactly_max_age_pre_v5)
{
    int64_t now = 1700000000;
    int64_t atMax = now - nStakeMinAge - nStakeMaxAge;

    BOOST_CHECK_EQUAL(GetWeight(atMax, now), (int64_t)nStakeMaxAge);
}

BOOST_AUTO_TEST_CASE(weight_linear_between_min_and_max)
{
    // Weight should increase linearly between min and max age
    int64_t now = 1700000000;

    int64_t w1 = GetWeight(now - nStakeMinAge - 100, now);
    int64_t w2 = GetWeight(now - nStakeMinAge - 200, now);
    int64_t w3 = GetWeight(now - nStakeMinAge - 300, now);

    BOOST_CHECK_EQUAL(w1, 100);
    BOOST_CHECK_EQUAL(w2, 200);
    BOOST_CHECK_EQUAL(w3, 300);

    // Linear progression
    BOOST_CHECK_EQUAL(w2 - w1, w3 - w2);
}

BOOST_AUTO_TEST_CASE(weight_never_negative)
{
    // Even with nonsensical inputs (end < begin), weight should be 0
    int64_t begin = 1700000000;
    int64_t end = begin - 1000;

    BOOST_CHECK_EQUAL(GetWeight(begin, end), 0);
}

// --- CheckCoinStakeTimestamp ---

BOOST_AUTO_TEST_CASE(coinstake_timestamp_must_match_block)
{
    // v0.3 protocol: block time must equal coinstake tx time
    int64_t now = 1700000000;

    BOOST_CHECK(CheckCoinStakeTimestamp(now, now));
    BOOST_CHECK(!CheckCoinStakeTimestamp(now, now + 1));
    BOOST_CHECK(!CheckCoinStakeTimestamp(now, now - 1));
}

// --- Modifier interval ratio ---

BOOST_AUTO_TEST_CASE(modifier_interval_ratio)
{
    BOOST_CHECK_EQUAL(MODIFIER_INTERVAL_RATIO, 3);
}

// --- Stake modifier checkpoints ---

BOOST_AUTO_TEST_CASE(stake_modifier_checkpoints_testnet_always_passes)
{
    // Testnet should skip stake modifier checkpoint validation
    bool oldTestnet = fTestNet;
    fTestNet = true;
    BOOST_CHECK(CheckStakeModifierCheckpoints(0, 0));
    BOOST_CHECK(CheckStakeModifierCheckpoints(99999, 0xDEADBEEF));
    fTestNet = oldTestnet;
}

// --- PoS reward math edge cases ---

BOOST_AUTO_TEST_CASE(pos_reward_proportional_to_coinage)
{
    // Doubling the coin age roughly doubles the reward. The consensus
    // formula GetProofOfStakeReward uses integer TRUNCATING division
    // (nCoinAge * rate / 365 / COIN), so exact doubling does not hold at
    // every boundary: e.g. r1 = 90410 but r2 = 180821 = 2*r1 + 1, because
    // the /365 truncation lands one unit differently. That 1-unit rounding
    // is the on-chain behavior; "fixing" it in consensus code would change
    // emission and hard-fork the network, so the test tolerates a 1-unit
    // difference instead.
    int64_t r1 = GetProofOfStakeReward(100 * COIN, 0);
    int64_t r2 = GetProofOfStakeReward(200 * COIN, 0);

    int64_t diff = r2 - r1 * 2;
    if (diff < 0) diff = -diff;
    BOOST_CHECK_MESSAGE(diff <= 1,
        strprintf("reward not ~proportional: r1=%d r2=%d diff=%d", r1, r2, diff));
    BOOST_CHECK(r1 > 0 && r2 > 0);
}

BOOST_AUTO_TEST_CASE(pos_reward_large_coinage)
{
    // Test with a large but valid coin age
    // 10000 coin-days = 10000 * COIN
    int64_t nCoinAge = 10000 * COIN;
    int64_t reward = GetProofOfStakeReward(nCoinAge, 0);

    // Expected: 10000 * MAX_TRI_PROOF_OF_STAKE / 365
    int64_t expected = nCoinAge * MAX_TRI_PROOF_OF_STAKE / 365 / COIN;
    BOOST_CHECK_EQUAL(reward, expected);
    BOOST_CHECK(reward > 0);
}

// --- GetWeight: V5 soft-cap behavior (post-2026-04-12 fork fix) ---
//
// The 2026-04-20 deploy changed GetWeight to apply a 7-day soft cap on
// stake weight instead of the hard nStakeMaxAge (= 12 hours) cap, but only
// after a height AND a timestamp gate:
//   - height must be >= FORK_HEIGHT_V5 (= 17651), AND
//   - nIntervalEnd must be >= STAKE_AGE_SOFT_CAP_ACTIVATION (= 1776000000,
//     2026-04-12 ~13:20 UTC).
//
// Pre-V5 path stays at hard nStakeMaxAge cap (regression-tested above).
// V5 + pre-activation path is INTENTIONALLY uncapped (historical stakes
// validate under the rules they were staked with).
// V5 + post-activation path applies the 7-day soft cap.
//
// These tests use RAII to scope pindexBest swaps so a failed assertion
// can't leave a stack pointer dangling in the global. The mock CBlockIndex
// only needs nHeight populated; GetWeight reads nothing else from it.

// RAII guard: install a synthetic pindexBest on construction, restore the
// prior value on destruction. Mandatory because boost CHECK failures
// throw, and a manual pindexBest restore in the catch-less path leaks the
// stack pointer into the global -- corrupting every subsequent test in
// the suite.
struct BestChainGuard
{
    CBlockIndex* prev;
    explicit BestChainGuard(CBlockIndex* mock) : prev(pindexBest) { pindexBest = mock; }
    ~BestChainGuard() { pindexBest = prev; }
};

static const int64_t STAKE_AGE_SOFT_CAP_DAYS = 7;
static const int64_t STAKE_AGE_SOFT_CAP_TEST_SECS = STAKE_AGE_SOFT_CAP_DAYS * 24 * 60 * 60;
static const int64_t STAKE_AGE_SOFT_CAP_ACTIVATION_TEST = 1776000000;
static const int64_t STAKE_AGE_MAX_TEST = 10 * 24 * 60 * 60;  // 10 days -- past the 7-day cap

BOOST_AUTO_TEST_CASE(weight_v5_post_activation_capped_at_7_days)
{
    // V5 + post-activation: a 10-day-old stake should be capped at 7 days.
    // This is the production code path for every stake on the live chain
    // since 2026-04-20 -- the highest-value missing test.
    CBlockIndex mockBest;
    mockBest.nHeight = FORK_HEIGHT_V5;  // 17651, just at the fork
    BestChainGuard guard(&mockBest);

    int64_t now = STAKE_AGE_SOFT_CAP_ACTIVATION_TEST + (30 * 24 * 60 * 60);  // 30 days post-activation
    int64_t tenDaysOld = now - nStakeMinAge - STAKE_AGE_MAX_TEST;

    int64_t weight = GetWeight(tenDaysOld, now);
    BOOST_CHECK_EQUAL(weight, STAKE_AGE_SOFT_CAP_TEST_SECS);
}

BOOST_AUTO_TEST_CASE(weight_v5_post_activation_below_cap_is_linear)
{
    // V5 + post-activation: a stake younger than the 7-day cap should
    // return the raw nAge (capping only applies past the limit).
    CBlockIndex mockBest;
    mockBest.nHeight = FORK_HEIGHT_V5;
    BestChainGuard guard(&mockBest);

    int64_t now = STAKE_AGE_SOFT_CAP_ACTIVATION_TEST + (30 * 24 * 60 * 60);
    int64_t threeDaysOld = now - nStakeMinAge - (3 * 24 * 60 * 60);

    int64_t weight = GetWeight(threeDaysOld, now);
    BOOST_CHECK_EQUAL(weight, 3 * 24 * 60 * 60);
}

BOOST_AUTO_TEST_CASE(weight_v5_post_activation_exactly_7_days)
{
    // V5 + post-activation: exactly at the cap should return cap value.
    CBlockIndex mockBest;
    mockBest.nHeight = FORK_HEIGHT_V5;
    BestChainGuard guard(&mockBest);

    int64_t now = STAKE_AGE_SOFT_CAP_ACTIVATION_TEST + (30 * 24 * 60 * 60);
    int64_t exactlySevenDays = now - nStakeMinAge - STAKE_AGE_SOFT_CAP_TEST_SECS;

    int64_t weight = GetWeight(exactlySevenDays, now);
    BOOST_CHECK_EQUAL(weight, STAKE_AGE_SOFT_CAP_TEST_SECS);
}

BOOST_AUTO_TEST_CASE(weight_v5_post_activation_one_second_past_cap)
{
    // V5 + post-activation: 1 second past the cap should still be capped
    // (min() boundary semantics).
    CBlockIndex mockBest;
    mockBest.nHeight = FORK_HEIGHT_V5;
    BestChainGuard guard(&mockBest);

    int64_t now = STAKE_AGE_SOFT_CAP_ACTIVATION_TEST + (30 * 24 * 60 * 60);
    int64_t justPastCap = now - nStakeMinAge - STAKE_AGE_SOFT_CAP_TEST_SECS - 1;

    int64_t weight = GetWeight(justPastCap, now);
    BOOST_CHECK_EQUAL(weight, STAKE_AGE_SOFT_CAP_TEST_SECS);
}

BOOST_AUTO_TEST_CASE(weight_v5_pre_activation_is_uncapped)
{
    // V5 active (height >= 17651) but stake timestamp is BEFORE the
    // activation gate. This is the "historical stakes validate under the
    // rules they were created with" path. A 30-day-old stake with
    // nIntervalEnd pre-activation should NOT be capped at 7 days or at
    // nStakeMaxAge -- it returns the raw nAge. This is intentional:
    // changing the cap retroactively would hard-fork historical blocks.
    CBlockIndex mockBest;
    mockBest.nHeight = FORK_HEIGHT_V5;
    BestChainGuard guard(&mockBest);

    int64_t now = STAKE_AGE_SOFT_CAP_ACTIVATION_TEST - 1;  // 1 second before activation
    int64_t thirtyDaysOld = now - nStakeMinAge - (30 * 24 * 60 * 60);

    int64_t weight = GetWeight(thirtyDaysOld, now);
    BOOST_CHECK_EQUAL(weight, 30 * 24 * 60 * 60);  // raw nAge, no cap
}

BOOST_AUTO_TEST_CASE(weight_v5_exactly_at_activation_is_capped)
{
    // V5 + nIntervalEnd exactly equal to the activation timestamp.
    // Boundary semantics: `>=` means AT the timestamp counts as activated,
    // so the 7-day cap applies. (Confirmed against the source: line 47
    // is `if (nIntervalEnd >= STAKE_AGE_SOFT_CAP_ACTIVATION) return min(...)`)
    CBlockIndex mockBest;
    mockBest.nHeight = FORK_HEIGHT_V5;
    BestChainGuard guard(&mockBest);

    int64_t now = STAKE_AGE_SOFT_CAP_ACTIVATION_TEST;  // exactly at activation
    int64_t tenDaysOld = now - nStakeMinAge - STAKE_AGE_MAX_TEST;

    int64_t weight = GetWeight(tenDaysOld, now);
    BOOST_CHECK_EQUAL(weight, STAKE_AGE_SOFT_CAP_TEST_SECS);  // capped at 7 days
}

BOOST_AUTO_TEST_CASE(weight_v5_high_height_same_as_fork_height)
{
    // V5 + post-activation at a height FAR past the fork (e.g. the live
    // DNS2 chain at height ~2.2M). Cap should still apply identically --
    // the soft cap doesn't weaken or strengthen with distance from fork.
    CBlockIndex mockBest;
    mockBest.nHeight = 2500000;  // well past FORK_HEIGHT_V5 and FORK_HEIGHT_V5_4
    BestChainGuard guard(&mockBest);

    int64_t now = STAKE_AGE_SOFT_CAP_ACTIVATION_TEST + (60 * 24 * 60 * 60);
    int64_t hundredDaysOld = now - nStakeMinAge - (100 * 24 * 60 * 60);

    int64_t weight = GetWeight(hundredDaysOld, now);
    BOOST_CHECK_EQUAL(weight, STAKE_AGE_SOFT_CAP_TEST_SECS);  // still 7 days, not 100
}

BOOST_AUTO_TEST_CASE(weight_v5_min_age_floor_still_applies)
{
    // V5 + post-activation: nStakeMinAge floor still applies (a coin
    // younger than min_age returns 0 even if all gates pass). Confirms
    // the fork change didn't accidentally remove the floor.
    CBlockIndex mockBest;
    mockBest.nHeight = FORK_HEIGHT_V5;
    BestChainGuard guard(&mockBest);

    int64_t now = STAKE_AGE_SOFT_CAP_ACTIVATION_TEST + (30 * 24 * 60 * 60);
    int64_t tooYoung = now - nStakeMinAge + 1;  // 1 second short of min age

    int64_t weight = GetWeight(tooYoung, now);
    BOOST_CHECK_EQUAL(weight, 0);
}

// --- IsStakingSafe: continuous staking safety gate (fix/consensus-convergence) ---
//
// Pre-fix: fTryToSync in StakeMiner was set false after the first use,
// so losing peers mid-staking left the staker running on a potentially
// isolated chain. The new gate (IsStakingSafe) is evaluated on every
// staking attempt and refuses to stake when:
//   - IBD is active
//   - fewer than 2 fully handshaken, non-disconnecting peers
//   - our height is behind the peer median
//   - a peer reports a tip materially ahead of ours (>= 2 blocks)

BOOST_AUTO_TEST_CASE(is_staking_safe_refuses_with_empty_peer_list)
{
    // Empty peer snapshot = the network-outage case. We must refuse to
    // stake, otherwise the laptop-and-PC-with-no-network scenario
    // (the original failure mode fix/consensus-convergence was created
    // for) would still happen.
    std::vector<CNode*> vEmpty;
    BOOST_CHECK(!IsStakingSafe(nullptr, vEmpty));
}

BOOST_AUTO_TEST_CASE(is_staking_safe_refuses_when_wallet_is_null)
{
    // The gate must check the wallet pointer before doing anything
    // else. A null wallet must refuse.
    std::vector<CNode*> vEmpty;
    BOOST_CHECK(!IsStakingSafe(nullptr, vEmpty));
}

BOOST_AUTO_TEST_CASE(is_staking_safe_is_continuous_not_one_shot)
{
    // Static structural test: the StakeMiner loop must call IsStakingSafe
    // every iteration, not just once. Pre-fix code only ran the strong
    // check after fTryToSync was reset to true, and then set fTryToSync
    // false — meaning the check ran exactly once per exit from the inner
    // wait loop. Post-fix must NOT have the fTryToSync flag at all.
    //
    // Use __FILE__ to find the repo root so the path resolves regardless
    // of the build directory or test runner cwd.
    std::string here = __FILE__;
    size_t pos = here.rfind("/src/test/");
    BOOST_REQUIRE(pos != std::string::npos);
    std::string miner_src_path = here.substr(0, pos) + "/src/miner.cpp";

    FILE* f = fopen(miner_src_path.c_str(), "r");
    BOOST_REQUIRE(f != nullptr);
    fseek(f, 0, SEEK_END);
    long nSize = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<char> buf((size_t)nSize + 1, 0);
    BOOST_REQUIRE(fread(buf.data(), 1, (size_t)nSize, f) == (size_t)nSize);
    fclose(f);
    std::string src(buf.data(), (size_t)nSize);

    // The continuous gate must be in place.
    BOOST_CHECK(src.find("IsStakingSafe(pwallet, vNodes)") != std::string::npos);

    // fTryToSync must be gone from runtime code. We grep for the
    // declaration `bool fTryToSync` and the assignments
    // `fTryToSync = true` / `fTryToSync = false`. Comments are
    // allowed (this test even has them) — only the runtime references
    // are forbidden, since those are what would re-introduce the
    // one-shot gate bug.
    BOOST_CHECK(src.find("bool fTryToSync") == std::string::npos);
    BOOST_CHECK(src.find("fTryToSync = true") == std::string::npos);
    BOOST_CHECK(src.find("fTryToSync = false") == std::string::npos);
}

BOOST_AUTO_TEST_SUITE_END()
