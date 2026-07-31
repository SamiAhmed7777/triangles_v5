// Copyright (c) 2026 Triangles developers
// Distributed under the MIT/X11 software license

#include <boost/test/unit_test.hpp>

#include "../bootstrap.h"

namespace {

std::string ManifestWithFilename(const std::string& filename)
{
    return std::string(R"json({
        "version": "1.6",
        "chain_tip": {
            "height": 2206004,
            "blockhash": "b34e8e6a7bb7f52167d81aaad4d26f87a876898fdd0fce860916fc1aaf9a2a46"
        },
        "files": {
            ")json") + filename + R"json(": {
                "sha256": "1419282DAE817315EE1B955543F6248233FE5800F5E8488734A0ECE5BD6781EA",
                "type": "utxo_snapshot_v3"
            }
        },
        "canonical": { "snapshot": ")json" + filename + R"json(" }
    })json";
}

} // namespace

BOOST_AUTO_TEST_SUITE(bootstrap_security_tests)

BOOST_AUTO_TEST_CASE(remote_manifest_parses_canonical_snapshot)
{
    Bootstrap::RemoteSnapshot snapshot;
    std::string error;
    BOOST_REQUIRE(Bootstrap::ParseRemoteSnapshotManifest(
        ManifestWithFilename("utxo-snapshot.bin"), snapshot, error));
    BOOST_CHECK_EQUAL(snapshot.filename, "utxo-snapshot.bin");
    BOOST_CHECK_EQUAL(snapshot.height, 2206004);
    BOOST_CHECK_EQUAL(
        snapshot.sha256,
        "1419282dae817315ee1b955543f6248233fe5800f5e8488734a0ece5bd6781ea");
}

BOOST_AUTO_TEST_CASE(remote_manifest_rejects_path_traversal)
{
    Bootstrap::RemoteSnapshot snapshot;
    std::string error;
    BOOST_CHECK(!Bootstrap::ParseRemoteSnapshotManifest(
        ManifestWithFilename("../../wallet.dat"), snapshot, error));
    BOOST_CHECK_NE(error.find("plain filename"), std::string::npos);
}

BOOST_AUTO_TEST_CASE(remote_manifest_rejects_malformed_hashes)
{
    std::string manifest = ManifestWithFilename("utxo-snapshot.bin");
    const std::string validHash =
        "1419282DAE817315EE1B955543F6248233FE5800F5E8488734A0ECE5BD6781EA";
    manifest.replace(manifest.find(validHash), validHash.size(), "not-a-sha256");

    Bootstrap::RemoteSnapshot snapshot;
    std::string error;
    BOOST_CHECK(!Bootstrap::ParseRemoteSnapshotManifest(manifest, snapshot, error));
    BOOST_CHECK_NE(error.find("invalid"), std::string::npos);
}

BOOST_AUTO_TEST_CASE(remote_manifest_rejects_non_snapshot_canonical_file)
{
    std::string manifest = ManifestWithFilename("wallet.dat");
    const std::string snapshotType = "utxo_snapshot_v3";
    manifest.replace(manifest.find(snapshotType), snapshotType.size(), "wallet_backup");

    Bootstrap::RemoteSnapshot snapshot;
    std::string error;
    BOOST_CHECK(!Bootstrap::ParseRemoteSnapshotManifest(manifest, snapshot, error));
    BOOST_CHECK_NE(error.find("not a UTXO snapshot"), std::string::npos);
}

BOOST_AUTO_TEST_SUITE_END()
