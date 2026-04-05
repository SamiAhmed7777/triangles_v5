// Copyright (c) 2024-2026 Triangles developers
// Tests for time drift functions (block timestamp validation)

#include <boost/test/unit_test.hpp>

#include "../main.h"

BOOST_AUTO_TEST_SUITE(time_drift_tests)

// --- GetMaxTimeDrift: different limits before and after V5.4 fork ---

BOOST_AUTO_TEST_CASE(max_drift_pre_v5_4)
{
    // Before FORK_HEIGHT_V5_4 (2186941): 10-minute drift allowed
    BOOST_CHECK_EQUAL(GetMaxTimeDrift(0), 10 * 60);
    BOOST_CHECK_EQUAL(GetMaxTimeDrift(1), 10 * 60);
    BOOST_CHECK_EQUAL(GetMaxTimeDrift(9000), 10 * 60);
    BOOST_CHECK_EQUAL(GetMaxTimeDrift(17650), 10 * 60);
    BOOST_CHECK_EQUAL(GetMaxTimeDrift(FORK_HEIGHT_V5_4 - 1), 10 * 60);
}

BOOST_AUTO_TEST_CASE(max_drift_at_v5_4_fork)
{
    // At exactly FORK_HEIGHT_V5_4: 3-minute drift (tighter)
    BOOST_CHECK_EQUAL(GetMaxTimeDrift(FORK_HEIGHT_V5_4), 3 * 60);
}

BOOST_AUTO_TEST_CASE(max_drift_post_v5_4)
{
    // After V5.4 fork: 3-minute drift
    BOOST_CHECK_EQUAL(GetMaxTimeDrift(FORK_HEIGHT_V5_4 + 1), 3 * 60);
    BOOST_CHECK_EQUAL(GetMaxTimeDrift(FORK_HEIGHT_V5_4 + 100000), 3 * 60);
    BOOST_CHECK_EQUAL(GetMaxTimeDrift(3000000), 3 * 60);
}

// --- PastDrift: time - maxDrift ---

BOOST_AUTO_TEST_CASE(past_drift_pre_fork)
{
    int64_t now = 1700000000;
    BOOST_CHECK_EQUAL(PastDrift(now, 0), now - 600);
    BOOST_CHECK_EQUAL(PastDrift(now, 9000), now - 600);
}

BOOST_AUTO_TEST_CASE(past_drift_post_fork)
{
    int64_t now = 1700000000;
    BOOST_CHECK_EQUAL(PastDrift(now, FORK_HEIGHT_V5_4), now - 180);
    BOOST_CHECK_EQUAL(PastDrift(now, FORK_HEIGHT_V5_4 + 1), now - 180);
}

// --- FutureDrift: time + maxDrift ---

BOOST_AUTO_TEST_CASE(future_drift_pre_fork)
{
    int64_t now = 1700000000;
    BOOST_CHECK_EQUAL(FutureDrift(now, 0), now + 600);
    BOOST_CHECK_EQUAL(FutureDrift(now, 9000), now + 600);
}

BOOST_AUTO_TEST_CASE(future_drift_post_fork)
{
    int64_t now = 1700000000;
    BOOST_CHECK_EQUAL(FutureDrift(now, FORK_HEIGHT_V5_4), now + 180);
    BOOST_CHECK_EQUAL(FutureDrift(now, FORK_HEIGHT_V5_4 + 1), now + 180);
}

// --- Symmetry: PastDrift and FutureDrift should be symmetric around the input ---

BOOST_AUTO_TEST_CASE(drift_symmetry)
{
    int64_t now = 1700000000;

    for (int height : {0, 1000, 17650, FORK_HEIGHT_V5_4 - 1, FORK_HEIGHT_V5_4, FORK_HEIGHT_V5_4 + 1})
    {
        int64_t past = PastDrift(now, height);
        int64_t future = FutureDrift(now, height);

        // FutureDrift - now should equal now - PastDrift
        BOOST_CHECK_EQUAL(future - now, now - past);

        // The drift window is 2 * GetMaxTimeDrift wide
        BOOST_CHECK_EQUAL(future - past, 2 * GetMaxTimeDrift(height));
    }
}

