#include <boost/test/unit_test.hpp>

#include "../checkpoints.h"
#include "../uint256.h"

BOOST_AUTO_TEST_SUITE(Checkpoints_tests)

BOOST_AUTO_TEST_CASE(hardened_checkpoints_match_current_chain)
{
    BOOST_CHECK(Checkpoints::CheckHardened(0, uint256("0x7e7a6e4dd5fe895106fca912dfbacaeaf2a89e76c6a588df8ff96e0e18b96021")));
    BOOST_CHECK(Checkpoints::CheckHardened(9000, uint256("0x00000000019ef6b2f5e7c324c7d083ee94502305aabc7e9cd73a7fb2a57bb8db")));
    BOOST_CHECK(Checkpoints::CheckHardened(9001, uint256("0x6d5c6c5f201cc9e59659ee0da30d1430dc6bf3b12a8ff4c3864ab8d6286b0007")));
    // Finality pins added 2026-07-01 (the old 2186940 pin was superseded).
    // After the operator rollback to 2,172,037 (cycle-32, 2026-08-06), the
    // 2205000/2206004 pins are no longer in the map (those block heights are
    // above the new canonical tip and reference non-existent blocks). The new
    // highest entry is 2172037.
    BOOST_CHECK(Checkpoints::CheckHardened(2172037, uint256("0x52b12f0970191505d9982449875822b78f075d7d76307abed45e7132f5fa2f16")));
}

BOOST_AUTO_TEST_CASE(hardened_checkpoints_reject_wrong_hashes_and_allow_unknown_heights)
{
    const uint256 wrongHash("0x0000000000000000000000000000000000000000000000000000000000000001");

    BOOST_CHECK(!Checkpoints::CheckHardened(9000, wrongHash));
    BOOST_CHECK(!Checkpoints::CheckHardened(9001, wrongHash));
    BOOST_CHECK(!Checkpoints::CheckHardened(2172037, wrongHash));

    // 2186940/2186941 are no longer pinned (superseded by the 2205000+
    // pins), and after the cycle-32 operator rollback the 2205000+ pins
    // themselves are gone. Any hash is allowed at those heights.
    BOOST_CHECK(Checkpoints::CheckHardened(2186940, wrongHash));
    BOOST_CHECK(Checkpoints::CheckHardened(2186941, wrongHash));
    BOOST_CHECK(Checkpoints::CheckHardened(2205000, wrongHash));
    BOOST_CHECK(Checkpoints::CheckHardened(2206004, wrongHash));
    BOOST_CHECK(Checkpoints::CheckHardened(42, wrongHash));
}

BOOST_AUTO_TEST_CASE(total_blocks_estimate_tracks_latest_hardened_checkpoint)
{
    // After operator rollback to 2,172,037, GetTotalBlocksEstimate() returns
    // 2,172,037 (the new highest compiled checkpoint).
    BOOST_CHECK_EQUAL(Checkpoints::GetTotalBlocksEstimate(), 2172037);
}

BOOST_AUTO_TEST_SUITE_END()
