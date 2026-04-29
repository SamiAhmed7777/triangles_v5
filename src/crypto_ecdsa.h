// Copyright (c) 2026 The Triangles developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef TRIANGLES_CRYPTO_ECDSA_H
#define TRIANGLES_CRYPTO_ECDSA_H

#include <cstddef>

/**
 * Verify a DER-encoded secp256k1 ECDSA signature using libsecp256k1.
 *
 * Drop-in replacement for OpenSSL's
 *   ECDSA_verify(0, hash, 32, sig, sig_len, pkey)
 * with one important caveat baked in: the DER input is parsed *laxly*
 * (Bitcoin Core's `lax_der_parsing` algorithm), so historical non-canonical
 * encodings already on chain — extra padding, leading zeros, length-byte
 * quirks that OpenSSL's permissive ASN.1 reader once accepted — continue
 * to verify. Strict-DER-only parsing here would silently fork the chain.
 *
 * High-S signatures are accepted (libsecp256k1's verify behaviour by default).
 * No malleability check is applied; that is policy and lives elsewhere.
 *
 * @param hash32      32-byte message hash to verify against.
 * @param sig         DER-encoded signature bytes.
 * @param sig_len     Length of `sig`.
 * @param pubkey      Serialized public key (33 bytes compressed or 65 uncompressed).
 * @param pubkey_len  33 or 65; any other length fails immediately.
 * @return true iff the signature is valid for (hash32, pubkey).
 */
bool ECDSA_verify_secp256k1(const unsigned char hash32[32],
                            const unsigned char* sig, std::size_t sig_len,
                            const unsigned char* pubkey, std::size_t pubkey_len);

/**
 * Sign `hash32` with `privkey32` and write a DER-encoded signature to `out`.
 *
 * libsecp256k1 uses RFC 6979 deterministic nonces, so signature bytes will
 * differ from OpenSSL's random-nonce output for the same key+hash, but any
 * resulting signature is equally valid. Low-S is enforced automatically.
 *
 * @param out         Output buffer; must be at least `*out_len` bytes.
 *                    libsecp256k1 produces at most 72 bytes of DER.
 * @param out_len     In: capacity of `out`. Out: bytes actually written.
 * @param hash32      32-byte message hash to sign.
 * @param privkey32   32-byte secret scalar.
 * @return true on success.
 */
bool ECDSA_sign_secp256k1(unsigned char* out, std::size_t* out_len,
                          const unsigned char hash32[32],
                          const unsigned char privkey32[32]);

/**
 * Produce a 65-byte recoverable compact signature.
 *
 * Output layout matches the existing wire format:
 *   out[0]      = 27 + recid + (fCompressed ? 4 : 0)
 *   out[1..33)  = R (big-endian, 32 bytes)
 *   out[33..65) = S (big-endian, 32 bytes)
 *
 * @param out65        65-byte output buffer.
 * @param hash32       32-byte message hash to sign.
 * @param privkey32    32-byte secret scalar.
 * @param fCompressed  Whether the matching public key is compressed; affects
 *                     the recid offset in the header byte.
 * @return true on success.
 */
bool ECDSA_sign_compact_secp256k1(unsigned char out65[65],
                                  const unsigned char hash32[32],
                                  const unsigned char privkey32[32],
                                  bool fCompressed);

/**
 * Recover the signing public key from a 65-byte compact signature (as produced
 * by ECDSA_sign_compact_secp256k1) and a message hash.
 *
 * The header byte's "compressed" flag determines whether the recovered key is
 * serialized as 33 bytes (compressed) or 65 bytes (uncompressed).
 *
 * @param pubkey_out      Output buffer; needs at least 65 bytes capacity.
 * @param pubkey_len_out  Receives the actual serialized length (33 or 65).
 * @param hash32          32-byte message hash that was signed.
 * @param sig65           65-byte compact signature.
 * @return true if recovery succeeded.
 */
bool ECDSA_recover_compact_secp256k1(unsigned char* pubkey_out,
                                     std::size_t* pubkey_len_out,
                                     const unsigned char hash32[32],
                                     const unsigned char sig65[65]);

/** Return true iff `privkey32` is a valid secp256k1 secret (in (0, n)). */
bool ECDSA_seckey_verify_secp256k1(const unsigned char privkey32[32]);

/** Return true iff `pubkey/pubkey_len` parses as a valid secp256k1 point. */
bool ECDSA_pubkey_verify_secp256k1(const unsigned char* pubkey, std::size_t pubkey_len);

/**
 * Derive the public key for `privkey32` and serialize it.
 * @param out          Output buffer; must be at least 65 bytes.
 * @param out_len_out  Receives the actual length (33 or 65).
 * @param privkey32    32-byte secret scalar.
 * @param fCompressed  Whether to serialize compressed (33B) or uncompressed (65B).
 * @return true on success.
 */
bool ECDSA_pubkey_from_privkey_secp256k1(unsigned char* out, std::size_t* out_len_out,
                                         const unsigned char privkey32[32],
                                         bool fCompressed);

/**
 * SEC1/RFC-5915 DER ECPrivateKey encoder/decoder for the secp256k1 curve.
 * Output bytes match the layout produced by OpenSSL's i2d_ECPrivateKey on this
 * curve (compressed = 214 bytes, uncompressed = 279 bytes), so wallet.dat
 * records written by previous OpenSSL-EC builds remain readable, and records
 * we write remain readable by older OpenSSL-based builds.
 */
bool ECDSA_privkey_export_der_secp256k1(unsigned char* out, std::size_t* out_len_out,
                                        const unsigned char privkey32[32],
                                        bool fCompressed);
bool ECDSA_privkey_import_der_secp256k1(unsigned char privkey32_out[32],
                                        const unsigned char* der, std::size_t der_len);

#endif // TRIANGLES_CRYPTO_ECDSA_H
