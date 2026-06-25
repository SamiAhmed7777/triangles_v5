// Copyright (c) 2026 Triangles developers
// Distributed under the MIT/X11 software license
//
// Live runtime smoke tests for the RocksDB chain-DB backend.
//
// Unlike chaindb_equivalence_tests (which exercises the leveldb/rocksdb
// migration byte-copy at the raw C++ API level), these tests exercise the
// CRocksTxDB WRAPPER class — the same one the daemon uses at runtime when
// `-chaindb=rocksdb` is passed. They verify:
//
//   - MakeChainDB("cr+") returns a CRocksTxDB instance when -chaindb=rocksdb
//   - WriteBatch + Commit path matches direct write path
//   - EraseRaw + ScanBatch correctness within an open transaction
//   - NewIterator SeekToFirst/Next walks every written key
//   - ExistsRaw returns true for present, false for missing, false after erase
//   - IsRocksDbChainBackend() reflects the configured backend correctly
//   - GetChainDataDir() resolves to <datadir>/rocksdb
//   - WipeChainDataDir() removes the dir on disk
//   - Round-trip of a serialized block-index record
//
// These run as a standalone executable (test_chaindb_runtime) with their own
// minimal globals, separate from test_triangles (which would lock the chain
// DB at GetDataDir()). Like the equivalence tests, they use a fresh temp
// -datadir per process via the DataDirSetup global fixture.

#define BOOST_TEST_MODULE chaindb_runtime_tests_standalone
#include <boost/test/unit_test.hpp>

#include "../txdb.h"
#include "../txdb-base.h"
#include "../txdb-rocksdb.h"
#include "../txdb-leveldb.h"
#include "../util.h"
#include "../serialize.h"
#include "../uint256.h"

#include <filesystem>
#include <memory>
#include <system_error>
#include <unistd.h>

namespace fs = std::filesystem;

// ─── Globals (minimal — chaindb wrappers don't pull in wallet/main) ───────
CClientUIInterface uiInterface;

void StartShutdown() { /* no-op */ }

namespace {

struct DataDirSetup
{
    fs::path tmp;
    DataDirSetup()
    {
        tmp = fs::temp_directory_path() /
            ("triangles_chaindb_rt_" + std::to_string(getpid()));
        std::error_code ec;
        fs::remove_all(tmp, ec);
        fs::create_directories(tmp);
        mapArgs["-datadir"] = tmp.string();
        // Constrain cache so the test host's memory budget doesn't get hit.
        mapArgs["-dbcache"] = "64";
    }
    ~DataDirSetup() {
        std::error_code ec;
        fs::remove_all(tmp, ec);
    }
};

// Wipe + recreate the rocksdb/ subdir so each test starts fresh. The
// CRocksTxDB constructor keeps a static g_rocksdb handle — to keep tests
// independent we open/close per test.
std::unique_ptr<CRocksTxDB> MakeFreshRocks()
{
    fs::path dir = GetDataDir() / "rocksdb";
    std::error_code ec;
    fs::remove_all(dir, ec);
    return std::make_unique<CRocksTxDB>("cr+");
}

} // namespace

BOOST_GLOBAL_FIXTURE(DataDirSetup);

// ───────────────────────────────────────────────────────────────────────────
// Backend selection
// ───────────────────────────────────────────────────────────────────────────

BOOST_AUTO_TEST_SUITE(chaindb_backend_selection)

BOOST_AUTO_TEST_CASE(is_rocksdb_backend_flag_default_off)
{
    // Default test build doesn't set -chaindb, so backend should NOT be rocksdb.
    BOOST_CHECK_EQUAL(GetBoolArg("-chaindb", false), false);
}

BOOST_AUTO_TEST_CASE(get_chain_data_dir_default_is_txleveldb)
{
    // No -chaindb flag set → GetChainDataDir() must return txleveldb path.
    mapArgs.erase("-chaindb");
    BOOST_CHECK_EQUAL(IsRocksDbChainBackend(), false);
    BOOST_CHECK_EQUAL(GetChainDataDir().filename().string(), "txleveldb");
}

