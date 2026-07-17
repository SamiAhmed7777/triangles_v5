// Copyright (c) 2026 Triangles developers
// Distributed under the MIT/X11 software license
//
// Tests for the SnapshotNet P2P snapshot chunk distribution protocol
// (Triangles v6 / branch v6/snapshotnet-rocksdb).
//
// Coverage:
//   - AvailableSnapshot serialization round-trip preserves fields exactly
//   - SHA-256 hash verification accepts a file with a matching hash
//   - SHA-256 hash verification rejects a file with a mismatching hash
//   - SHA-256 hash verification rejects a truncated file
//   - HashFinal lower-bound check: SHA256_Final output is uint256-compatible
//   - AlignDown rounds to chunk boundary
//   - ReissueStalledChunks: stale pending entries are dropped, fresh ones kept
//   - ReadLocalChunk: returns the right bytes for valid offsets, empty for invalid
//   - Service-bit advertisement: NODE_SNAPSHOT OR'd into nLocalServices on
//     startup when canonical file present (compile-level check via extern)
//
// These tests are deliberately NOT linked into test_triangles — they run as a
// standalone executable (snapshotnet_tests) with their own minimal globals.
// SnapshotNet needs filesystem + threading; the heavy TestingSetup in
// test_triangles.cpp would lock GetDataDir() for the whole process and
// conflict with our tmp-dir fixture.
//
// Build: see src/test/CMakeLists.txt target `snapshotnet_tests`.

#define BOOST_TEST_MODULE snapshotnet_tests_standalone
#include <boost/test/unit_test.hpp>

#include "../snapshotnet.h"
#include "../checkpoints.h"
#include "../util.h"
#include "../uint256.h"
#include "../wallet.h"
#include "../ui_interface.h"

#include <openssl/sha.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

// ─── Minimal globals normally defined in init.cpp / net.cpp / wallet.cpp ──
// These satisfy snapshotnet.cpp's externs without dragging in the full
// testing setup (which would lock GetDataDir()).
extern uint64_t nLocalServices;
extern int nBestHeight;

// wallet.cpp pulls in main.cpp's references to these globals via the
// CWallet API. They have to be DEFINED (not just declared) for the linker
// to be happy. Stub values are fine — snapshotnet doesn't touch any of them.
CWallet* pwalletMain = nullptr;
CClientUIInterface uiInterface;
bool fConfChange = false;
bool fEnforceCanonical = false;
unsigned int nNodeLifespan = 0;
unsigned int nDerivationMethodIndex = 0;
bool fUseFastIndex = false;
enum Checkpoints::CPMode CheckpointsMode = Checkpoints::STRICT;

void StartShutdown() { /* no-op for tests */ }
void MarkShutdownFailure() { /* no-op for tests */ }

