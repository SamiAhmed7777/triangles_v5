// Copyright (c) 2026 The Triangles developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "crypto_ecdh.h"

#include <cstring>
#include <mutex>

#include <secp256k1.h>
#include <secp256k1_ecdh.h>

namespace {

// One process-wide context is sufficient for ECDH — no signing or verification
// flags needed. Created lazily on first use; libsecp256k1 contexts are
// thread-safe for read-only operations like ECDH.
secp256k1_context* GetECDHContext()
{
    static std::once_flag once;
    static secp256k1_context* ctx = nullptr;
    std::call_once(once, []() {
        ctx = secp256k1_context_create(SECP256K1_CONTEXT_NONE);
    });
    return ctx;
}

// Hash function callback that returns the raw X coordinate of the shared
// point. Mirrors OpenSSL's ECDH_compute_key behaviour when the KDF is nullptr.
int hash_xonly(unsigned char* output,
               const unsigned char* x32,
               const unsigned char* /*y32*/,
               void* /*data*/)
{
    std::memcpy(output, x32, 32);
    return 1;
}

} // namespace

bool ECDH_xonly_secp256k1(unsigned char out32[32],
                          const unsigned char privkey32[32],
                          const unsigned char* pubkey,
                          std::size_t pubkey_len)
{
    if (pubkey_len != 33 && pubkey_len != 65) return false;

    secp256k1_context* ctx = GetECDHContext();
    if (!ctx) return false;

    secp256k1_pubkey pk;
    if (!secp256k1_ec_pubkey_parse(ctx, &pk, pubkey, pubkey_len))
        return false;

    return secp256k1_ecdh(ctx, out32, &pk, privkey32, hash_xonly, nullptr) == 1;
}
