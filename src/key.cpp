// Copyright (c) 2009-2012 The Bitcoin developers
// Copyright (c) 2026 The Triangles developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <cstring>

#include <openssl/crypto.h>   // OPENSSL_cleanse for secure wipe of secret bytes
#include <openssl/rand.h>     // RAND_bytes for new-key entropy

#include "crypto_ecdsa.h"
#include "key.h"

// ─────────────────────────────────────────────────────────────────────────────
// Order-of-generator constants (still used by CheckSignatureElement, the only
// caller into the BigEndian comparison helper below). Kept here so the file
// remains self-contained.
// ─────────────────────────────────────────────────────────────────────────────

namespace {

int CompareBigEndian(const unsigned char* c1, std::size_t c1len,
                     const unsigned char* c2, std::size_t c2len)
{
    while (c1len > c2len) { if (*c1) return  1; c1++; c1len--; }
    while (c2len > c1len) { if (*c2) return -1; c2++; c2len--; }
    while (c1len > 0) {
        if (*c1 > *c2) return  1;
        if (*c2 > *c1) return -1;
        c1++; c2++; c1len--;
    }
    return 0;
}

// Order of secp256k1's generator minus 1.
const unsigned char vchMaxModOrder[32] = {
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFE,
    0xBA,0xAE,0xDC,0xE6,0xAF,0x48,0xA0,0x3B,
    0xBF,0xD2,0x5E,0x8C,0xD0,0x36,0x41,0x40
};

// Half of the order of secp256k1's generator minus 1.
const unsigned char vchMaxModHalfOrder[32] = {
    0x7F,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0x5D,0x57,0x6E,0x73,0x57,0xA4,0x50,0x1D,
    0xDF,0xE9,0x2F,0x46,0x68,0x1B,0x20,0xA0
};

const unsigned char vchZero[1] = { 0 };

} // namespace

