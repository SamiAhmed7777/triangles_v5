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
#include "../ui_interface.h"
#include "../wallet.h"
#include "../checkpoints.h"

#include <atomic>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <system_error>
#include <unistd.h>

namespace fs = std::filesystem;

// ─── Test-only friend accessor ─────────────────────────────────────────────
// CRocksTxDB keeps its raw methods (ReadRaw/WriteRaw/EraseRaw/ExistsRaw)
// protected because they're internal to the wrapper. This struct is declared
// as a friend of CRocksTxDB (see txdb-rocksdb.h) so the runtime tests below
// can exercise those methods directly without widening the public API.
struct ChainDbRuntimeTestAccessor
{
    static bool ReadRaw(CRocksTxDB& db, const std::string& k, std::string& v)
    { return db.ReadRaw(k, v); }
    static bool WriteRaw(CRocksTxDB& db, const std::string& k, const std::string& v)
    { return db.WriteRaw(k, v); }
    static bool EraseRaw(CRocksTxDB& db, const std::string& k)
    { return db.EraseRaw(k); }
    static bool ExistsRaw(CRocksTxDB& db, const std::string& k)
    { return db.ExistsRaw(k); }
};

// ─── Globals (minimal — chaindb wrappers don't pull in wallet/main) ───────
// Same rationale as test_snapshotnet: wallet.cpp (linked in for CWallet
// symbols) drags in main.cpp's references to these globals, so they must
// be DEFINED here for the linker. The values are never read by the
// chaindb runtime tests, so stubs are fine.
CClientUIInterface uiInterface;
CWallet* pwalletMain = nullptr;
bool fConfChange = false;
bool fEnforceCanonical = false;
unsigned int nNodeLifespan = 0;
unsigned int nDerivationMethodIndex = 0;
bool fUseFastIndex = false;
enum Checkpoints::CPMode CheckpointsMode = Checkpoints::STRICT;

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
// independent we explicitly close any prior handle before reopening. Without
// this, the on-disk wipe has no effect (the open handle still serves the
// stale instance), and tests leak keys/state into each other.
//
// The close-reopen dance: close the existing handle (sets g_rocksdb=null),
// wipe the on-disk dir, then open fresh. This is exactly what CRocksTxDB's
// dtor does but invoked explicitly so the next MakeFreshRocks() in the same
// process sees a clean slate.
std::unique_ptr<CRocksTxDB> MakeFreshRocks()
{
    fs::path dir = GetDataDir() / "rocksdb";
    std::error_code ec;

    // First close any existing global handle so the on-disk wipe below
    // actually takes effect. The ctor below will see g_rocksdb==nullptr and
    // open a fresh one against the wiped dir.
    {
        CRocksTxDB closer("r");
        closer.Close();
    }

    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
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
    // The default test build doesn't set the -chaindb flag at all. (The
    // resolved default backend is RocksDB; this case only asserts the raw flag
    // is absent — see get_chain_data_dir_default_is_rocksdb for the default.)
    BOOST_CHECK_EQUAL(GetBoolArg("-chaindb", false), false);
}

BOOST_AUTO_TEST_CASE(get_chain_data_dir_default_is_rocksdb)
{
    // No -chaindb flag set → RocksDB is the default backend, so
    // GetChainDataDir() must return the rocksdb path.
    mapArgs.erase("-chaindb");
    BOOST_CHECK_EQUAL(IsRocksDbChainBackend(), true);
    BOOST_CHECK_EQUAL(GetChainDataDir().filename().string(), "rocksdb");
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
    BOOST_REQUIRE(ChainDbRuntimeTestAccessor::WriteRaw(*db, key, val));

    std::string got;
    BOOST_REQUIRE(ChainDbRuntimeTestAccessor::ReadRaw(*db, key, got));
    BOOST_CHECK_EQUAL(got, val);

    // Exists must agree.
    BOOST_CHECK(ChainDbRuntimeTestAccessor::ExistsRaw(*db, key));
}

BOOST_AUTO_TEST_CASE(exists_returns_false_for_missing_key)
{
    auto db = MakeFreshRocks();
    BOOST_CHECK(!ChainDbRuntimeTestAccessor::ExistsRaw(*db, "never_written_key"));
}

BOOST_AUTO_TEST_CASE(erase_removes_key)
{
    auto db = MakeFreshRocks();
    std::string key = "to_erase";
    BOOST_REQUIRE(ChainDbRuntimeTestAccessor::WriteRaw(*db, key, "v"));
    BOOST_CHECK(ChainDbRuntimeTestAccessor::ExistsRaw(*db, key));

    BOOST_REQUIRE(ChainDbRuntimeTestAccessor::EraseRaw(*db, key));
    BOOST_CHECK(!ChainDbRuntimeTestAccessor::ExistsRaw(*db, key));

    std::string got;
    BOOST_CHECK(!ChainDbRuntimeTestAccessor::ReadRaw(*db, key, got));
}

