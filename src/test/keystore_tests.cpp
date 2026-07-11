// Copyright (c) 2026 Triangles developers
// Tests for CKeyStore / CBasicKeyStore / CCryptoKeyStore
//
// Added 2026-07-06 during the test audit. The keystore layer guards every
// spendable key in the wallet: a bug here can lose keys, accept wrong keys,
// or break encryption round-trips. CCrypter itself is covered by
// crypter_tests.cpp -- this suite focuses on the keystore's map operations,
// lock/unlock state machine, and the encrypt-on-AddKey / decrypt-on-GetKey
// flow that combines CCrypter with the keystore.
//
// No new crypto primitives are introduced -- we exercise existing
// CKeyStore / CCryptoKeyStore public APIs. Test vectors come from running
// the code itself under observation (round-trip patterns) rather than from
// hand-written hex values.

#include <boost/test/unit_test.hpp>

#include "../keystore.h"
#include "../key.h"
#include "../script.h"
#include "../crypter.h"

#include <string>
#include <vector>

BOOST_AUTO_TEST_SUITE(keystore_tests)

// Test-only subclass that exposes the protected Unlock/EncryptKeys paths.
// In production these are called by CWallet after reading the master key
// from disk; from a unit test we don't have that driver, so we widen the
// access narrowly for testing. The override is a passthrough (no behavior
// change) -- it exists only so the test can drive the protected methods
// without modifying production code.
class TestableCryptoKeyStore : public CCryptoKeyStore
{
public:
    using CCryptoKeyStore::Unlock;
    using CCryptoKeyStore::EncryptKeys;
};

// Helper: derive a deterministic master key from a passphrase for use in
// encryption tests. Avoids hand-written 64-byte hex strings (see
// crypto-primitive-vendoring pitfall #8).
static CKeyingMaterial DeriveMasterKey(const std::string& passphrase)
{
    CKeyingMaterial vMasterKey;
    RandAddSeedPerfmon();
    vMasterKey.resize(WALLET_CRYPTO_KEY_SIZE);
    // Passphrase hash truncated to WALLET_CRYPTO_KEY_SIZE matches the
    // wallet's own pre-key setup in CCryptoKeyStore::Unlock.
    auto hash = Hash(passphrase.begin(), passphrase.end());
    memcpy(vMasterKey.data(), hash.begin(),
           std::min((size_t)WALLET_CRYPTO_KEY_SIZE, (size_t)hash.size()));
    return vMasterKey;
}

// --- CBasicKeyStore: plain (unencrypted) key storage ---

BOOST_AUTO_TEST_CASE(basic_keystore_add_then_have)
{
    CBasicKeyStore ks;
    CKey key;
    key.MakeNewKey(true);

    BOOST_CHECK(ks.AddKey(key));
    BOOST_CHECK(ks.HaveKey(key.GetPubKey().GetID()));
}

BOOST_AUTO_TEST_CASE(basic_keystore_have_missing_returns_false)
{
    CBasicKeyStore ks;
    CKey key;
    key.MakeNewKey(true);

    BOOST_CHECK(!ks.HaveKey(key.GetPubKey().GetID()));
}

BOOST_AUTO_TEST_CASE(basic_keystore_get_roundtrip)
{
    CBasicKeyStore ks;
    CKey key;
    key.MakeNewKey(true);
    ks.AddKey(key);

    CKey recovered;
    BOOST_CHECK(ks.GetKey(key.GetPubKey().GetID(), recovered));

    // The recovered key must produce the same public key (proof of
    // faithful round-trip of the underlying secret bytes).
    BOOST_CHECK(recovered.GetPubKey() == key.GetPubKey());
}

BOOST_AUTO_TEST_CASE(basic_keystore_get_missing_returns_false)
{
    CBasicKeyStore ks;
    CKey key;
    key.MakeNewKey(true);

    CKey recovered;
    BOOST_CHECK(!ks.GetKey(key.GetPubKey().GetID(), recovered));
}

