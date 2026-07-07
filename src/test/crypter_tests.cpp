// Wallet-encryption (CCrypter) tests. Added 2026-07-04 during the test audit.
// crypter.cpp had ZERO coverage despite guarding every encrypted wallet: a
// bug here corrupts keys or weakens protection. These are round-trip,
// negative, and determinism checks (no brittle hard-coded ciphertext).
#include <boost/test/unit_test.hpp>

#include "../crypter.h"
#include "../key.h"

#include <string>
#include <vector>

BOOST_AUTO_TEST_SUITE(crypter_tests)

static std::vector<unsigned char> Salt8(unsigned char seed)
{
    return std::vector<unsigned char>(WALLET_CRYPTO_SALT_SIZE, seed);
}

static CKeyingMaterial MakePlain(const std::string& s)
{
    return CKeyingMaterial(s.begin(), s.end());
}

// sha512 KDF (method 0): passphrase -> encrypt -> decrypt round-trips.
BOOST_AUTO_TEST_CASE(passphrase_roundtrip_sha512)
{
    CCrypter c;
    BOOST_REQUIRE(c.SetKeyFromPassphrase(SecureString("correct horse"), Salt8(0x11), 1000, 0));

    CKeyingMaterial plain = MakePlain("a 32-byte secret payload here!!");
    std::vector<unsigned char> cipher;
    BOOST_REQUIRE(c.Encrypt(plain, cipher));
    BOOST_CHECK(cipher.size() >= plain.size());
    BOOST_CHECK(cipher != std::vector<unsigned char>(plain.begin(), plain.end()));

    CKeyingMaterial out;
    BOOST_REQUIRE(c.Decrypt(cipher, out));
    BOOST_CHECK(out == plain);
}

// scrypt KDF (method 1) round-trips too.
BOOST_AUTO_TEST_CASE(passphrase_roundtrip_scrypt)
{
    CCrypter c;
    BOOST_REQUIRE(c.SetKeyFromPassphrase(SecureString("correct horse"), Salt8(0x22), 100, 1));

    CKeyingMaterial plain = MakePlain("scrypt-derived key path payload");
    std::vector<unsigned char> cipher;
    BOOST_REQUIRE(c.Encrypt(plain, cipher));
    CKeyingMaterial out;
    BOOST_REQUIRE(c.Decrypt(cipher, out));
    BOOST_CHECK(out == plain);
}

// A different passphrase derives a different key: decryption must NOT recover
// the plaintext (AES-CBC padding check rejects the wrong key).
BOOST_AUTO_TEST_CASE(wrong_passphrase_fails)
{
    std::vector<unsigned char> salt = Salt8(0x33);
    CCrypter good;
    BOOST_REQUIRE(good.SetKeyFromPassphrase(SecureString("right pass"), salt, 1000, 0));
    CKeyingMaterial plain = MakePlain("top secret wallet material x");
    std::vector<unsigned char> cipher;
    BOOST_REQUIRE(good.Encrypt(plain, cipher));

    CCrypter bad;
    BOOST_REQUIRE(bad.SetKeyFromPassphrase(SecureString("wrong pass"), salt, 1000, 0));
    CKeyingMaterial out;
    bool ok = bad.Decrypt(cipher, out);
    // Either the padding check fails outright, or (rarely) it "succeeds" with
    // garbage — in no case may it recover the real plaintext.
    BOOST_CHECK(!ok || out != plain);
}

// Different salt => different derived key => different ciphertext.
BOOST_AUTO_TEST_CASE(salt_affects_key)
{
    CKeyingMaterial plain = MakePlain("same plaintext, two salts here");
    CCrypter a, b;
    BOOST_REQUIRE(a.SetKeyFromPassphrase(SecureString("pw"), Salt8(0x01), 1000, 0));
    BOOST_REQUIRE(b.SetKeyFromPassphrase(SecureString("pw"), Salt8(0x02), 1000, 0));
    std::vector<unsigned char> ca, cb;
    BOOST_REQUIRE(a.Encrypt(plain, ca));
    BOOST_REQUIRE(b.Encrypt(plain, cb));
    BOOST_CHECK(ca != cb);
}

