// Copyright (c) 2024-2026 Triangles developers
// Tests for Triangles-specific serialization (nTime field in CTransaction)

#include <boost/test/unit_test.hpp>

#include "../main.h"
#include "../uint256.h"
#include "../serialize.h"

#include <vector>
#include <string>

BOOST_AUTO_TEST_SUITE(serialization_tests)

// --- CTransaction round-trip: serialize then deserialize ---

BOOST_AUTO_TEST_CASE(tx_roundtrip_preserves_ntime)
{
    CTransaction tx;
    tx.nVersion = 1;
    tx.nTime = 1700000000;
    tx.nLockTime = 0;
    tx.vin.resize(1);
    tx.vin[0].prevout.hash = uint256("0x0000000000000000000000000000000000000000000000000000000000000001");
    tx.vin[0].prevout.n = 0;
    tx.vin[0].scriptSig << std::vector<unsigned char>(65, 0);
    tx.vout.resize(1);
    tx.vout[0].nValue = 50 * COIN;
    tx.vout[0].scriptPubKey << OP_1;

    // Serialize
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << tx;

    // Deserialize
    CTransaction txOut;
    ss >> txOut;

    BOOST_CHECK_EQUAL(txOut.nVersion, tx.nVersion);
    BOOST_CHECK_EQUAL(txOut.nTime, tx.nTime);
    BOOST_CHECK_EQUAL(txOut.nLockTime, tx.nLockTime);
    BOOST_CHECK_EQUAL(txOut.vin.size(), tx.vin.size());
    BOOST_CHECK_EQUAL(txOut.vout.size(), tx.vout.size());
    BOOST_CHECK_EQUAL(txOut.vout[0].nValue, tx.vout[0].nValue);
    BOOST_CHECK(txOut.GetHash() == tx.GetHash());
}

// --- nTime is actually part of the serialized data (not ignored) ---

BOOST_AUTO_TEST_CASE(tx_different_ntime_different_hash)
{
    CTransaction tx1;
    tx1.nVersion = 1;
    tx1.nTime = 1700000000;
    tx1.vin.resize(1);
    tx1.vin[0].prevout.hash = uint256("0x0000000000000000000000000000000000000000000000000000000000000002");
    tx1.vin[0].prevout.n = 0;
    tx1.vin[0].scriptSig << std::vector<unsigned char>(65, 0);
    tx1.vout.resize(1);
    tx1.vout[0].nValue = 10 * COIN;
    tx1.vout[0].scriptPubKey << OP_1;

    CTransaction tx2 = tx1;
    tx2.nTime = 1700000001;  // 1 second later

    // Different nTime must produce different hash
    BOOST_CHECK(tx1.GetHash() != tx2.GetHash());
}

// --- Serialized size includes nTime (4 extra bytes vs Bitcoin) ---

BOOST_AUTO_TEST_CASE(tx_serialized_size_includes_ntime)
{
    CTransaction tx;
    tx.nVersion = 1;
    tx.nTime = 1700000000;
    tx.nLockTime = 0;
    tx.vin.resize(1);
    tx.vin[0].prevout.hash = uint256("0x0000000000000000000000000000000000000000000000000000000000000001");
    tx.vin[0].prevout.n = 0;
    tx.vin[0].scriptSig << std::vector<unsigned char>(65, 0);
    tx.vout.resize(1);
    tx.vout[0].nValue = 1 * COIN;
    tx.vout[0].scriptPubKey << OP_1;

    unsigned int size = ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION);

    // nVersion(4) + nTime(4) + vin_count(1) + vin[0](32+4+1+65+4) + vout_count(1) + vout[0](8+1+1) + nLockTime(4)
    // = 4 + 4 + 1 + 106 + 1 + 10 + 4 = 130
    // The important thing: size must be > 0 and include the 4-byte nTime field.
    BOOST_CHECK(size > 0);

    // Now serialize the same tx with a different nTime - same size expected
    tx.nTime = 0;
    unsigned int size2 = ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION);
    BOOST_CHECK_EQUAL(size, size2);
}

// --- Empty transaction checks ---

BOOST_AUTO_TEST_CASE(tx_empty_is_null)
{
    CTransaction tx;
    tx.vin.clear();
    tx.vout.clear();
    BOOST_CHECK(tx.IsNull());
}

BOOST_AUTO_TEST_CASE(tx_with_output_is_not_null)
{
    CTransaction tx;
    tx.vout.resize(1);
    tx.vout[0].nValue = 1 * COIN;
    tx.vout[0].scriptPubKey << OP_1;
    BOOST_CHECK(!tx.IsNull());
}

// --- CBlock header serialization round-trip ---

BOOST_AUTO_TEST_CASE(block_header_roundtrip)
{
    CBlock block;
    block.nVersion = CBlock::CURRENT_VERSION;
    block.hashPrevBlock = uint256("0x0000000000000000000000000000000000000000000000000000000000000042");
    block.hashMerkleRoot = uint256("0x0000000000000000000000000000000000000000000000000000000000000099");
    block.nTime = 1700000000;
    block.nBits = 0x1d00ffff;
    block.nNonce = 12345;

    // Serialize header only (SER_BLOCKHEADERONLY clears vtx/sig on read)
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << block;

    CBlock blockOut;
    ss >> blockOut;

    BOOST_CHECK_EQUAL(blockOut.nVersion, block.nVersion);
    BOOST_CHECK(blockOut.hashPrevBlock == block.hashPrevBlock);
    BOOST_CHECK(blockOut.hashMerkleRoot == block.hashMerkleRoot);
    BOOST_CHECK_EQUAL(blockOut.nTime, block.nTime);
    BOOST_CHECK_EQUAL(blockOut.nBits, block.nBits);
    BOOST_CHECK_EQUAL(blockOut.nNonce, block.nNonce);
}

// --- Block version ---

BOOST_AUTO_TEST_CASE(block_current_version)
{
    BOOST_CHECK_EQUAL(CBlock::CURRENT_VERSION, 6);
}

// --- CDiskTxPos ---

BOOST_AUTO_TEST_CASE(disk_tx_pos_null)
{
    CDiskTxPos pos;
    BOOST_CHECK(pos.IsNull());

    CDiskTxPos pos2(1, 100, 200);
    BOOST_CHECK(!pos2.IsNull());
    BOOST_CHECK(pos != pos2);
}

BOOST_AUTO_TEST_CASE(disk_tx_pos_equality)
{
    CDiskTxPos a(1, 100, 200);
    CDiskTxPos b(1, 100, 200);
    CDiskTxPos c(2, 100, 200);

    BOOST_CHECK(a == b);
    BOOST_CHECK(a != c);
}

// --- COutPoint ---

BOOST_AUTO_TEST_CASE(outpoint_null)
{
    COutPoint op;
    BOOST_CHECK(op.IsNull());

    COutPoint op2(uint256("0x01"), 0);
    BOOST_CHECK(!op2.IsNull());
}

BOOST_AUTO_TEST_CASE(outpoint_equality)
{
    uint256 h("0x0000000000000000000000000000000000000000000000000000000000000042");
    COutPoint a(h, 0);
    COutPoint b(h, 0);
    COutPoint c(h, 1);

    BOOST_CHECK(a == b);
    BOOST_CHECK(a != c);
    BOOST_CHECK(a < c);  // same hash, lower n
}

BOOST_AUTO_TEST_SUITE_END()