bool CKey::CheckSignatureElement(const unsigned char* vchIn, int len, bool half)
{
    return CompareBigEndian(vchIn, len, vchZero, 0) > 0 &&
           CompareBigEndian(vchIn, len, half ? vchMaxModHalfOrder : vchMaxModOrder, 32) <= 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void CKey::Reset()
{
    OPENSSL_cleanse(vch, sizeof(vch));
    vchPubKey.clear();
    fSet              = false;
    fHavePrivKey      = false;
    fCompressedPubKey = false;
}

CKey::CKey()
{
    std::memset(vch, 0, sizeof(vch));
    vchPubKey.clear();
    fSet              = false;
    fHavePrivKey      = false;
    fCompressedPubKey = false;
}

CKey::CKey(const CKey& b)
{
    *this = b;
}

CKey& CKey::operator=(const CKey& b)
{
    if (this == &b) return *this;
    std::memcpy(vch, b.vch, sizeof(vch));
    vchPubKey         = b.vchPubKey;
    fSet              = b.fSet;
    fHavePrivKey      = b.fHavePrivKey;
    fCompressedPubKey = b.fCompressedPubKey;
    return *this;
}

CKey::~CKey()
{
    OPENSSL_cleanse(vch, sizeof(vch));
}

bool CKey::IsNull() const       { return !fSet; }
bool CKey::IsCompressed() const { return fCompressedPubKey; }

// ─────────────────────────────────────────────────────────────────────────────
// Compression toggle
//
// In the new model the pubkey is always cached at the current compression. If
// we hold the private key we can re-derive trivially; if we only hold a public
// key, callers don't toggle compression in practice in this codebase, so we
// just flip the flag and rely on the next SetPubKey/SetSecret to refresh the
// cache.
// ─────────────────────────────────────────────────────────────────────────────

void CKey::SetCompressedPubKey()
{
    if (fCompressedPubKey) return;
    fCompressedPubKey = true;
    if (fSet && fHavePrivKey) {
        std::size_t len = 33;
        vchPubKey.resize(len);
        if (!ECDSA_pubkey_from_privkey_secp256k1(&vchPubKey[0], &len, vch, /*fCompressed=*/true)) {
            Reset();
            return;
        }
        vchPubKey.resize(len);
    }
}

void CKey::SetUnCompressedPubKey()
{
    if (!fCompressedPubKey && fSet) return;
    fCompressedPubKey = false;
    if (fSet && fHavePrivKey) {
        std::size_t len = 65;
        vchPubKey.resize(len);
        if (!ECDSA_pubkey_from_privkey_secp256k1(&vchPubKey[0], &len, vch, /*fCompressed=*/false)) {
            Reset();
            return;
        }
        vchPubKey.resize(len);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Key generation / load / store
// ─────────────────────────────────────────────────────────────────────────────

void CKey::MakeNewKey(bool fCompressed)
{
    // Sample 32 bytes of entropy and reject any that fall outside (0, n).
    // Probability of needing a retry is ~2^-128.
    do {
        if (RAND_bytes(vch, sizeof(vch)) != 1)
            throw key_error("CKey::MakeNewKey() : RAND_bytes failed");
    } while (!ECDSA_seckey_verify_secp256k1(vch));

    fSet              = true;
    fHavePrivKey      = true;
    fCompressedPubKey = fCompressed;

    std::size_t len = fCompressed ? 33 : 65;
    vchPubKey.resize(len);
    if (!ECDSA_pubkey_from_privkey_secp256k1(&vchPubKey[0], &len, vch, fCompressed)) {
        Reset();
        throw key_error("CKey::MakeNewKey() : failed to derive public key");
    }
    vchPubKey.resize(len);
}

bool CKey::SetPrivKey(const CPrivKey& vchPrivKey)
{
    unsigned char raw[32];
    if (!ECDSA_privkey_import_der_secp256k1(raw, &vchPrivKey[0], vchPrivKey.size())) {
        OPENSSL_cleanse(raw, sizeof(raw));
        Reset();
        return false;
    }

    // Carry the compressed flag out of the DER blob. The two valid sizes
    // produced by ECDSA_privkey_export_der_secp256k1 are 214 (compressed) and
    // 279 (uncompressed); foreign DER blobs are best-effort but those two
    // cover every record this codebase has ever written.
    bool fCompressed = (vchPrivKey.size() == 214);

    CSecret secret(raw, raw + 32);
    OPENSSL_cleanse(raw, sizeof(raw));
    return SetSecret(secret, fCompressed);
}

bool CKey::SetSecret(const CSecret& vchSecret, bool fCompressed)
{
    if (vchSecret.size() != 32)
        throw key_error("CKey::SetSecret() : secret must be 32 bytes");
    if (!ECDSA_seckey_verify_secp256k1(&vchSecret[0]))
        throw key_error("CKey::SetSecret() : secret is not a valid scalar");

    std::memcpy(vch, &vchSecret[0], 32);
    fSet              = true;
    fHavePrivKey      = true;
    // Preserve sticky-compression behaviour from the OpenSSL implementation:
    // if either the explicit argument or the previously-set flag is true,
    // the result is compressed.
    bool fComp = fCompressed || fCompressedPubKey;
    fCompressedPubKey = fComp;

    std::size_t len = fComp ? 33 : 65;
    vchPubKey.resize(len);
    if (!ECDSA_pubkey_from_privkey_secp256k1(&vchPubKey[0], &len, vch, fComp)) {
        Reset();
        return false;
    }
    vchPubKey.resize(len);
    return true;
}

CSecret CKey::GetSecret(bool& fCompressed) const
{
    if (!fSet || !fHavePrivKey)
        throw key_error("CKey::GetSecret() : key is not set or has no private component");
    CSecret out(vch, vch + 32);
    fCompressed = fCompressedPubKey;
    return out;
}

CPrivKey CKey::GetPrivKey() const
{
    if (!fSet || !fHavePrivKey)
        throw key_error("CKey::GetPrivKey() : key is not set or has no private component");

    // Max possible output: 279 bytes (uncompressed).
    CPrivKey out(279, 0);
    std::size_t out_len = out.size();
    if (!ECDSA_privkey_export_der_secp256k1(&out[0], &out_len, vch, fCompressedPubKey))
        throw key_error("CKey::GetPrivKey() : DER export failed");
    out.resize(out_len);
    return out;
}

bool CKey::SetPubKey(const CPubKey& cpub)
{
    const std::vector<unsigned char>& vchPub = cpub.vchPubKey;
    if (vchPub.size() != 33 && vchPub.size() != 65) {
        Reset();
        return false;
    }
    if (!ECDSA_pubkey_verify_secp256k1(&vchPub[0], vchPub.size())) {
        Reset();
        return false;
    }
    vchPubKey         = vchPub;
    fSet              = true;
    fHavePrivKey      = false;
    fCompressedPubKey = (vchPub.size() == 33);
    return true;
}

CPubKey CKey::GetPubKey() const
{
    return CPubKey(vchPubKey);
}

// ─────────────────────────────────────────────────────────────────────────────
// Sign / verify / recover (all delegate to crypto_ecdsa wrappers)
// ─────────────────────────────────────────────────────────────────────────────

bool CKey::Sign(uint256 hash, std::vector<unsigned char>& vchSig)
{
    vchSig.clear();
    if (!fSet || !fHavePrivKey) return false;

    // libsecp256k1's max DER output is 72 bytes; allocate that and shrink.
    vchSig.resize(72);
    std::size_t sig_len = vchSig.size();
    if (!ECDSA_sign_secp256k1(&vchSig[0], &sig_len,
                               reinterpret_cast<const unsigned char*>(&hash),
                               vch))
    {
        vchSig.clear();
        return false;
    }
    vchSig.resize(sig_len);
    return true;
}

// Compact signature (65 bytes): one header byte (encoding recid + compression)
// followed by 32-byte r and 32-byte s.
bool CKey::SignCompact(uint256 hash, std::vector<unsigned char>& vchSig)
{
    vchSig.clear();
    if (!fSet || !fHavePrivKey) return false;

    vchSig.resize(65, 0);
    if (!ECDSA_sign_compact_secp256k1(&vchSig[0],
                                       reinterpret_cast<const unsigned char*>(&hash),
                                       vch,
                                       fCompressedPubKey))
    {
        vchSig.clear();
        return false;
    }
    return true;
}

bool CKey::SetCompactSignature(uint256 hash, const std::vector<unsigned char>& vchSig)
{
    if (vchSig.size() != 65) return false;
    int nV = vchSig[0];
    if (nV < 27 || nV >= 35) return false;

    unsigned char pubkey[65];
    std::size_t pubkey_len = 0;
    if (!ECDSA_recover_compact_secp256k1(pubkey, &pubkey_len,
                                          reinterpret_cast<const unsigned char*>(&hash),
                                          &vchSig[0]))
        return false;

    std::vector<unsigned char> vchPub(pubkey, pubkey + pubkey_len);
    return SetPubKey(CPubKey(vchPub));
}

bool CKey::Verify(uint256 hash, const std::vector<unsigned char>& vchSig)
{
    if (vchSig.empty() || !fSet) return false;

    return ECDSA_verify_secp256k1(
        reinterpret_cast<const unsigned char*>(&hash),
        &vchSig[0], vchSig.size(),
        &vchPubKey[0], vchPubKey.size());
}

bool CKey::VerifyCompact(uint256 hash, const std::vector<unsigned char>& vchSig)
{
    CKey key;
    if (!key.SetCompactSignature(hash, vchSig)) return false;
    return GetPubKey() == key.GetPubKey();
}

bool CKey::IsValid()
{
    if (!fSet) return false;

    if (fHavePrivKey) {
        if (!ECDSA_seckey_verify_secp256k1(vch)) return false;

        // Re-derive the pubkey and check it matches the cache. This is the
        // libsecp256k1 equivalent of OpenSSL's "consistency between priv and
        // pub" check the original implementation performed.
        unsigned char rederived[65];
        std::size_t rederived_len = 0;
        if (!ECDSA_pubkey_from_privkey_secp256k1(rederived, &rederived_len, vch, fCompressedPubKey))
            return false;
        if (rederived_len != vchPubKey.size()) return false;
        return std::memcmp(rederived, &vchPubKey[0], rederived_len) == 0;
    }

    return ECDSA_pubkey_verify_secp256k1(&vchPubKey[0], vchPubKey.size());
}

// ─────────────────────────────────────────────────────────────────────────────
// Startup smoke test for the cryptography backend.
// ─────────────────────────────────────────────────────────────────────────────

bool ECC_InitSanityCheck()
{
    // Verify that libsecp256k1 can validate a trivially-known good secret
    // (the scalar 1) and reject zero. If either of these fails, the linked
    // library is broken and we should refuse to start.
    static const unsigned char one[32]  = {
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1
    };
    static const unsigned char zero[32] = {0};
    if (!ECDSA_seckey_verify_secp256k1(one))   return false;
    if ( ECDSA_seckey_verify_secp256k1(zero))  return false;
    return true;
}