// Same passphrase+salt+rounds is deterministic (fixed key+IV, AES-CBC).
BOOST_AUTO_TEST_CASE(derivation_is_deterministic)
{
    CKeyingMaterial plain = MakePlain("deterministic check payload!!");
    CCrypter a, b;
    BOOST_REQUIRE(a.SetKeyFromPassphrase(SecureString("pw"), Salt8(0x44), 2000, 0));
    BOOST_REQUIRE(b.SetKeyFromPassphrase(SecureString("pw"), Salt8(0x44), 2000, 0));
    std::vector<unsigned char> ca, cb;
    BOOST_REQUIRE(a.Encrypt(plain, ca));
    BOOST_REQUIRE(b.Encrypt(plain, cb));
    BOOST_CHECK(ca == cb);
}

// Bad parameters are rejected: zero rounds and wrong salt length.
BOOST_AUTO_TEST_CASE(bad_params_rejected)
{
    CCrypter c;
    BOOST_CHECK(!c.SetKeyFromPassphrase(SecureString("pw"), Salt8(0x55), 0, 0));
    std::vector<unsigned char> shortSalt(WALLET_CRYPTO_SALT_SIZE - 1, 0x00);
    BOOST_CHECK(!c.SetKeyFromPassphrase(SecureString("pw"), shortSalt, 1000, 0));
    // Encrypt before any key is set must fail.
    CCrypter unset;
    std::vector<unsigned char> cipher;
    BOOST_CHECK(!unset.Encrypt(MakePlain("x"), cipher));
}

// The actual wallet key-encryption path: EncryptSecret/DecryptSecret with a
// 32-byte master key and a uint256 IV round-trips a private-key-sized secret.
BOOST_AUTO_TEST_CASE(encrypt_secret_roundtrip)
{
    CKeyingMaterial master(WALLET_CRYPTO_KEY_SIZE, 0xAB);
    uint256 iv("0x0102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f20");

    CSecret secret;
    for (int i = 0; i < 32; i++) secret.push_back((unsigned char)(i * 7 + 1));

    std::vector<unsigned char> cipher;
    BOOST_REQUIRE(EncryptSecret(master, secret, iv, cipher));
    BOOST_CHECK(cipher.size() >= secret.size());

    CSecret recovered;
    BOOST_REQUIRE(DecryptSecret(master, cipher, iv, recovered));
    BOOST_CHECK(recovered == secret);

    // Wrong IV must not recover the secret. NOTE: uint256 hex is big-endian
    // for display but little-endian in memory, and AES-256-CBC uses only the
    // FIRST 16 memory bytes as the IV. So we must perturb a low-order byte
    // (the trailing hex pair), which maps to memory byte 0 -- inside the AES
    // IV window. A wrong IV corrupts the first plaintext block, so the full
    // 32-byte secret cannot be recovered intact.
    uint256 iv2("0x0102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f21");
    CSecret wrong;
    bool ok = DecryptSecret(master, cipher, iv2, wrong);
    BOOST_CHECK(!ok || wrong != secret);
}

// Flipping a ciphertext byte must break decryption (padding/integrity).
BOOST_AUTO_TEST_CASE(tampered_ciphertext_fails)
{
    CCrypter c;
    BOOST_REQUIRE(c.SetKeyFromPassphrase(SecureString("pw"), Salt8(0x66), 1000, 0));
    CKeyingMaterial plain = MakePlain("integrity of this block matters");
    std::vector<unsigned char> cipher;
    BOOST_REQUIRE(c.Encrypt(plain, cipher));

    cipher[cipher.size() - 1] ^= 0x01; // corrupt last block
    CKeyingMaterial out;
    bool ok = c.Decrypt(cipher, out);
    BOOST_CHECK(!ok || out != plain);
}

BOOST_AUTO_TEST_SUITE_END()