// --- Edge case: very small timestamps ---

BOOST_AUTO_TEST_CASE(drift_at_zero_time)
{
    // PastDrift with time 0 goes negative (which is fine for comparison)
    int64_t past = PastDrift(0, 0);
    BOOST_CHECK_EQUAL(past, -600);

    int64_t future = FutureDrift(0, 0);
    BOOST_CHECK_EQUAL(future, 600);
}

// --- Transaction validity: coinbase and coinstake identification ---

BOOST_AUTO_TEST_CASE(coinbase_identification)
{
    CTransaction tx;
    tx.nVersion = 1;
    tx.nTime = 1700000000;

    // Empty tx is not coinbase
    BOOST_CHECK(!tx.IsCoinBase());
    BOOST_CHECK(!tx.IsCoinStake());

    // Coinbase: single input with null prevout
    tx.vin.resize(1);
    tx.vin[0].prevout.SetNull();
    tx.vout.resize(1);
    tx.vout[0].nValue = 1 * COIN;
    tx.vout[0].scriptPubKey << OP_1;

    BOOST_CHECK(tx.IsCoinBase());
    BOOST_CHECK(!tx.IsCoinStake());
}

BOOST_AUTO_TEST_CASE(coinstake_identification)
{
    CTransaction tx;
    tx.nVersion = 1;
    tx.nTime = 1700000000;

    // Coinstake: first input is NOT null, first output is empty
    tx.vin.resize(1);
    tx.vin[0].prevout.hash = uint256("0x01");
    tx.vin[0].prevout.n = 0;
    tx.vout.resize(2);
    tx.vout[0].nValue = 0;
    tx.vout[0].scriptPubKey.clear();  // empty marker output (nValue=0, empty script)
    tx.vout[1].nValue = 50 * COIN;
    tx.vout[1].scriptPubKey << OP_1;

    BOOST_CHECK(!tx.IsCoinBase());
    BOOST_CHECK(tx.IsCoinStake());
}

// --- CheckTransaction: comprehensive validation ---

BOOST_AUTO_TEST_CASE(check_tx_empty_vin_fails)
{
    CTransaction tx;
    tx.nVersion = 1;
    tx.nTime = 1700000000;
    tx.vout.resize(1);
    tx.vout[0].nValue = 1 * COIN;
    tx.vout[0].scriptPubKey << OP_1;
    // vin is empty
    BOOST_CHECK(!tx.CheckTransaction());
}

BOOST_AUTO_TEST_CASE(check_tx_empty_vout_fails)
{
    CTransaction tx;
    tx.nVersion = 1;
    tx.nTime = 1700000000;
    tx.vin.resize(1);
    tx.vin[0].prevout.SetNull();
    tx.vin[0].scriptSig << std::vector<unsigned char>(10, 0);
    // vout is empty
    BOOST_CHECK(!tx.CheckTransaction());
}

BOOST_AUTO_TEST_CASE(check_tx_negative_value_fails)
{
    CTransaction tx;
    tx.nVersion = 1;
    tx.nTime = 1700000000;
    tx.vin.resize(1);
    tx.vin[0].prevout.hash = uint256("0x01");
    tx.vin[0].prevout.n = 0;
    tx.vin[0].scriptSig << std::vector<unsigned char>(65, 0);
    tx.vout.resize(1);
    tx.vout[0].nValue = -1;
    tx.vout[0].scriptPubKey << OP_1;

    BOOST_CHECK(!tx.CheckTransaction());
}

BOOST_AUTO_TEST_CASE(check_tx_over_max_money_fails)
{
    CTransaction tx;
    tx.nVersion = 1;
    tx.nTime = 1700000000;
    tx.vin.resize(1);
    tx.vin[0].prevout.hash = uint256("0x01");
    tx.vin[0].prevout.n = 0;
    tx.vin[0].scriptSig << std::vector<unsigned char>(65, 0);
    tx.vout.resize(1);
    tx.vout[0].nValue = MAX_MONEY + 1;
    tx.vout[0].scriptPubKey << OP_1;

    BOOST_CHECK(!tx.CheckTransaction());
}

BOOST_AUTO_TEST_SUITE_END()