BOOST_AUTO_TEST_CASE(erase_idempotent_on_missing_key)
{
    auto db = MakeFreshRocks();
    // EraseRaw on a missing key must not throw or return false in a way
    // that breaks callers — the migration code relies on this when wiping
    // the destination before copying.
    BOOST_CHECK(ChainDbRuntimeTestAccessor::EraseRaw(*db, "never_existed"));
}

BOOST_AUTO_TEST_CASE(transactional_batch_commit)
{
    auto db = MakeFreshRocks();

    BOOST_REQUIRE(db->TxnBegin());
    ChainDbRuntimeTestAccessor::WriteRaw(*db, "tx_key_a", "tx_val_a");
    ChainDbRuntimeTestAccessor::WriteRaw(*db, "tx_key_b", "tx_val_b");
    ChainDbRuntimeTestAccessor::WriteRaw(*db, "tx_key_c", "tx_val_c");
    BOOST_REQUIRE(db->TxnCommit());

    std::string got;
    BOOST_REQUIRE(ChainDbRuntimeTestAccessor::ReadRaw(*db, "tx_key_a", got));
    BOOST_CHECK_EQUAL(got, "tx_val_a");
    BOOST_REQUIRE(ChainDbRuntimeTestAccessor::ReadRaw(*db, "tx_key_b", got));
    BOOST_CHECK_EQUAL(got, "tx_val_b");
    BOOST_REQUIRE(ChainDbRuntimeTestAccessor::ReadRaw(*db, "tx_key_c", got));
    BOOST_CHECK_EQUAL(got, "tx_val_c");
}

BOOST_AUTO_TEST_CASE(transactional_batch_abort_discards_writes)
{
    auto db = MakeFreshRocks();

    BOOST_REQUIRE(db->TxnBegin());
    ChainDbRuntimeTestAccessor::WriteRaw(*db, "abort_key", "abort_val");
    BOOST_REQUIRE(db->TxnAbort());

    // The aborted writes must not be visible.
    std::string got;
    BOOST_CHECK(!ChainDbRuntimeTestAccessor::ReadRaw(*db, "abort_key", got));
    BOOST_CHECK(!ChainDbRuntimeTestAccessor::ExistsRaw(*db, "abort_key"));
}

BOOST_AUTO_TEST_CASE(within_batch_read_sees_pending_writes)
{
    auto db = MakeFreshRocks();

    BOOST_REQUIRE(db->TxnBegin());
    ChainDbRuntimeTestAccessor::WriteRaw(*db, "pending_key", "pending_val");

    // ReadRaw inside an open batch must see the pending write, not fall
    // through to the underlying DB (which doesn't have it yet).
    std::string got;
    BOOST_REQUIRE(ChainDbRuntimeTestAccessor::ReadRaw(*db, "pending_key", got));
    BOOST_CHECK_EQUAL(got, "pending_val");

    BOOST_REQUIRE(db->TxnCommit());

    // And after commit, still visible.
    BOOST_REQUIRE(ChainDbRuntimeTestAccessor::ReadRaw(*db, "pending_key", got));
    BOOST_CHECK_EQUAL(got, "pending_val");
}

