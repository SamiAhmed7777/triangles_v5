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
    BOOST_CHECK(Checkpoints::CheckHardened(2205000, uint256("0x6bdd3c5e5a32e1dd9a70e705f1a28d1dd84929f89579bd2696d41bc87f39446f")));
    BOOST_CHECK(Checkpoints::CheckHardened(2206004, uint256("0xb34e8e6a7bb7f52167d81aaad4d26f87a876898fdd0fce860916fc1aaf9a2a46")));
}

BOOST_AUTO_TEST_CASE(hardened_checkpoints_reject_wrong_hashes_and_allow_unknown_heights)
{
    const uint256 wrongHash("0x0000000000000000000000000000000000000000000000000000000000000001");

    BOOST_CHECK(!Checkpoints::CheckHardened(9000, wrongHash));
    BOOST_CHECK(!Checkpoints::CheckHardened(9001, wrongHash));
    BOOST_CHECK(!Checkpoints::CheckHardened(2205000, wrongHash));
    BOOST_CHECK(!Checkpoints::CheckHardened(2206004, wrongHash));

    // 2186940/2186941 are no longer pinned (superseded by the 2205000+
    // pins), so any hash is allowed at those heights.
    BOOST_CHECK(Checkpoints::CheckHardened(2186940, wrongHash));
    BOOST_CHECK(Checkpoints::CheckHardened(2186941, wrongHash));
    BOOST_CHECK(Checkpoints::CheckHardened(42, wrongHash));
}

BOOST_AUTO_TEST_CASE(total_blocks_estimate_tracks_latest_hardened_checkpoint)
{
    BOOST_CHECK(Checkpoints::GetTotalBlocksEstimate() >= 2205000);
}

BOOST_AUTO_TEST_SUITE_END()
