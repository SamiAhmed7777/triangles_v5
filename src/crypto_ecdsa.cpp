// Copyright (c) 2026 The Triangles developers
// Copyright (c) 2015 Pieter Wuille (lax DER parser, MIT licence)
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "crypto_ecdsa.h"

#include <cstring>
#include <mutex>

#include <secp256k1.h>
#include <secp256k1_recovery.h>

namespace {

// Combined VERIFY + SIGN context. libsecp256k1 contexts are thread-safe for
// signing and verification once created. In libsecp256k1 >= 0.2 these flags
// are accepted but increasingly no-ops; passing both keeps us compatible with
// older versions still in distro packages.
secp256k1_context* GetEcdsaContext()
{
    static std::once_flag once;
    static secp256k1_context* ctx = nullptr;
    std::call_once(once, []() {
        ctx = secp256k1_context_create(SECP256K1_CONTEXT_VERIFY | SECP256K1_CONTEXT_SIGN);
    });
    return ctx;
}

// ─────────────────────────────────────────────────────────────────────────────
// Lax DER parser, vendored from Bitcoin Core (contrib/lax_der_parsing.c).
//
// libsecp256k1's strict parser rejects DER encodings that OpenSSL has
// historically accepted: non-minimal length bytes, extra leading zeros on R/S,
// negative integers, etc. Many such signatures already exist on chain. This
// parser tolerates them, normalises (R, S) into a 64-byte compact buffer, and
// hands that to libsecp256k1's compact-signature parser. Anything that still
// fails to fit (e.g. R or S exceeding 32 bytes after stripping leading zeros)
// is treated as zero so the verify call returns a clean failure rather than
// crashing.
// ─────────────────────────────────────────────────────────────────────────────
int ecdsa_signature_parse_der_lax(const secp256k1_context* ctx,
                                  secp256k1_ecdsa_signature* sig,
                                  const unsigned char* input,
                                  std::size_t inputlen)
{
    std::size_t rpos, rlen, spos, slen;
    std::size_t pos = 0;
    std::size_t lenbyte;
    unsigned char tmpsig[64] = {0};
    int overflow = 0;

    // Initialise sig with a parseable but invalid signature so the caller
    // always gets a defined value back even on early-exit paths.
    secp256k1_ecdsa_signature_parse_compact(ctx, sig, tmpsig);

    // SEQUENCE tag.
    if (pos == inputlen || input[pos] != 0x30) return 0;
    pos++;

    // SEQUENCE length (skipped — we trust the inner element lengths).
    if (pos == inputlen) return 0;
    lenbyte = input[pos++];
    if (lenbyte & 0x80) {
        lenbyte -= 0x80;
        if (lenbyte > inputlen - pos) return 0;
        pos += lenbyte;
    }

    // R: INTEGER tag.
    if (pos == inputlen || input[pos] != 0x02) return 0;
    pos++;

    // R: length.
    if (pos == inputlen) return 0;
    lenbyte = input[pos++];
    if (lenbyte & 0x80) {
        lenbyte -= 0x80;
        if (lenbyte > inputlen - pos) return 0;
        while (lenbyte > 0 && input[pos] == 0) { pos++; lenbyte--; }
        if (lenbyte >= sizeof(std::size_t)) return 0;
        rlen = 0;
        while (lenbyte > 0) { rlen = (rlen << 8) + input[pos]; pos++; lenbyte--; }
    } else {
        rlen = lenbyte;
    }
    if (rlen > inputlen - pos) return 0;
    rpos = pos;
    pos += rlen;

    // S: INTEGER tag.
    if (pos == inputlen || input[pos] != 0x02) return 0;
    pos++;

    // S: length.
    if (pos == inputlen) return 0;
    lenbyte = input[pos++];
    if (lenbyte & 0x80) {
        lenbyte -= 0x80;
        if (lenbyte > inputlen - pos) return 0;
        while (lenbyte > 0 && input[pos] == 0) { pos++; lenbyte--; }
        if (lenbyte >= sizeof(std::size_t)) return 0;
        slen = 0;
        while (lenbyte > 0) { slen = (slen << 8) + input[pos]; pos++; lenbyte--; }
    } else {
        slen = lenbyte;
    }
    if (slen > inputlen - pos) return 0;
    spos = pos;

    // Strip leading zeros from R and place right-aligned in tmpsig[0..32).
    while (rlen > 0 && input[rpos] == 0) { rlen--; rpos++; }
    if (rlen > 32) {
        overflow = 1;
    } else {
        std::memcpy(tmpsig + 32 - rlen, input + rpos, rlen);
    }

    // Strip leading zeros from S and place right-aligned in tmpsig[32..64).
    while (slen > 0 && input[spos] == 0) { slen--; spos++; }
    if (slen > 32) {
        overflow = 1;
    } else {
        std::memcpy(tmpsig + 64 - slen, input + spos, slen);
    }

    if (!overflow) {
        overflow = !secp256k1_ecdsa_signature_parse_compact(ctx, sig, tmpsig);
    }
    if (overflow) {
        std::memset(tmpsig, 0, 64);
        secp256k1_ecdsa_signature_parse_compact(ctx, sig, tmpsig);
    }
    return 1;
}

} // namespace

