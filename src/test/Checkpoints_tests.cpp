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
    // above the rollback tip and reference non-existent blocks). The rebase
    // base pin (2200899) and the rebase snapshot anchor (2201018, added
    // 2026-09-06) are the highest entries.
    BOOST_CHECK(Checkpoints::CheckHardened(2172037, uint256("0x52b12f0970191505d9982449875822b78f075d7d76307abed45e7132f5fa2f16")));
    BOOST_CHECK(Checkpoints::CheckHardened(2200899, uint256("0x28e57e03c7f48df8ef0dedba2b93fd5176500729c955f86546c381be66952e55")));
    BOOST_CHECK(Checkpoints::CheckHardened(2201018, uint256("0x2a1894007595acaa5d303554253b3c328ebc870f248ffebf83e09a4c8156a78f")));
}

BOOST_AUTO_TEST_CASE(hardened_checkpoints_reject_wrong_hashes_and_allow_unknown_heights)
{
    const uint256 wrongHash("0x0000000000000000000000000000000000000000000000000000000000000001");

    BOOST_CHECK(!Checkpoints::CheckHardened(9000, wrongHash));
    BOOST_CHECK(!Checkpoints::CheckHardened(9001, wrongHash));
    BOOST_CHECK(!Checkpoints::CheckHardened(2172037, wrongHash));
    BOOST_CHECK(!Checkpoints::CheckHardened(2200899, wrongHash));
    // Negative assertion for the rebase snapshot anchor pin (2026-09-06):
    // the height is hardened, so a wrong hash must be rejected.
    BOOST_CHECK(!Checkpoints::CheckHardened(2201018, wrongHash));

    // 2186940/2186941 are no longer pinned (superseded by the 2205000+
    // pins, which were themselves removed in the cycle-32 operator
    // rollback). Any hash is allowed at those heights.
    BOOST_CHECK(Checkpoints::CheckHardened(2186940, wrongHash));
    BOOST_CHECK(Checkpoints::CheckHardened(2186941, wrongHash));
    BOOST_CHECK(Checkpoints::CheckHardened(2205000, wrongHash));
    BOOST_CHECK(Checkpoints::CheckHardened(2206004, wrongHash));
    BOOST_CHECK(Checkpoints::CheckHardened(42, wrongHash));
}

BOOST_AUTO_TEST_CASE(total_blocks_estimate_tracks_latest_hardened_checkpoint)
{
    // After operator rollback to 2,172,037, GetTotalBlocksEstimate() returned
    // 2,172,037. Since the rebase snapshot anchor (2026-09-06), the highest
    // compiled checkpoint is 2,201,018.
    BOOST_CHECK_EQUAL(Checkpoints::GetTotalBlocksEstimate(), 2201018);
}

BOOST_AUTO_TEST_CASE(best_snapshot_is_canonical_rebase_snapshot)
{
    // The auto-download path (DownloadUtxoSnapshot) selects whatever
    // GetBestSnapshotHeight() returns and enforces the compiled (height, sha)
    // pair from mapSnapshotHashes. Lock both to the canonical rebase snapshot
    // (2026-09-06) so a retired entry can never be re-selected and a stale or
    // replayed bootstrap manifest cannot satisfy the gate with an old file.
    BOOST_CHECK_EQUAL(Checkpoints::GetBestSnapshotHeight(), 2201018);
    uint256 fileHash;
    BOOST_CHECK(Checkpoints::GetSnapshotHash(2201018, fileHash));
    BOOST_CHECK_EQUAL(fileHash.GetHex(),
                      "ed3fe84ee2388a7083873462af298bd4ba345ceb84e5ac65e3d2906419c0efab");
    // The retired snapshots must NOT be selectable: the 2172037 rollback-era
    // snapshot (removed 2026-09-06) and the 2200899 Sep-1 dump (writer/reader
    // serialization mismatch — unloadable on deployed binaries).
    BOOST_CHECK(!Checkpoints::GetSnapshotHash(2172037, fileHash));
    BOOST_CHECK(!Checkpoints::GetSnapshotHash(2200899, fileHash));
    // Cross-map invariant: the best snapshot height must sit on a hardened
    // checkpoint whose block hash matches the published snapshot's tip. This
    // prevents future snapshot/checkpoint drift — the two maps are written
    // together, and DownloadUtxoSnapshot requires BOTH gates to pass.
    BOOST_CHECK(Checkpoints::CheckHardened(
        2201018, uint256("0x2a1894007595acaa5d303554253b3c328ebc870f248ffebf83e09a4c8156a78f")));
}

BOOST_AUTO_TEST_SUITE_END()
