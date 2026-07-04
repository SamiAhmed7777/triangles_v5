// HD wallet (BIP39 + BIP32) tests. Added 2026-07-04 during the test audit —
// this security-critical derivation path previously had ZERO coverage.
//
// Vectors are the canonical ones:
//   - BIP39: Trezor english test vector (all-zero 128-bit entropy).
//   - BIP32: test vector 1 from the BIP32 spec.
#include <boost/test/unit_test.hpp>

#include "../hdwallet.h"

#include <string>
#include <vector>
#include <cstdio>

namespace {

std::string ToHex(const unsigned char* p, size_t n)
{
    static const char* h = "0123456789abcdef";
    std::string s;
    s.reserve(n * 2);
    for (size_t i = 0; i < n; i++) { s += h[p[i] >> 4]; s += h[p[i] & 0xf]; }
    return s;
}

} // namespace

BOOST_AUTO_TEST_SUITE(hd_wallet_tests)

// BIP39 Trezor vector: all-zero 128-bit entropy -> known 12-word phrase, and
// with passphrase "TREZOR" -> known 64-byte seed.
BOOST_AUTO_TEST_CASE(bip39_trezor_vector)
{
    const std::string mnemonic =
        "abandon abandon abandon abandon abandon abandon "
        "abandon abandon abandon abandon abandon about";

    BOOST_CHECK(hd::CheckMnemonic(mnemonic));

    unsigned char seed[64];
    BOOST_CHECK(hd::MnemonicToSeed(mnemonic, "TREZOR", seed));
    BOOST_CHECK_EQUAL(
        ToHex(seed, 64),
        "c55257c360c07c72029aebc1b53c05ed0362ada38ead3e3e9efa3708e5349553"
        "1f09a6987599d18264c1e1c92f2cf141630c7a3c4ab7c81b2f001698e7463b04");
}

// A phrase with a corrupted checksum word must be rejected.
BOOST_AUTO_TEST_CASE(bip39_bad_checksum_rejected)
{
    // Same as the Trezor phrase but last word swapped to another valid word,
    // which breaks the checksum.
    const std::string bad =
        "abandon abandon abandon abandon abandon abandon "
        "abandon abandon abandon abandon abandon abandon";
    BOOST_CHECK(!hd::CheckMnemonic(bad));

    // Non-wordlist token must also be rejected.
    BOOST_CHECK(!hd::CheckMnemonic("zzzz not real bip39 words here at all foo bar baz qux"));
    // Wrong word count.
    BOOST_CHECK(!hd::CheckMnemonic("abandon abandon abandon"));
}

// BIP32 test vector 1: seed 000102...0f -> known master key + chain code,
// and m/0H -> known child key + chain code.
BOOST_AUTO_TEST_CASE(bip32_vector1_master_and_hardened_child)
{
    unsigned char seed[16];
    for (int i = 0; i < 16; i++) seed[i] = (unsigned char)i;

    hd::ExtKey master;
    BOOST_CHECK(hd::MasterFromSeed(seed, sizeof(seed), master));
    BOOST_CHECK_EQUAL(ToHex(master.key, 32),
        "e8f32e723decf4051aefac8e2c93c9c5b214313817cdb01a1494b917c8436b35");
    BOOST_CHECK_EQUAL(ToHex(master.chaincode, 32),
        "873dff81c02f525623fd1fe5167eac3a55a049de3d314bb42ee227ffed37d508");

    hd::ExtKey child;
    BOOST_CHECK(hd::CKDpriv(master, 0u | hd::HARDENED, child));
    BOOST_CHECK_EQUAL(ToHex(child.key, 32),
        "edb2e14f9ee77d26dd93b4ecede8d16ed408ce149b6cd80b0715a2d911a0afea");
    BOOST_CHECK_EQUAL(ToHex(child.chaincode, 32),
        "47fdacbd0f1097043b78c63c20c34ef4ed9a111d980047ad16282c7ae6236141");
}

// DeriveTriangles must be deterministic and index-sensitive.
BOOST_AUTO_TEST_CASE(derive_triangles_deterministic)
{
    const std::string mnemonic =
        "abandon abandon abandon abandon abandon abandon "
        "abandon abandon abandon abandon abandon about";

    unsigned char a[32], b[32], c[32];
    BOOST_CHECK(hd::DeriveTriangles(mnemonic, "", 0, 0, 0, a));
    BOOST_CHECK(hd::DeriveTriangles(mnemonic, "", 0, 0, 0, b));
    BOOST_CHECK(hd::DeriveTriangles(mnemonic, "", 0, 0, 1, c));

    // Same path -> identical key.
    BOOST_CHECK_EQUAL(ToHex(a, 32), ToHex(b, 32));
    // Different index -> different key.
    BOOST_CHECK(ToHex(a, 32) != ToHex(c, 32));
}

BOOST_AUTO_TEST_SUITE_END()