bool ECDSA_verify_secp256k1(const unsigned char hash32[32],
                            const unsigned char* sig, std::size_t sig_len,
                            const unsigned char* pubkey, std::size_t pubkey_len)
{
    if (sig_len == 0)                           return false;
    if (pubkey_len != 33 && pubkey_len != 65)   return false;

    secp256k1_context* ctx = GetEcdsaContext();
    if (!ctx) return false;

    secp256k1_pubkey pk;
    if (!secp256k1_ec_pubkey_parse(ctx, &pk, pubkey, pubkey_len))
        return false;

    secp256k1_ecdsa_signature parsed_sig;
    if (!ecdsa_signature_parse_der_lax(ctx, &parsed_sig, sig, sig_len))
        return false;

    return secp256k1_ecdsa_verify(ctx, &parsed_sig, hash32, &pk) == 1;
}

bool ECDSA_sign_secp256k1(unsigned char* out, std::size_t* out_len,
                          const unsigned char hash32[32],
                          const unsigned char privkey32[32])
{
    if (!out || !out_len) return false;
    secp256k1_context* ctx = GetEcdsaContext();
    if (!ctx) return false;

    secp256k1_ecdsa_signature sig;
    if (!secp256k1_ecdsa_sign(ctx, &sig, hash32, privkey32, nullptr, nullptr))
        return false;

    return secp256k1_ecdsa_signature_serialize_der(ctx, out, out_len, &sig) == 1;
}

bool ECDSA_sign_compact_secp256k1(unsigned char out65[65],
                                  const unsigned char hash32[32],
                                  const unsigned char privkey32[32],
                                  bool fCompressed)
{
    secp256k1_context* ctx = GetEcdsaContext();
    if (!ctx) return false;

    secp256k1_ecdsa_recoverable_signature recsig;
    if (!secp256k1_ecdsa_sign_recoverable(ctx, &recsig, hash32, privkey32, nullptr, nullptr))
        return false;

    int recid = -1;
    if (!secp256k1_ecdsa_recoverable_signature_serialize_compact(ctx, &out65[1], &recid, &recsig))
        return false;
    if (recid < 0 || recid > 3) return false;

    out65[0] = static_cast<unsigned char>(27 + recid + (fCompressed ? 4 : 0));
    return true;
}

bool ECDSA_recover_compact_secp256k1(unsigned char* pubkey_out,
                                     std::size_t* pubkey_len_out,
                                     const unsigned char hash32[32],
                                     const unsigned char sig65[65])
{
    if (!pubkey_out || !pubkey_len_out) return false;

    int header = sig65[0];
    if (header < 27 || header >= 35) return false;
    bool fCompressed = (header >= 31);
    int recid = (header - 27) & 0x3;

    secp256k1_context* ctx = GetEcdsaContext();
    if (!ctx) return false;

    secp256k1_ecdsa_recoverable_signature recsig;
    if (!secp256k1_ecdsa_recoverable_signature_parse_compact(ctx, &recsig, &sig65[1], recid))
        return false;

    secp256k1_pubkey pk;
    if (!secp256k1_ecdsa_recover(ctx, &pk, &recsig, hash32))
        return false;

    std::size_t out_len = fCompressed ? 33 : 65;
    if (!secp256k1_ec_pubkey_serialize(ctx, pubkey_out, &out_len, &pk,
                                        fCompressed ? SECP256K1_EC_COMPRESSED
                                                    : SECP256K1_EC_UNCOMPRESSED))
        return false;

    *pubkey_len_out = out_len;
    return true;
}

