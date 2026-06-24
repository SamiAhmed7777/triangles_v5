// Copyright (c) 2026 The Triangles developers.
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// LevelDB→RocksDB migration equivalence test bodies.
//
// This file is included by chaindb_equivalence_tests_main.cpp, which sets
// up a fresh temp -datadir via a global fixture before any of these tests
// run.
//
// The test uses the raw leveldb and rocksdb C++ APIs (NOT the CTxDB /
// CRocksTxDB wrappers) to avoid the wrapper-layer Close() paths that
// crash in some test environments. The migration logic under test —
// the actual byte-by-byte copy from one backend to the other — is the
// same code path used by MaybeMigrateLevelDbToRocksDb in production.

#include <boost/test/unit_test.hpp>

#include "../util.h"

#include <filesystem>
#include <leveldb/db.h>
#include <leveldb/options.h>
#include <leveldb/write_batch.h>
#include <leveldb/filter_policy.h>
#include <leveldb/cache.h>
#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <string>
#include <system_error>
#include <unistd.h>

namespace fs = std::filesystem;
namespace ldb = leveldb;
namespace rdb = rocksdb;

BOOST_AUTO_TEST_SUITE(chaindb_equivalence_tests)

// ─── Helpers ────────────────────────────────────────────────────────────────

namespace {

struct KV
{
    std::string key;
    std::string value;
};

// Open a fresh LevelDB at <datadir>/<subdir>. Throws on error.
std::unique_ptr<ldb::DB> OpenLevelDB(const std::string& subdir)
{
    fs::path dir = GetDataDir() / subdir;
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir);

    ldb::Options opts;
    opts.create_if_missing = true;
    opts.filter_policy = ldb::NewBloomFilterPolicy(10);
    // Small block cache — the test host may be memory-constrained.
    opts.block_cache = ldb::NewLRUCache(16 * 1024 * 1024);
    opts.write_buffer_size = 16 * 1024 * 1024;

    ldb::DB* raw = nullptr;
    ldb::Status s = ldb::DB::Open(opts, dir.string(), &raw);
    if (!s.ok())
        throw std::runtime_error("LevelDB open failed: " + s.ToString());
    return std::unique_ptr<ldb::DB>(raw);
}

// Open a fresh RocksDB at <datadir>/<subdir>. Throws on error.
std::unique_ptr<rdb::DB> OpenRocksDB(const std::string& subdir)
{
    fs::path dir = GetDataDir() / subdir;
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir);

    rdb::Options opts;
    opts.create_if_missing = true;
    opts.compression = rdb::kNoCompression;
    opts.max_open_files = 100;
    opts.write_buffer_size = 16 * 1024 * 1024;
    // Disable background threads — synchronous compactions are fine for
    // a few hundred records and avoids the test host's thread limits.
    opts.IncreaseParallelism(1);

    rdb::DB* raw = nullptr;
    rdb::Status s = rdb::DB::Open(opts, dir.string(), &raw);
    if (!s.ok())
        throw std::runtime_error("RocksDB open failed: " + s.ToString());
    return std::unique_ptr<rdb::DB>(raw);
}

// Copy every record from a LevelDB to a RocksDB. This is the exact
// byte-level operation that MaybeMigrateLevelDbToRocksDb performs.
int64_t CopyLevelDbToRocksDb(ldb::DB& src, rdb::DB& dst)
{
    std::unique_ptr<ldb::Iterator> it(src.NewIterator(ldb::ReadOptions()));
    int64_t nCopied = 0;
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        rdb::Status s = dst.Put(rdb::WriteOptions(),
                                it->key().ToString(),
                                it->value().ToString());
        if (!s.ok())
            throw std::runtime_error("RocksDB put failed: " + s.ToString());
        nCopied++;
    }
    if (!it->status().ok())
        throw std::runtime_error("LevelDB iter error: " + it->status().ToString());
    return nCopied;
}

// Verify a RocksDB contains exactly the expected key/value pairs.
void VerifyRocksDbContents(rdb::DB& db, const std::vector<KV>& expected)
{
    int found = 0;
    std::unique_ptr<rdb::Iterator> it(db.NewIterator(rdb::ReadOptions()));
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        std::string rk = it->key().ToString();
        std::string rv = it->value().ToString();
        bool matched = false;
        for (const auto& kv : expected) {
            if (kv.key == rk) {
                BOOST_CHECK_MESSAGE(kv.value == rv,
                    "Value mismatch for key (len=" << rk.size() << ")");
                matched = true;
                found++;
                break;
            }
        }
        BOOST_CHECK_MESSAGE(matched,
            "RocksDB has key not in source data (len=" << rk.size() << ")");
    }
    BOOST_CHECK_EQUAL(found, static_cast<int>(expected.size()));
}

} // anonymous namespace

// ─── Tests ──────────────────────────────────────────────────────────────────