BOOST_AUTO_TEST_CASE(basic_keystore_get_pubkey_matches_get_key)
{
    // CKeyStore::GetPubKey default impl calls GetKey then derives pubkey;
    // verify the two paths agree.
    CBasicKeyStore ks;
    CKey key;
    key.MakeNewKey(true);
    ks.AddKey(key);

    CKey recovered;
    CPubKey pub;
    BOOST_CHECK(ks.GetKey(key.GetPubKey().GetID(), recovered));
    BOOST_CHECK(ks.GetPubKey(key.GetPubKey().GetID(), pub));
    BOOST_CHECK(pub == key.GetPubKey());
    BOOST_CHECK(pub == recovered.GetPubKey());
}

BOOST_AUTO_TEST_CASE(basic_keystore_get_pubkey_missing_returns_false)
{
    CBasicKeyStore ks;
    CKey key;
    key.MakeNewKey(true);

    CPubKey pub;
    BOOST_CHECK(!ks.GetPubKey(key.GetPubKey().GetID(), pub));
}

BOOST_AUTO_TEST_CASE(basic_keystore_get_secret_compressed_flag_preserved)
{
    // The keystore stores (secret, compressed) pairs. A compressed key
    // added must come back as a compressed key.
    CBasicKeyStore ks;
    CKey compressed;
    compressed.MakeNewKey(true);  // compressed=true
    ks.AddKey(compressed);

    CSecret secret;
    bool fCompressed = false;
    BOOST_CHECK(ks.GetSecret(compressed.GetPubKey().GetID(), secret, fCompressed));
    BOOST_CHECK(fCompressed);

    // Now an uncompressed key.
    CBasicKeyStore ks2;
    CKey uncompressed;
    uncompressed.MakeNewKey(false);  // compressed=false
    ks2.AddKey(uncompressed);

    BOOST_CHECK(ks2.GetSecret(uncompressed.GetPubKey().GetID(), secret, fCompressed));
    BOOST_CHECK(!fCompressed);
}

BOOST_AUTO_TEST_CASE(basic_keystore_getkeys_returns_all_added)
{
    CBasicKeyStore ks;
    CKey k1, k2, k3;
    k1.MakeNewKey(true);
    k2.MakeNewKey(true);
    k3.MakeNewKey(true);
    ks.AddKey(k1);
    ks.AddKey(k2);
    ks.AddKey(k3);

    std::set<CKeyID> setAddr;
    ks.GetKeys(setAddr);
    BOOST_CHECK_EQUAL(setAddr.size(), 3u);
    BOOST_CHECK(setAddr.count(k1.GetPubKey().GetID()) == 1);
    BOOST_CHECK(setAddr.count(k2.GetPubKey().GetID()) == 1);
    BOOST_CHECK(setAddr.count(k3.GetPubKey().GetID()) == 1);
}

BOOST_AUTO_TEST_CASE(basic_keystore_getkeys_empty_store)
{
    CBasicKeyStore ks;
    std::set<CKeyID> setAddr;
    ks.GetKeys(setAddr);
    BOOST_CHECK_EQUAL(setAddr.size(), 0u);
}

BOOST_AUTO_TEST_CASE(basic_keystore_getkeys_clears_input_set)
{
    // GetKeys must clear the caller's set first -- if it didn't, leftover
    // entries from a prior call would silently corrupt downstream code.
    CBasicKeyStore ks;
    CKey k;
    k.MakeNewKey(true);
    ks.AddKey(k);

    std::set<CKeyID> setAddr;
    setAddr.insert(uint160(42));  // garbage left in
    ks.GetKeys(setAddr);
    BOOST_CHECK_EQUAL(setAddr.size(), 1u);  // only the real key, garbage gone
}

// --- CBasicKeyStore: CScript storage (BIP-0013 / P2SH) ---

