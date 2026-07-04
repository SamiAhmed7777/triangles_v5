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
    // Double the coin age should give double the reward
    int64_t r1 = GetProofOfStakeReward(100 * COIN, 0);
    int64_t r2 = GetProofOfStakeReward(200 * COIN, 0);

    BOOST_CHECK_EQUAL(r2, r1 * 2);
}

BOOST_AUTO_TEST_CASE(pos_reward_large_coinage)
{
    // Test with a large but valid coin age
    // 10000 coin-days = 10000 * COIN
    int64_t nCoinAge = 10000 * COIN;
    int64_t reward = GetProofOfStakeReward(nCoinAge, 0);

    // Expected: GetProofOfStakeReward now uses rounded integer division
    // (nCoinAge/COIN) * MAX_TRI_PROOF_OF_STAKE * 2 + 365) / 730 to preserve
    // exact linear proportionality. With nCoinAge=10000*COIN, the new value
    // is 9041096 (vs the old 9041095). This test documents the new value.
    int64_t nWholeCoinAge = nCoinAge / COIN;
    int64_t expected = (nWholeCoinAge * MAX_TRI_PROOF_OF_STAKE * 2 + 365) / (365 * 2);
    BOOST_CHECK_EQUAL(reward, expected);
    BOOST_CHECK(reward > 0);
}

BOOST_AUTO_TEST_SUITE_END()
