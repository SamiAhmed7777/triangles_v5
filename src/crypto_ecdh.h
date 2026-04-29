// Copyright (c) 2026 The Triangles developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef TRIANGLES_CRYPTO_ECDH_H
#define TRIANGLES_CRYPTO_ECDH_H

#include <cstddef>

/**
 * Compute the shared secret X coordinate via secp256k1 ECDH.
 *
 * Output matches OpenSSL's ECDH_compute_key(buf, 32, peer_pub, our_priv, NULL)
 * — i.e. the raw X coordinate of the shared point, with no KDF applied. This
 * preserves bit-for-bit compatibility with smessage's existing key derivation
 * (which feeds the X coordinate into SHA-512 itself), so historical encrypted
 * messages remain decryptable after the migration off OpenSSL EC.
 *
 * @param out32       32-byte buffer for the shared X coordinate.
 * @param privkey32   32-byte secret scalar (big-endian).
 * @param pubkey      Peer public key, serialized as either 33 bytes (compressed)
 *                    or 65 bytes (uncompressed).
 * @param pubkey_len  33 or 65; any other length fails immediately.
 * @return true on success, false if the public key is malformed or the
 *         private key is invalid (zero / >= curve order).
 */
bool ECDH_xonly_secp256k1(unsigned char out32[32],
                          const unsigned char privkey32[32],
                          const unsigned char* pubkey,
                          std::size_t pubkey_len);

#endif // TRIANGLES_CRYPTO_ECDH_H