BOOST_AUTO_TEST_CASE(basic_keystore_addcscript_then_have)
{
    CBasicKeyStore ks;
    CScript script = CScript() << OP_1 << OP_2 << OP_3;

    BOOST_CHECK(ks.AddCScript(script));
    BOOST_CHECK(ks.HaveCScript(script.GetID()));
}

BOOST_AUTO_TEST_CASE(basic_keystore_havecscript_missing)
{
    CBasicKeyStore ks;
    CScript script = CScript() << OP_1 << OP_2 << OP_3;
    BOOST_CHECK(!ks.HaveCScript(script.GetID()));
}

BOOST_AUTO_TEST_CASE(basic_keystore_getcscript_roundtrip)
{
    CBasicKeyStore ks;
    CScript original = CScript() << OP_DUP << OP_HASH160 <<
        std::vector<unsigned char>{0x01, 0x02, 0x03} << OP_EQUALVERIFY << OP_CHECKSIG;
    ks.AddCScript(original);

    CScript recovered;
    BOOST_CHECK(ks.GetCScript(original.GetID(), recovered));
    BOOST_CHECK(recovered == original);
}

BOOST_AUTO_TEST_CASE(basic_keystore_getcscript_missing)
{
    CBasicKeyStore ks;
    CScript script = CScript() << OP_1;
    CScript recovered;
    BOOST_CHECK(!ks.GetCScript(script.GetID(), recovered));
}

BOOST_AUTO_TEST_CASE(basic_keystore_addcscript_idempotent)
{
    // Adding the same script twice must NOT corrupt the store. The second
    // insert just replaces the value at the same script ID.
    CBasicKeyStore ks;
    CScript s = CScript() << OP_1 << OP_2;
    ks.AddCScript(s);
    ks.AddCScript(s);
    BOOST_CHECK(ks.HaveCScript(s.GetID()));
}

// --- CCryptoKeyStore: state machine (IsCrypted / IsLocked) ---

BOOST_AUTO_TEST_CASE(crypto_keystore_starts_uncrypted_unlocked)
{
    TestableCryptoKeyStore cks;
    BOOST_CHECK(!cks.IsCrypted());
    BOOST_CHECK(!cks.IsLocked());
}

BOOST_AUTO_TEST_CASE(crypto_keystore_lock_sets_crypted)
{
    // LockKeyStore flips the store into crypted mode (forced SetCrypted)
    // and clears the master key. After Lock, IsCrypted() && IsLocked().
    TestableCryptoKeyStore cks;
    BOOST_CHECK(cks.LockKeyStore());
    BOOST_CHECK(cks.IsCrypted());
    BOOST_CHECK(cks.IsLocked());
}

BOOST_AUTO_TEST_CASE(crypto_keystore_lock_with_plain_keys_refuses)
{
    // The SetCrypted precondition: if mapKeys is non-empty, we refuse to
    // switch to crypted mode (those plain keys would be lost). Must call
    // EncryptKeys first to migrate them.
    TestableCryptoKeyStore cks;
    CKey k;
    k.MakeNewKey(true);
    BOOST_CHECK(cks.AddKey(k));  // goes into mapKeys (uncrypted path)
    BOOST_CHECK(!cks.LockKeyStore());  // must refuse: plaintext keys exist
}

// --- CCryptoKeyStore: encrypt / decrypt round trip ---

BOOST_AUTO_TEST_CASE(crypto_keystore_addkey_when_locked_refuses)
{
    // Locked store has no master key to encrypt new secrets with. AddKey
    // must refuse rather than silently insert a plaintext key.
    TestableCryptoKeyStore cks;
    cks.LockKeyStore();
    CKey k;
    k.MakeNewKey(true);
    BOOST_CHECK(!cks.AddKey(k));
}