BOOST_AUTO_TEST_CASE(get_chain_data_dir_rocksdb_when_flag_set)
{
    mapArgs["-chaindb"] = "rocksdb";
    BOOST_CHECK_EQUAL(IsRocksDbChainBackend(), true);
    BOOST_CHECK_EQUAL(GetChainDataDir().filename().string(), "rocksdb");
    mapArgs.erase("-chaindb");
}

BOOST_AUTO_TEST_CASE(get_chain_data_dir_leveldb_explicit)
{
    mapArgs["-chaindb"] = "leveldb";
    BOOST_CHECK_EQUAL(IsRocksDbChainBackend(), false);
    BOOST_CHECK_EQUAL(GetChainDataDir().filename().string(), "txleveldb");
    mapArgs.erase("-chaindb");
}

BOOST_AUTO_TEST_SUITE_END()

// ───────────────────────────────────────────────────────────────────────────
// CRocksTxDB wrapper behavior
// ───────────────────────────────────────────────────────────────────────────

BOOST_AUTO_TEST_SUITE(rocksdb_wrapper)

BOOST_AUTO_TEST_CASE(make_chain_db_returns_rocks_instance_when_flagged)
{
    mapArgs["-chaindb"] = "rocksdb";
    auto db = MakeChainDB("cr+");
    BOOST_REQUIRE(db != nullptr);
    // CRocksTxDB inherits from CTxDBBase; check via dynamic_cast.
    BOOST_CHECK(dynamic_cast<CRocksTxDB*>(db.get()) != nullptr);
    mapArgs.erase("-chaindb");
}

BOOST_AUTO_TEST_CASE(write_then_read_raw_key)
{
    auto db = MakeFreshRocks();
    BOOST_REQUIRE(db != nullptr);

    std::string key = "testkey_basic";
    std::string val = "testvalue_basic";
    BOOST_REQUIRE(db->WriteRaw(key, val));

    std::string got;
    BOOST_REQUIRE(db->ReadRaw(key, got));
    BOOST_CHECK_EQUAL(got, val);

    // Exists must agree.
    BOOST_CHECK(db->ExistsRaw(key));
}

BOOST_AUTO_TEST_CASE(exists_returns_false_for_missing_key)
{
    auto db = MakeFreshRocks();
    BOOST_CHECK(!db->ExistsRaw("never_written_key"));
}

BOOST_AUTO_TEST_CASE(erase_removes_key)
{
    auto db = MakeFreshRocks();
    std::string key = "to_erase";
    BOOST_REQUIRE(db->WriteRaw(key, "v"));
    BOOST_CHECK(db->ExistsRaw(key));

    BOOST_REQUIRE(db->EraseRaw(key));
    BOOST_CHECK(!db->ExistsRaw(key));

    std::string got;
    BOOST_CHECK(!db->ReadRaw(key, got));
}

BOOST_AUTO_TEST_CASE(erase_idempotent_on_missing_key)
{
    auto db = MakeFreshRocks();
    // EraseRaw on a missing key must not throw or return false in a way
    // that breaks callers — the migration code relies on this when wiping
    // the destination before copying.
    BOOST_CHECK(db->EraseRaw("never_existed"));
}

BOOST_AUTO_TEST_CASE(transactional_batch_commit)
{
    auto db = MakeFreshRocks();

    BOOST_REQUIRE(db->TxnBegin());
    db->WriteRaw("tx_key_a", "tx_val_a");
    db->WriteRaw("tx_key_b", "tx_val_b");
    db->WriteRaw("tx_key_c", "tx_val_c");
    BOOST_REQUIRE(db->TxnCommit());

    std::string got;
    BOOST_REQUIRE(db->ReadRaw("tx_key_a", got));
    BOOST_CHECK_EQUAL(got, "tx_val_a");
    BOOST_REQUIRE(db->ReadRaw("tx_key_b", got));
    BOOST_CHECK_EQUAL(got, "tx_val_b");
    BOOST_REQUIRE(db->ReadRaw("tx_key_c", got));
    BOOST_CHECK_EQUAL(got, "tx_val_c");
}

