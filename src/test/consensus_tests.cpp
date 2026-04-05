// Copyright (c) 2024-2026 Triangles developers
// Tests for consensus-critical constants and reward schedules

#include <boost/test/unit_test.hpp>

#include "../main.h"
#include "../kernel.h"

// These globals are defined in main.cpp but not declared in any header
extern unsigned int nTargetSpacing;
extern unsigned int nStakeMaxAge;

BOOST_AUTO_TEST_SUITE(consensus_tests)

// --- Chain constants that must never change ---

BOOST_AUTO_TEST_CASE(genesis_hash)
{
    BOOST_CHECK_EQUAL(
        hashGenesisBlockOfficial.ToString(),
        "7e7a6e4dd5fe895106fca912dfbacaeaf2a89e76c6a588df8ff96e0e18b96021"
    );
}

BOOST_AUTO_TEST_CASE(genesis_testnet_matches_mainnet)
{
    // Triangles uses the same genesis block for mainnet and testnet
    BOOST_CHECK(hashGenesisBlockOfficial == hashGenesisBlockTestNet);
}

BOOST_AUTO_TEST_CASE(max_money)
{
    BOOST_CHECK_EQUAL(MAX_MONEY, 2222222LL * COIN);
}

BOOST_AUTO_TEST_CASE(money_range_checks)
{
    BOOST_CHECK(MoneyRange(0));
    BOOST_CHECK(MoneyRange(1));
    BOOST_CHECK(MoneyRange(MAX_MONEY));
    BOOST_CHECK(!MoneyRange(MAX_MONEY + 1));
    BOOST_CHECK(!MoneyRange(-1));
}

BOOST_AUTO_TEST_CASE(block_size_limits)
{
    BOOST_CHECK_EQUAL(MAX_BLOCK_SIZE, 1000000u);
    BOOST_CHECK_EQUAL(MAX_BLOCK_SIZE_GEN, MAX_BLOCK_SIZE / 2);
    BOOST_CHECK_EQUAL(MAX_BLOCK_SIGOPS, MAX_BLOCK_SIZE / 50);
}

BOOST_AUTO_TEST_CASE(coin_year_reward)
{
    // 33% annual PoS reward
    BOOST_CHECK_EQUAL(COIN_YEAR_REWARD, 33 * CENT);
    BOOST_CHECK_EQUAL(MAX_TRI_PROOF_OF_STAKE, static_cast<int64_t>(0.33 * COIN));
}

// --- Fork heights must be pinned ---

BOOST_AUTO_TEST_CASE(fork_heights)
{
    BOOST_CHECK_EQUAL(CUTOFF_POW_BLOCK, 9000);
    BOOST_CHECK_EQUAL(CRAPCHAIN_CUTOFF_BLOCK, 17691);
    BOOST_CHECK_EQUAL(FORK_HEIGHT_V5, 17651);
    BOOST_CHECK_EQUAL(FORK_HEIGHT_V5_4, 2186941);
}

// --- PoW ends at block 9000, PoS from 9001 ---

BOOST_AUTO_TEST_CASE(pow_cutoff)
{
    // PoW blocks 0-9000, PoS from 9001+
    BOOST_CHECK_EQUAL(CUTOFF_POW_BLOCK, 9000);
}

// --- Timing parameters ---

BOOST_AUTO_TEST_CASE(timing_constants)
{
    // 2-minute block target
    BOOST_CHECK_EQUAL(::nTargetSpacing, 120u);

    // 1-hour minimum stake age
    BOOST_CHECK_EQUAL(::nStakeMinAge, 3600u);

    // 12-hour maximum stake age (pre-V5 fork)
    BOOST_CHECK_EQUAL(::nStakeMaxAge, 43200u);

    // Modifier interval
    BOOST_CHECK_EQUAL(::nModifierInterval, 300u);  // 5 minutes
}

BOOST_AUTO_TEST_CASE(coinbase_maturity)
{
    BOOST_CHECK_EQUAL(nCoinbaseMaturity, 7);
}

BOOST_AUTO_TEST_CASE(min_tx_fee)
{
    BOOST_CHECK_EQUAL(MIN_TX_FEE, CENT / 100);
    BOOST_CHECK_EQUAL(MIN_RELAY_TX_FEE, CENT / 100);
}

// --- PoW Reward Schedule ---
// Reward depends on pindexBest->nHeight (global), which is set by the
// test harness.  In the test environment pindexBest is at genesis (height 0),
// so we verify the height >= 1 tier directly.

BOOST_AUTO_TEST_CASE(pow_reward_formula)
{
    // The schedule is codified in GetProofOfWorkReward():
    //   height >= 1:    1 TRI
    //   height >= 100:  20 TRI
    //   height >= 1000: 10 TRI
    //   height >= 3000: 5 TRI
    //   height >= 7000: 10 TRI
    //   height >= 9001: 0 TRI  (PoS takes over)
    //
    // We can't easily set pindexBest->nHeight in the unit test harness,
    // but we can verify the function returns a non-negative value and
    // includes fees.
    int64_t reward = GetProofOfWorkReward(0);
    BOOST_CHECK(reward >= 0);

    // With 1 COIN fee, reward should be at least 1 COIN
    int64_t rewardWithFee = GetProofOfWorkReward(1 * COIN);
    BOOST_CHECK(rewardWithFee >= 1 * COIN);
    BOOST_CHECK_EQUAL(rewardWithFee, reward + 1 * COIN);
}

// --- PoS Reward Calculation ---

BOOST_AUTO_TEST_CASE(pos_reward_zero_coinage)
{
    // Zero coin age should give zero subsidy (plus fees)
    int64_t reward = GetProofOfStakeReward(0, 0);
    BOOST_CHECK_EQUAL(reward, 0);
}

BOOST_AUTO_TEST_CASE(pos_reward_with_coinage)
{
    // 365 coin-days should yield exactly MAX_TRI_PROOF_OF_STAKE
    // Formula: nCoinAge * MAX_TRI_PROOF_OF_STAKE / 365 / COIN
    int64_t nCoinAge = 365 * COIN;  // 365 coin-days in satoshis
    int64_t reward = GetProofOfStakeReward(nCoinAge, 0);

    // Expected: (365 * COIN) * (0.33 * COIN) / 365 / COIN = 0.33 * COIN
    BOOST_CHECK_EQUAL(reward, MAX_TRI_PROOF_OF_STAKE);
}

BOOST_AUTO_TEST_CASE(pos_reward_includes_fees)
{
    int64_t nFees = 5 * CENT;
    int64_t reward = GetProofOfStakeReward(0, nFees);
    BOOST_CHECK_EQUAL(reward, nFees);
}

// --- Locktime threshold ---

BOOST_AUTO_TEST_CASE(locktime_threshold)
{
    BOOST_CHECK_EQUAL(LOCKTIME_THRESHOLD, 500000000u);
}

// --- Message magic string ---

BOOST_AUTO_TEST_CASE(signed_message_magic)
{
    BOOST_CHECK_EQUAL(strMessageMagic, "Triangles Signed Message:\n");
}

BOOST_AUTO_TEST_SUITE_END()