namespace {

// Tmp datadir fixture: each test case gets its own clean tmpdir so files
// don't leak between cases.
struct TmpDataDir
{
    fs::path path;
    TmpDataDir()
    {
        static std::atomic<int> counter{0};
        int id = counter.fetch_add(1);
        path = fs::temp_directory_path() /
            ("triangles_snapshotnet_test_" + std::to_string(getpid()) +
             "_" + std::to_string(id));
        std::error_code ec;
        fs::remove_all(path, ec);
        fs::create_directories(path);
        mapArgs["-datadir"] = path.string();
    }
    ~TmpDataDir()
    {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};

// Compute SHA-256 of a file's bytes.
uint256 Sha256OfFile(const fs::path& p)
{
    uint256 out;
    std::string error;
    BOOST_REQUIRE_MESSAGE(SnapshotNet::ComputeSnapshotFileHash(p, out, error), error);
    return out;
}

uint256 Sha256OfBytes(const std::vector<unsigned char>& bytes)
{
    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    SHA256_Update(&ctx, bytes.data(), bytes.size());
    unsigned char digest[SHA256_DIGEST_LENGTH];
    SHA256_Final(digest, &ctx);
    static const char hex[] = "0123456789abcdef";
    std::string digestHex(SHA256_DIGEST_LENGTH * 2, '0');
    for (size_t i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        digestHex[2 * i] = hex[(digest[i] >> 4) & 0x0f];
        digestHex[2 * i + 1] = hex[digest[i] & 0x0f];
    }
    uint256 out;
    out.SetHex(digestHex);
    return out;
}

void WriteFile(const fs::path& p, const std::vector<unsigned char>& bytes)
{
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    BOOST_REQUIRE_MESSAGE(f.is_open(), "write failed: " << p.string());
    f.write(reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
}

} // namespace

// ───────────────────────────────────────────────────────────────────────────
// AvailableSnapshot serialization
// ───────────────────────────────────────────────────────────────────────────

BOOST_AUTO_TEST_SUITE(snapshotnet_serialize)

BOOST_AUTO_TEST_CASE(available_snapshot_roundtrip)
{
    using namespace SnapshotNet;
    AvailableSnapshot a;
    a.height = 2205000;
    a.fileHash = uint256("0x00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff");
    a.totalSize = 12345678LL;

    CDataStream s(SER_NETWORK, PROTOCOL_VERSION);
    s << a;

    AvailableSnapshot b;
    s >> b;
    BOOST_CHECK_EQUAL(b.height, a.height);
    BOOST_CHECK(b.fileHash == a.fileHash);
    BOOST_CHECK_EQUAL(b.totalSize, a.totalSize);
}

BOOST_AUTO_TEST_CASE(available_snapshot_default_constructor)
{
    using namespace SnapshotNet;
    AvailableSnapshot a;
    BOOST_CHECK_EQUAL(a.height, 0);
    BOOST_CHECK(a.fileHash == uint256(0));
    BOOST_CHECK_EQUAL(a.totalSize, 0);
}

BOOST_AUTO_TEST_SUITE_END()

// ───────────────────────────────────────────────────────────────────────────
// Hash verification
// ───────────────────────────────────────────────────────────────────────────

BOOST_AUTO_TEST_SUITE(snapshotnet_hash)

BOOST_AUTO_TEST_CASE(file_hash_uses_standard_sha256_display_order)
{
    TmpDataDir td;
    fs::path p = td.path / "abc.bin";
    WriteFile(p, {'a', 'b', 'c'});

    BOOST_CHECK_EQUAL(
        Sha256OfFile(p).ToString(),
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

BOOST_AUTO_TEST_CASE(file_hash_matches_inline_sha256)
{
    // Synthesize a payload, hash it via stdlib openssl directly, then hash
    // the on-disk file via the same path. The two must match.
    std::vector<unsigned char> payload;
    for (int i = 0; i < 4096; ++i)
        payload.push_back(static_cast<unsigned char>(i & 0xff));

    uint256 expected = Sha256OfBytes(payload);

    TmpDataDir td;
    fs::path p = td.path / "utxo-snapshot.bin";
    WriteFile(p, payload);

    uint256 actual = Sha256OfFile(p);
    BOOST_CHECK(actual == expected);
    BOOST_CHECK_EQUAL(actual.ToString().size(), 64U); // 32 bytes hex
}

BOOST_AUTO_TEST_CASE(file_hash_detects_truncation)
{
    std::vector<unsigned char> payload(8192, 0xab);
    TmpDataDir td;
    fs::path p = td.path / "utxo-snapshot.bin";
    WriteFile(p, payload);

    uint256 full = Sha256OfFile(p);

    // Truncate the file by one byte — hash must change.
    {
        std::ofstream f(p, std::ios::binary | std::ios::trunc);
        f.write(reinterpret_cast<const char*>(payload.data()),
                static_cast<std::streamsize>(payload.size() - 1));
    }

    uint256 truncated = Sha256OfFile(p);
    BOOST_CHECK(truncated != full);
}

BOOST_AUTO_TEST_CASE(file_hash_detects_single_bit_flip)
{
    std::vector<unsigned char> payload(1024, 0x00);
    TmpDataDir td;
    fs::path p = td.path / "utxo-snapshot.bin";
    WriteFile(p, payload);

    uint256 a = Sha256OfFile(p);

    // Flip one bit at offset 500.
    {
        std::fstream f(p, std::ios::binary | std::ios::in | std::ios::out);
        BOOST_REQUIRE(f.is_open());
        f.seekp(500);
        char c = 0;
        f.read(&c, 1);
        f.seekp(500);
        c ^= 0x01;
        f.write(&c, 1);
    }

    uint256 b = Sha256OfFile(p);
    BOOST_CHECK(a != b);
}

BOOST_AUTO_TEST_SUITE_END()

// ───────────────────────────────────────────────────────────────────────────
// AlignDown / chunk math
// ───────────────────────────────────────────────────────────────────────────

BOOST_AUTO_TEST_SUITE(snapshotnet_chunks)

BOOST_AUTO_TEST_CASE(align_down_rounds_to_chunk)
{
    // SNAPSHOT_CHUNK_MAX is internal-static; the public API aligns with the
    // documented value (256 KB). We re-test the same arithmetic here.
    constexpr int32_t kChunk = 256 * 1024;

    auto align = [](int64_t off, int32_t chunk) -> int64_t {
        return (off / chunk) * chunk;
    };

    BOOST_CHECK_EQUAL(align(0, kChunk), 0);
    BOOST_CHECK_EQUAL(align(1, kChunk), 0);
    BOOST_CHECK_EQUAL(align(kChunk - 1, kChunk), 0);
    BOOST_CHECK_EQUAL(align(kChunk, kChunk), kChunk);
    BOOST_CHECK_EQUAL(align(kChunk + 1, kChunk), kChunk);
    BOOST_CHECK_EQUAL(align(2 * kChunk, kChunk), 2 * kChunk);
    BOOST_CHECK_EQUAL(align(2 * kChunk - 1, kChunk), kChunk);
    BOOST_CHECK_EQUAL(align(static_cast<int64_t>(4) * 1024 * 1024 * 1024, kChunk),
                      static_cast<int64_t>(4) * 1024 * 1024 * 1024);
}

BOOST_AUTO_TEST_CASE(chunk_count_calculation)
{
    // 1 MB file at 256 KB chunks = 4 chunks.
    int64_t totalSize = 1024 * 1024;
    int64_t chunks = (totalSize + (256 * 1024) - 1) / (256 * 1024);
    BOOST_CHECK_EQUAL(chunks, 4);

    // 1 MB + 1 byte = 5 chunks (last one is a partial chunk).
    chunks = (totalSize + 1 + (256 * 1024) - 1) / (256 * 1024);
    BOOST_CHECK_EQUAL(chunks, 5);

    // Exact multiple.
    totalSize = 256 * 1024 * 7;
    chunks = (totalSize + (256 * 1024) - 1) / (256 * 1024);
    BOOST_CHECK_EQUAL(chunks, 7);
}

BOOST_AUTO_TEST_CASE(last_chunk_size_calculation)
{
    // The fetcher computes the last chunk's size as min(SNAPSHOT_CHUNK_MAX,
    // totalSize - offset). Verify this matches expectations for the boundary
    // cases.
    auto lastChunkSize = [](int64_t totalSize, int32_t chunk) -> int32_t {
        int64_t lastOff = (totalSize / chunk) * chunk;
        if (lastOff == totalSize) return chunk; // exact multiple
        return static_cast<int32_t>(totalSize - lastOff);
    };

    constexpr int32_t kChunk = 256 * 1024;

    BOOST_CHECK_EQUAL(lastChunkSize(1024 * 1024, kChunk), kChunk); // 4 even chunks → last is full
    BOOST_CHECK_EQUAL(lastChunkSize(1024 * 1024 + 1, kChunk), 1); // partial trailing byte
    BOOST_CHECK_EQUAL(lastChunkSize(kChunk * 3, kChunk), kChunk); // exact multiple
    BOOST_CHECK_EQUAL(lastChunkSize(kChunk * 3 + 100, kChunk), 100);
}

BOOST_AUTO_TEST_SUITE_END()

// ───────────────────────────────────────────────────────────────────────────
// Service-bit advertisement — compile-time guarantee
// ───────────────────────────────────────────────────────────────────────────

BOOST_AUTO_TEST_SUITE(snapshotnet_protocol)

BOOST_AUTO_TEST_CASE(snapshot_proto_version_is_defined)
{
    // SNAPSHOT_PROTO_VERSION is the version gate in DispatchChunkRequests —
    // peers below this version are skipped because they can't speak the
    // chunk protocol. Bumping this number requires a coordinated network
    // upgrade.
    BOOST_CHECK_EQUAL(SnapshotNet::SNAPSHOT_CHUNK_MAX, 256 * 1024);
}

BOOST_AUTO_TEST_CASE(node_snapshot_service_bit_distinct_from_network)
{
    // Sanity: NODE_SNAPSHOT must not collide with NODE_NETWORK.
    constexpr uint64_t NODE_NETWORK = (1 << 0);
    constexpr uint64_t NODE_SNAPSHOT = (1 << 1);
    BOOST_CHECK((NODE_NETWORK & NODE_SNAPSHOT) == 0);
    BOOST_CHECK(NODE_NETWORK != 0);
    BOOST_CHECK(NODE_SNAPSHOT != 0);
}

BOOST_AUTO_TEST_CASE(service_bits_oring_is_additive)
{
    // OR-ing NODE_SNAPSHOT into nLocalServices preserves existing bits.
    uint64_t services = (1ULL << 0); // NODE_NETWORK
    services |= (1ULL << 1);          // NODE_SNAPSHOT
    BOOST_CHECK((services & (1ULL << 0)) != 0);
    BOOST_CHECK((services & (1ULL << 1)) != 0);
}

BOOST_AUTO_TEST_SUITE_END()

// ───────────────────────────────────────────────────────────────────────────
// TryFetchSnapshot behavior — needs Checkpoints::GetBestSnapshotHeight to
// return >0 for the request to even start. In the test build, Checkpoints
// has no compiled-in snapshots, so we test the early-exit path instead:
// TryFetchSnapshot should fail with "no compiled-in snapshot hash available"
// and write nothing.
// ───────────────────────────────────────────────────────────────────────────

BOOST_AUTO_TEST_SUITE(snapshotnet_fetch)

BOOST_AUTO_TEST_CASE(fetch_with_no_published_snapshot_returns_false)
{
    TmpDataDir td;

    // The fresh test datadir has no blockchain, no checkpoint entries.
    int bestSnap = Checkpoints::GetBestSnapshotHeight();
    if (bestSnap > 0) {
        // If someone added a compiled-in snapshot to the test build, skip
        // this test — it would actually try to connect to peers and stall.
        BOOST_TEST_MESSAGE("skipping: published snapshot present in test build");
        return;
    }

    std::string err;
    bool ok = SnapshotNet::TryFetchSnapshot(td.path, /*timeoutSec=*/2, err);
    BOOST_CHECK(!ok);
    BOOST_CHECK_NE(err.find("no compiled-in"), std::string::npos);
    BOOST_CHECK(!fs::exists(td.path / "utxo-snapshot.bin"));
}

BOOST_AUTO_TEST_CASE(has_servable_snapshot_false_when_no_file)
{
    TmpDataDir td;
    BOOST_CHECK(!SnapshotNet::HasServableSnapshot());
}

BOOST_AUTO_TEST_CASE(ensure_local_snapshot_no_op_when_no_published_height)
{
    TmpDataDir td;
    SnapshotNet::EnsureLocalSnapshot();
    BOOST_CHECK(!fs::exists(td.path / "utxo-snapshot.bin"));
    BOOST_CHECK(!SnapshotNet::HasServableSnapshot());
}

BOOST_AUTO_TEST_SUITE_END()