BOOST_AUTO_TEST_CASE(crypto_keystore_encrypt_then_decrypt_roundtrip)
{
    // End-to-end: add key in plaintext mode, encrypt the store with a
    // passphrase-derived master key (EncryptKeys migrates plaintext ->
    // encrypted), then verify the key round-trips through lock/unlock
    // cycles.
    //
    // Important: Unlock() refuses when mapKeys is non-empty (SetCrypted's
    // precondition). EncryptKeys() is the bridge -- it moves plaintext
    // keys into the encrypted map. After EncryptKeys, the store is crypted
    // but the master key is NOT yet held (EncryptKeys never sets vMasterKey)
    // -- a subsequent Unlock() installs it. This is documented behavior;
    // the wallet layer sequences EncryptKeys + Unlock in that order when
    // migrating a wallet from unencrypted to encrypted.
    TestableCryptoKeyStore cks;
    CKey k;
    k.MakeNewKey(true);
    BOOST_CHECK(cks.AddKey(k));  // plain path -> mapKeys

    CKeyingMaterial master = DeriveMasterKey("correct horse battery staple");
    BOOST_CHECK(cks.EncryptKeys(master));  // migrate plaintext -> encrypted

    // After EncryptKeys: crypted mode on, but master key not yet held.
    BOOST_CHECK(cks.IsCrypted());
    BOOST_CHECK(cks.IsLocked());

    // Unlock installs the master key and verifies by attempting to decrypt.
    BOOST_CHECK(cks.Unlock(master));
    BOOST_CHECK(!cks.IsLocked());

    CKey recovered;
    BOOST_CHECK(cks.GetKey(k.GetPubKey().GetID(), recovered));
    BOOST_CHECK(recovered.GetPubKey() == k.GetPubKey());

    // Lock and verify we still get the right key back when unlocked.
    BOOST_CHECK(cks.LockKeyStore());
    BOOST_CHECK(cks.IsLocked());
    BOOST_CHECK(cks.Unlock(master));
    BOOST_CHECK(cks.GetKey(k.GetPubKey().GetID(), recovered));
    BOOST_CHECK(recovered.GetPubKey() == k.GetPubKey());
}

BOOST_AUTO_TEST_CASE(crypto_keystore_unlock_with_wrong_master_fails)
{
    // Unlock must reject a wrong master key without crashing. (DecryptSecret
    // returns false on bad material; Unlock propagates that.)
    //
    // Setup: build a fully encrypted store via Unlock on empty + AddKey +
    // LockKeyStore, so the second Unlock runs against a non-empty crypted
    // store.
    TestableCryptoKeyStore cks;
    CKey k;
    k.MakeNewKey(true);

    CKeyingMaterial correctMaster = DeriveMasterKey("the right one");
    CKeyingMaterial wrongMaster = DeriveMasterKey("the wrong one");

    // Bootstrap into the crypted state with the correct master.
    BOOST_CHECK(cks.Unlock(correctMaster));
    cks.AddKey(k);
    cks.LockKeyStore();

    BOOST_CHECK(!cks.Unlock(wrongMaster));
    // Correct master still works.
    BOOST_CHECK(cks.Unlock(correctMaster));
}

BOOST_AUTO_TEST_CASE(crypto_keystore_addkey_when_crypted_and_unlocked_encrypts)
{
    // After Unlock, AddKey should encrypt the new key on insert (not
    // silently drop it into mapKeys). We verify by locking, unlocking with
    // the same master, and reading the key back.
    TestableCryptoKeyStore cks;
    CKeyingMaterial master = DeriveMasterKey("test");
    BOOST_CHECK(cks.Unlock(master));  // creates empty crypted store

    CKey k;
    k.MakeNewKey(true);
    BOOST_CHECK(cks.AddKey(k));

    cks.LockKeyStore();
    BOOST_CHECK(cks.Unlock(master));

    CKey recovered;
    BOOST_CHECK(cks.GetKey(k.GetPubKey().GetID(), recovered));
    BOOST_CHECK(recovered.GetPubKey() == k.GetPubKey());
}