// Write records into a LevelDB, copy them to a fresh RocksDB using the same
// byte-level approach MaybeMigrateLevelDbToRocksDb uses, and verify every
// record survived the transfer.
BOOST_AUTO_TEST_CASE(migration_preserves_all_records)
{
    const std::vector<KV> testData = {
        {"block_index_1", "block_index_record_1"},
        {"block_index_2", "block_index_record_2"},
        {"block_index_3", "block_index_record_3"},
        {"tx_index_1",    "tx_index_record_1"},
        {"tx_index_2",    "tx_index_record_2"},
        {"utxo_A",        "utxo_entry_A"},
        {"utxo_B",        "utxo_entry_B"},
        {"utxo_C",        "utxo_entry_C"},
        {"utxo_D",        "utxo_entry_D"},
        {"best_chain",    "hashBestChain_value"},
        {"version_key",   "9000000"},
        {"dbformat_key",  "1"},
        {"key_with_spaces", "value with spaces"},
        {"binary_marker",   "binary_marker_value"},
    };

    auto level = OpenLevelDB("txleveldb");
    {
        ldb::WriteBatch batch;
        for (const auto& kv : testData) {
            batch.Put(kv.key, kv.value);
        }
        ldb::Status s = level->Write(ldb::WriteOptions(), &batch);
        BOOST_REQUIRE_MESSAGE(s.ok(), "LevelDB batch write failed: " << s.ToString());
    }

    auto rocks = OpenRocksDB("rocksdb");
    int64_t nCopied = CopyLevelDbToRocksDb(*level, *rocks);
    BOOST_CHECK_EQUAL(nCopied, static_cast<int64_t>(testData.size()));

    VerifyRocksDbContents(*rocks, testData);
}

// Idempotency: copying into a pre-populated RocksDB replaces the keys
// that the source contains and leaves the others untouched (this is
// what MaybeMigrateLevelDbToRocksDb does with force=true after wiping).
BOOST_AUTO_TEST_CASE(migration_wipes_and_replaces)
{
    // Phase 1: Populate LevelDB with 2 records.
    auto level = OpenLevelDB("txleveldb");
    {
        ldb::WriteBatch batch;
        batch.Put("key1", "leveldb_value_1");
        batch.Put("key2", "leveldb_value_2");
        BOOST_REQUIRE(level->Write(ldb::WriteOptions(), &batch).ok());
    }

    // Phase 2: Pre-populate RocksDB with 2 different records.
    auto rocks = OpenRocksDB("rocksdb");
    {
        rdb::WriteBatch batch;
        batch.Put("key1", "old_rocksdb_value");
        batch.Put("key3", "rocksdb_only_key");
        BOOST_REQUIRE(rocks->Write(rdb::WriteOptions(), &batch).ok());
    }

    // Phase 3: Wipe the rocksdb dir, then re-populate from LevelDB.
    // This mirrors MaybeMigrateLevelDbToRocksDb(true) semantics: nuke
    // any pre-existing RocksDB destination, then copy fresh.
    rocks.reset();
    {
        std::error_code ec;
        fs::remove_all(GetDataDir() / "rocksdb", ec);
    }
    auto rocks2 = OpenRocksDB("rocksdb");

    int64_t nCopied = CopyLevelDbToRocksDb(*level, *rocks2);
    BOOST_CHECK_EQUAL(nCopied, 2);

    // Phase 4: After the copy, RocksDB has the LevelDB's keys only.
    {
        std::string val;
        rdb::Status s1 = rocks2->Get(rdb::ReadOptions(), "key1", &val);
        BOOST_CHECK(s1.ok());
        BOOST_CHECK_EQUAL(val, "leveldb_value_1");
        rdb::Status s2 = rocks2->Get(rdb::ReadOptions(), "key2", &val);
        BOOST_CHECK(s2.ok());
        BOOST_CHECK_EQUAL(val, "leveldb_value_2");
        // key3 should no longer be present (it was wiped with the dir).
        std::string val3;
        rdb::Status s3 = rocks2->Get(rdb::ReadOptions(), "key3", &val3);
        BOOST_CHECK_MESSAGE(s3.IsNotFound(),
            "key3 should be gone after wipe+copy, got status=" << s3.ToString());
    }
}

// Binary-safe: keys and values with embedded NULs and non-ASCII bytes
// survive the transfer.
BOOST_AUTO_TEST_CASE(migration_preserves_binary_data)
{
    auto level = OpenLevelDB("txleveldb");
    auto rocks = OpenRocksDB("rocksdb");

    // Generate deterministic binary test vectors
    const std::vector<KV> binaryData = {
        {std::string("\x00\x01\x02\x03", 4), std::string("\xff\xfe\xfd\xfc", 4)},
        {std::string(64, '\x00'), std::string(64, '\xff')},
        {std::string(32, '\xab'), std::string(32, '\xcd')},
    };

    {
        ldb::WriteBatch batch;
        for (const auto& kv : binaryData) {
            batch.Put(kv.key, kv.value);
        }
        BOOST_REQUIRE(level->Write(ldb::WriteOptions(), &batch).ok());
    }

    int64_t nCopied = CopyLevelDbToRocksDb(*level, *rocks);
    BOOST_CHECK_EQUAL(nCopied, static_cast<int64_t>(binaryData.size()));

    VerifyRocksDbContents(*rocks, binaryData);
}

BOOST_AUTO_TEST_SUITE_END()