BOOST_AUTO_TEST_CASE(transactional_batch_abort_discards_writes)
{
    auto db = MakeFreshRocks();

    BOOST_REQUIRE(db->TxnBegin());
    db->WriteRaw("abort_key", "abort_val");
    BOOST_REQUIRE(db->TxnAbort());

    // The aborted writes must not be visible.
    std::string got;
    BOOST_CHECK(!db->ReadRaw("abort_key", got));
    BOOST_CHECK(!db->ExistsRaw("abort_key"));
}

BOOST_AUTO_TEST_CASE(within_batch_read_sees_pending_writes)
{
    auto db = MakeFreshRocks();

    BOOST_REQUIRE(db->TxnBegin());
    db->WriteRaw("pending_key", "pending_val");

    // ReadRaw inside an open batch must see the pending write, not fall
    // through to the underlying DB (which doesn't have it yet).
    std::string got;
    BOOST_REQUIRE(db->ReadRaw("pending_key", got));
    BOOST_CHECK_EQUAL(got, "pending_val");

    BOOST_REQUIRE(db->TxnCommit());

    // And after commit, still visible.
    BOOST_REQUIRE(db->ReadRaw("pending_key", got));
    BOOST_CHECK_EQUAL(got, "pending_val");
}

BOOST_AUTO_TEST_CASE(within_batch_erase_visible_via_exists)
{
    auto db = MakeFreshRocks();

    // Seed outside the batch.
    BOOST_REQUIRE(db->WriteRaw("erase_in_batch", "value"));

    BOOST_REQUIRE(db->TxnBegin());
    BOOST_REQUIRE(db->EraseRaw("erase_in_batch"));

    // Inside the batch, ExistsRaw must return false (ScanBatch returns
    // deleted=true).
    BOOST_CHECK(!db->ExistsRaw("erase_in_batch"));

    BOOST_REQUIRE(db->TxnCommit());

    // After commit, the key is gone for real.
    BOOST_CHECK(!db->ExistsRaw("erase_in_batch"));
}

BOOST_AUTO_TEST_CASE(iterator_walks_every_key_in_sorted_order)
{
    auto db = MakeFreshRocks();

    // Insert in scrambled order; the iterator must produce them sorted.
    const std::vector<std::pair<std::string, std::string>> entries = {
        {"zebra",  "z_val"},
        {"alpha",  "a_val"},
        {"mango",  "m_val"},
        {"banana", "b_val"},
    };
    for (const auto& kv : entries) {
        BOOST_REQUIRE(db->WriteRaw(kv.first, kv.second));
    }

    auto it = db->NewIterator();
    BOOST_REQUIRE(it != nullptr);
    std::vector<std::string> seenKeys;
    for (it->Seek(std::string()); it->Valid(); it->Next()) {
        seenKeys.push_back(it->KeyStr());
    }
    BOOST_REQUIRE_EQUAL(seenKeys.size(), entries.size());
    // Sorted order.
    BOOST_CHECK_EQUAL(seenKeys[0], "alpha");
    BOOST_CHECK_EQUAL(seenKeys[1], "banana");
    BOOST_CHECK_EQUAL(seenKeys[2], "mango");
    BOOST_CHECK_EQUAL(seenKeys[3], "zebra");

    // And each value matches the source.
    for (auto& it2 = db->NewIterator(); it2->Seek(std::string()); it2->Next()) {
        std::string k = it2->KeyStr();
        std::string v = it2->ValueStr();
        bool matched = false;
        for (const auto& kv : entries) {
            if (kv.first == k) {
                BOOST_CHECK_EQUAL(v, kv.second);
                matched = true;
                break;
            }
        }
        BOOST_CHECK(matched);
    }
}