bool ECDSA_seckey_verify_secp256k1(const unsigned char privkey32[32])
{
    secp256k1_context* ctx = GetEcdsaContext();
    if (!ctx) return false;
    return secp256k1_ec_seckey_verify(ctx, privkey32) == 1;
}

bool ECDSA_pubkey_verify_secp256k1(const unsigned char* pubkey, std::size_t pubkey_len)
{
    if (pubkey_len != 33 && pubkey_len != 65) return false;
    secp256k1_context* ctx = GetEcdsaContext();
    if (!ctx) return false;
    secp256k1_pubkey pk;
    return secp256k1_ec_pubkey_parse(ctx, &pk, pubkey, pubkey_len) == 1;
}

bool ECDSA_pubkey_from_privkey_secp256k1(unsigned char* out, std::size_t* out_len_out,
                                         const unsigned char privkey32[32],
                                         bool fCompressed)
{
    if (!out || !out_len_out) return false;
    secp256k1_context* ctx = GetEcdsaContext();
    if (!ctx) return false;

    secp256k1_pubkey pk;
    if (!secp256k1_ec_pubkey_create(ctx, &pk, privkey32))
        return false;

    std::size_t len = fCompressed ? 33 : 65;
    if (!secp256k1_ec_pubkey_serialize(ctx, out, &len, &pk,
                                        fCompressed ? SECP256K1_EC_COMPRESSED
                                                    : SECP256K1_EC_UNCOMPRESSED))
        return false;
    *out_len_out = len;
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// SEC1 / RFC-5915 DER codec for secp256k1 ECPrivateKey
//
// Vendored from Bitcoin Core (src/key.cpp), MIT-licensed. The decoder is lax
// about details (matches OpenSSL's d2i_ECPrivateKey lenience); the encoder
// writes the exact byte layout that OpenSSL's i2d_ECPrivateKey produces for
// this curve so wallet.dat records remain interchangeable across versions.
//
//   Compressed pubkey:   214 bytes
//   Uncompressed pubkey: 279 bytes
//
// The static templates below carry every byte except the 32-byte private
// scalar and the public key bytes, which are spliced into the precomputed
// offsets at encode time.
// ─────────────────────────────────────────────────────────────────────────────

namespace {

const unsigned char der_template_compressed[214] = {
    0x30,0x81,0xD3,0x02,0x01,0x01,0x04,0x20,
    /* private key (32 bytes) at offset 8 */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0xA0,0x81,0x85,0x30,0x81,0x82,0x02,0x01,0x01,0x30,0x2C,0x06,0x07,0x2A,0x86,0x48,
    0xCE,0x3D,0x01,0x01,0x02,0x21,0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFE,0xFF,0xFF,0xFC,0x2F,0x30,0x06,0x04,0x01,0x00,
    0x04,0x01,0x07,0x04,0x21,0x02,0x79,0xBE,0x66,0x7E,0xF9,0xDC,0xBB,0xAC,0x55,0xA0,
    0x62,0x95,0xCE,0x87,0x0B,0x07,0x02,0x9B,0xFC,0xDB,0x2D,0xCE,0x28,0xD9,0x59,0xF2,
    0x81,0x5B,0x16,0xF8,0x17,0x98,0x02,0x21,0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFE,0xBA,0xAE,0xDC,0xE6,0xAF,0x48,0xA0,
    0x3B,0xBF,0xD2,0x5E,0x8C,0xD0,0x36,0x41,0x41,0x02,0x01,0x01,0xA1,0x24,0x03,0x22,
    0x00,
    /* compressed pubkey (33 bytes) at offset 181 */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

const unsigned char der_template_uncompressed[279] = {
    0x30,0x82,0x01,0x13,0x02,0x01,0x01,0x04,0x20,
    /* private key (32 bytes) at offset 9 */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0xA0,0x81,0xA5,0x30,0x81,0xA2,0x02,0x01,0x01,0x30,0x2C,0x06,0x07,0x2A,0x86,0x48,
    0xCE,0x3D,0x01,0x01,0x02,0x21,0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFE,0xFF,0xFF,0xFC,0x2F,0x30,0x06,0x04,0x01,0x00,
    0x04,0x01,0x07,0x04,0x41,0x04,0x79,0xBE,0x66,0x7E,0xF9,0xDC,0xBB,0xAC,0x55,0xA0,
    0x62,0x95,0xCE,0x87,0x0B,0x07,0x02,0x9B,0xFC,0xDB,0x2D,0xCE,0x28,0xD9,0x59,0xF2,
    0x81,0x5B,0x16,0xF8,0x17,0x98,0x48,0x3A,0xDA,0x77,0x26,0xA3,0xC4,0x65,0x5D,0xA4,
    0xFB,0xFC,0x0E,0x11,0x08,0xA8,0xFD,0x17,0xB4,0x48,0xA6,0x85,0x54,0x19,0x9C,0x47,
    0xD0,0x8F,0xFB,0x10,0xD4,0xB8,0x02,0x21,0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFE,0xBA,0xAE,0xDC,0xE6,0xAF,0x48,0xA0,
    0x3B,0xBF,0xD2,0x5E,0x8C,0xD0,0x36,0x41,0x41,0x02,0x01,0x01,0xA1,0x44,0x03,0x42,
    0x00,
    /* uncompressed pubkey (65 bytes) at offset 214 */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0
};

} // namespace

bool ECDSA_privkey_export_der_secp256k1(unsigned char* out, std::size_t* out_len_out,
                                        const unsigned char privkey32[32],
                                        bool fCompressed)
{
    if (!out || !out_len_out) return false;

    secp256k1_context* ctx = GetEcdsaContext();
    if (!ctx) return false;

    secp256k1_pubkey pk;
    if (!secp256k1_ec_pubkey_create(ctx, &pk, privkey32))
        return false;

    if (fCompressed) {
        std::memcpy(out, der_template_compressed, sizeof(der_template_compressed));
        std::memcpy(out + 8, privkey32, 32);
        std::size_t pub_len = 33;
        if (!secp256k1_ec_pubkey_serialize(ctx, out + 181, &pub_len, &pk, SECP256K1_EC_COMPRESSED))
            return false;
        *out_len_out = sizeof(der_template_compressed);
    } else {
        std::memcpy(out, der_template_uncompressed, sizeof(der_template_uncompressed));
        std::memcpy(out + 9, privkey32, 32);
        std::size_t pub_len = 65;
        if (!secp256k1_ec_pubkey_serialize(ctx, out + 214, &pub_len, &pk, SECP256K1_EC_UNCOMPRESSED))
            return false;
        *out_len_out = sizeof(der_template_uncompressed);
    }
    return true;
}

bool ECDSA_privkey_import_der_secp256k1(unsigned char privkey32_out[32],
                                        const unsigned char* der, std::size_t der_len)
{
    // Lax SEC1/RFC-5915 ECPrivateKey parser. We only need to find the OCTET
    // STRING containing the private key scalar; everything else (curve params,
    // optional public key) is informational. Mirrors Bitcoin Core's
    // ec_privkey_import_der.
    const unsigned char* end = der + der_len;
    if (end < der + 1 || *(der++) != 0x30) return false;

    // Outer SEQUENCE length — variable length encoding.
    if (der >= end) return false;
    int lenb = *(der++);
    if (lenb < 0x80) {
        // short form, ignore
    } else {
        int n = lenb & 0x7F;
        if (n == 0 || n > 2) return false;
        if (der + n > end) return false;
        der += n;
    }

    // Version INTEGER (1).
    if (der + 3 > end || der[0] != 0x02 || der[1] != 0x01 || der[2] != 0x01) return false;
    der += 3;

    // privateKey OCTET STRING (length 32).
    if (der + 2 > end || der[0] != 0x04 || der[1] != 0x20) return false;
    der += 2;
    if (der + 32 > end) return false;
    std::memcpy(privkey32_out, der, 32);

    // Validate the result against the curve order; reject zero / >= n.
    return ECDSA_seckey_verify_secp256k1(privkey32_out);
}