BOOST_AUTO_TEST_CASE(within_batch_erase_visible_via_exists)
{
    auto db = MakeFreshRocks();

    // Seed outside the batch.
    BOOST_REQUIRE(ChainDbRuntimeTestAccessor::WriteRaw(*db, "erase_in_batch", "value"));

    BOOST_REQUIRE(db->TxnBegin());
    BOOST_REQUIRE(ChainDbRuntimeTestAccessor::EraseRaw(*db, "erase_in_batch"));

    // Inside the batch, ExistsRaw must return false (ScanBatch returns
    // deleted=true).
    BOOST_CHECK(!ChainDbRuntimeTestAccessor::ExistsRaw(*db, "erase_in_batch"));

    BOOST_REQUIRE(db->TxnCommit());

    // After commit, the key is gone for real.
    BOOST_CHECK(!ChainDbRuntimeTestAccessor::ExistsRaw(*db, "erase_in_batch"));
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
        BOOST_REQUIRE(ChainDbRuntimeTestAccessor::WriteRaw(*db, kv.first, kv.second));
    }

    auto it = db->NewIterator();
    BOOST_REQUIRE(it != nullptr);
    std::vector<std::string> seenKeys;
    for (it->Seek(std::string()); it->Valid(); it->Next()) {
        // CTxDBBase::Write(string, value) length-prefixes the key string
        // (VarInt), so the actual stored key is e.g. "\x07version" rather
        // than "version". Compare against the length-prefixed form rather
        // than the bare string. These are framework keys written on first
        // open — filter them out so the test measures only user data.
        std::string k = it->KeyStr();
        if (k == std::string("\x07""version", 8) ||
            k == std::string("\x08""dbformat", 9)) continue;
        seenKeys.push_back(k);
    }
    BOOST_REQUIRE_EQUAL(seenKeys.size(), entries.size());
    // Sorted order.
    BOOST_CHECK_EQUAL(seenKeys[0], "alpha");
    BOOST_CHECK_EQUAL(seenKeys[1], "banana");
    BOOST_CHECK_EQUAL(seenKeys[2], "mango");
    BOOST_CHECK_EQUAL(seenKeys[3], "zebra");

    // And each value matches the source.
    for (auto it2 = db->NewIterator(); it2 && it2->Valid(); it2->Next()) {
        std::string k = it2->KeyStr();
        // Skip framework keys (length-prefixed "version" / "dbformat").
        if (k == std::string("\x07""version", 8) ||
            k == std::string("\x08""dbformat", 9)) continue;
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
        {"blockindex", uint256("0x0000000000000000000000000000000000000000000000000000000000000001")},
        {"blockindex", uint256("0x00000000000000000000000000000000000000000000000000000000000000ff")},
        {"blockindex", uint256("0x0000000000000000000000000000000000000000000000000000000000000abc")},
    };

    for (const auto& blk : blocks) {
        CDataStream ssKey(SER_DISK, 1);
        ssKey << blk;
        // The wrapper exposes WriteRaw that takes a string; build the key bytes.
        std::string keyBytes(ssKey.begin(), ssKey.end());
        std::string valBytes(64, 'x');
        BOOST_REQUIRE(ChainDbRuntimeTestAccessor::WriteRaw(*db, keyBytes, valBytes));
    }

    // Re-iterate and count. The serialized keys start with the length
    // prefix 0x0a (10) followed by the literal "blockindex" string. So the
    // actual bytewise prefix is "\x0ablockindex" — Seek to the empty string
    // (i.e. first key) and walk from there.
    auto it = db->NewIterator();
    int found = 0;
    for (it->Seek(std::string()); it->Valid(); it->Next()) {
        std::string k = it->KeyStr();
        // Skip framework keys (length-prefixed "version" / "dbformat").
        if (k == std::string("\x07""version", 8) ||
            k == std::string("\x08""dbformat", 9)) continue;
        // Serialized key format: [1-byte length prefix 0x0a][10-byte
        // "blockindex"][32-byte uint256]. Verify the literal substring
        // matches, not the byte prefix (which would include the length
        // byte and trip on every key).
        BOOST_CHECK(k.find("blockindex") != std::string::npos);
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
        BOOST_REQUIRE(ChainDbRuntimeTestAccessor::WriteRaw(*db, "persisted", "across_close"));
        db->Close();
    }
    // Re-open by constructing a new instance against the same dir.
    {
        auto db = std::make_unique<CRocksTxDB>("r+");
        std::string got;
        BOOST_REQUIRE(ChainDbRuntimeTestAccessor::ReadRaw(*db, "persisted", got));
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
        auto base = MakeChainDB("cr+");
        BOOST_REQUIRE(base != nullptr);
        // MakeChainDB returns CTxDBBase&; we know we set -chaindb=rocksdb so
        // the concrete type is CRocksTxDB. Cast to access the wrapper methods
        // via the friend accessor. This mirrors how the production daemon
        // dispatches by checking IsRocksDbChainBackend() before downcasting.
        auto& rocks = static_cast<CRocksTxDB&>(*base);
        BOOST_REQUIRE(ChainDbRuntimeTestAccessor::WriteRaw(rocks, "wipe_test", "v"));
    }
    fs::path dir = GetDataDir() / "rocksdb";
    BOOST_REQUIRE(fs::exists(dir));

    WipeChainDataDir();
    BOOST_CHECK(!fs::exists(dir));
    mapArgs.erase("-chaindb");
}

BOOST_AUTO_TEST_CASE(wipe_removes_txleveldb_dir_when_leveldb_selected)
{
    // With -chaindb=leveldb, MakeChainDB("cr+") opens the LevelDB handle which
    // creates the txleveldb/ directory on disk. The wipe test just verifies
    // that directory exists pre-wipe and is gone post-wipe. (RocksDB is the
    // default now, so LevelDB must be requested explicitly.)
    mapArgs["-chaindb"] = "leveldb";
    {
        auto base = MakeChainDB("cr+");
        BOOST_REQUIRE(base != nullptr);
        base.reset(); // close handle before checking dir
    }
    fs::path dir = GetDataDir() / "txleveldb";
    BOOST_REQUIRE(fs::exists(dir));

    WipeChainDataDir();
    BOOST_CHECK(!fs::exists(dir));
    mapArgs.erase("-chaindb");
}

BOOST_AUTO_TEST_SUITE_END()