BOOST_AUTO_TEST_CASE(serialized_block_index_record_roundtrip)
{
    // The real-world key shape for block index is a (string, uint256) pair
    // serialized via CDataStream. Verify the wrapper handles that pattern.
    auto db = MakeFreshRocks();

    std::vector<std::pair<std::string, uint256>> blocks = {
        {"blockindex", uint256S("0000000000000000000000000000000000000000000000000000000000000001")},
        {"blockindex", uint256S("00000000000000000000000000000000000000000000000000000000000000ff")},
        {"blockindex", uint256S("0000000000000000000000000000000000000000000000000000000000000abc")},
    };

    for (const auto& blk : blocks) {
        CDataStream ssKey(SER_DISK, 1);
        ssKey << blk;
        // The wrapper exposes WriteRaw that takes a string; build the key bytes.
        std::string keyBytes(ssKey.begin(), ssKey.end());
        std::string valBytes(64, 'x');
        BOOST_REQUIRE(db->WriteRaw(keyBytes, valBytes));
    }

    // Re-iterate and count.
    auto it = db->NewIterator();
    int found = 0;
    for (it->Seek(std::string("blockindex")); it->Valid(); it->Next()) {
        // CRocksTxDB iterator returns raw bytes; verify the key starts with
        // "blockindex" as a sanity check on the prefix pattern.
        std::string k = it->KeyStr();
        BOOST_CHECK(k.substr(0, 10) == "blockindex");
        ++found;
    }
    BOOST_CHECK_EQUAL(found, 3);
}

BOOST_AUTO_TEST_CASE(close_then_reopen_preserves_data)
{
    // The CRocksTxDB class uses a static g_rocksdb handle. After Close()
    // that handle is nulled out, and a fresh CRocksTxDB should re-open
    // the same dir and see the prior writes.
    {
        auto db = MakeFreshRocks();
        BOOST_REQUIRE(db->WriteRaw("persisted", "across_close"));
        db->Close();
    }
    // Re-open by constructing a new instance against the same dir.
    {
        auto db = std::make_unique<CRocksTxDB>("r+");
        std::string got;
        BOOST_REQUIRE(db->ReadRaw("persisted", got));
        BOOST_CHECK_EQUAL(got, "across_close");
    }
}

BOOST_AUTO_TEST_SUITE_END()

// ───────────────────────────────────────────────────────────────────────────
// WipeChainDataDir
// ───────────────────────────────────────────────────────────────────────────

BOOST_AUTO_TEST_SUITE(chaindb_wipe)

BOOST_AUTO_TEST_CASE(wipe_removes_rocksdb_dir_when_flagged)
{
    mapArgs["-chaindb"] = "rocksdb";
    {
        auto db = MakeChainDB("cr+");
        BOOST_REQUIRE(db != nullptr);
        BOOST_REQUIRE(db->WriteRaw("wipe_test", "v"));
    }
    fs::path dir = GetDataDir() / "rocksdb";
    BOOST_REQUIRE(fs::exists(dir));

    WipeChainDataDir();
    BOOST_CHECK(!fs::exists(dir));
    mapArgs.erase("-chaindb");
}

BOOST_AUTO_TEST_CASE(wipe_removes_txleveldb_dir_by_default)
{
    mapArgs.erase("-chaindb");
    {
        auto db = MakeChainDB("cr+");
        BOOST_REQUIRE(db != nullptr);
        BOOST_REQUIRE(db->WriteRaw("wipe_test_leveldb", "v"));
    }
    fs::path dir = GetDataDir() / "txleveldb";
    BOOST_REQUIRE(fs::exists(dir));

    WipeChainDataDir();
    BOOST_CHECK(!fs::exists(dir));
}

BOOST_AUTO_TEST_SUITE_END()
