#include <boost/test/unit_test.hpp>

#include "../checkpoints.h"
#include "../uint256.h"

BOOST_AUTO_TEST_SUITE(Checkpoints_tests)

BOOST_AUTO_TEST_CASE(hardened_checkpoints_match_current_chain)
{
    BOOST_CHECK(Checkpoints::CheckHardened(0, uint256("0x7e7a6e4dd5fe895106fca912dfbacaeaf2a89e76c6a588df8ff96e0e18b96021")));
    BOOST_CHECK(Checkpoints::CheckHardened(9000, uint256("0x00000000019ef6b2f5e7c324c7d083ee94502305aabc7e9cd73a7fb2a57bb8db")));
    BOOST_CHECK(Checkpoints::CheckHardened(9001, uint256("0x6d5c6c5f201cc9e59659ee0da30d1430dc6bf3b12a8ff4c3864ab8d6286b0007")));
    BOOST_CHECK(Checkpoints::CheckHardened(2186940, uint256("0xbd952e8d4a612e336d840ad924a7e09395e36bcd9d929b302e47e60b5c3098c0")));
}

BOOST_AUTO_TEST_CASE(hardened_checkpoints_reject_wrong_hashes_and_allow_unknown_heights)
{
    const uint256 wrongHash("0x0000000000000000000000000000000000000000000000000000000000000001");

    BOOST_CHECK(!Checkpoints::CheckHardened(9000, wrongHash));
    BOOST_CHECK(!Checkpoints::CheckHardened(9001, wrongHash));
    BOOST_CHECK(!Checkpoints::CheckHardened(2186940, wrongHash));

    BOOST_CHECK(Checkpoints::CheckHardened(2186941, wrongHash));
    BOOST_CHECK(Checkpoints::CheckHardened(42, wrongHash));
}

BOOST_AUTO_TEST_CASE(total_blocks_estimate_tracks_latest_hardened_checkpoint)
{
    BOOST_CHECK(Checkpoints::GetTotalBlocksEstimate() >= 2186940);
}

BOOST_AUTO_TEST_SUITE_END()