BOOST_AUTO_TEST_CASE(crypto_keystore_havekey_when_crypted_uses_crypted_map)
{
    // HaveKey's crypted-mode branch must look at mapCryptedKeys, not
    // mapKeys. Without this, HaveKey would say "no" for a key the store
    // can actually decrypt.
    TestableCryptoKeyStore cks;
    CKeyingMaterial master = DeriveMasterKey("test");
    cks.Unlock(master);

    CKey k;
    k.MakeNewKey(true);
    cks.AddKey(k);

    BOOST_CHECK(cks.HaveKey(k.GetPubKey().GetID()));
}

BOOST_AUTO_TEST_CASE(crypto_keystore_getkeys_crypted_lists_crypted_keys)
{
    // GetKeys in crypted mode must enumerate mapCryptedKeys, not mapKeys.
    // Empty mapKeys + populated mapCryptedKeys -> set contains the crypted
    // key.
    TestableCryptoKeyStore cks;
    CKeyingMaterial master = DeriveMasterKey("test");
    cks.Unlock(master);

    CKey k1, k2;
    k1.MakeNewKey(true);
    k2.MakeNewKey(true);
    cks.AddKey(k1);
    cks.AddKey(k2);

    std::set<CKeyID> setAddr;
    cks.GetKeys(setAddr);
    BOOST_CHECK_EQUAL(setAddr.size(), 2u);
    BOOST_CHECK(setAddr.count(k1.GetPubKey().GetID()) == 1);
    BOOST_CHECK(setAddr.count(k2.GetPubKey().GetID()) == 1);
}

// --- CCryptoKeyStore: GetPubKey in crypted mode ---

BOOST_AUTO_TEST_CASE(crypto_keystore_getpubkey_crypted_returns_stored_pubkey)
{
    // In crypted mode, GetPubKey must read from mapCryptedKeys (storing
    // the CPubKey alongside the encrypted secret) -- it can't derive pubkey
    // from the decrypted secret without the master key.
    TestableCryptoKeyStore cks;
    CKeyingMaterial master = DeriveMasterKey("test");
    cks.Unlock(master);

    CKey k;
    k.MakeNewKey(true);
    cks.AddKey(k);

    // Lock so GetPubKey must take the crypted-only path (no master key
    // available to derive pubkey from secret).
    cks.LockKeyStore();

    CPubKey pub;
    BOOST_CHECK(cks.GetPubKey(k.GetPubKey().GetID(), pub));
    BOOST_CHECK(pub == k.GetPubKey());
}

// --- CCryptoKeyStore: edge cases ---

BOOST_AUTO_TEST_CASE(crypto_keystore_unlock_empty_store_succeeds)
{
    // Unlocking an empty crypted store must succeed -- there's nothing to
    // verify, so any master key (even "wrong") is acceptable. (The
    // for-loop body never executes, the for-range is empty.)
    TestableCryptoKeyStore cks;
    BOOST_CHECK(cks.Unlock(DeriveMasterKey("anything")));
    BOOST_CHECK(cks.IsCrypted());
    BOOST_CHECK(!cks.IsLocked());
}

BOOST_AUTO_TEST_CASE(crypto_keystore_double_unlock_succeeds)
{
    // Calling Unlock twice with the same master is idempotent: the second
    // call re-decrypts and re-sets the master key. Both calls succeed.
    TestableCryptoKeyStore cks;
    CKeyingMaterial master = DeriveMasterKey("test");
    cks.Unlock(master);

    CKey k;
    k.MakeNewKey(true);
    cks.AddKey(k);

    BOOST_CHECK(cks.Unlock(master));
    BOOST_CHECK(cks.Unlock(master));

    CKey recovered;
    BOOST_CHECK(cks.GetKey(k.GetPubKey().GetID(), recovered));
    BOOST_CHECK(recovered.GetPubKey() == k.GetPubKey());
}

BOOST_AUTO_TEST_SUITE_END()