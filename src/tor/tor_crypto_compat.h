// Copyright (c) 2024 Triangles developers
// Tor Crypto Compatibility Layer for LibreSSL 3.x
// Distributed under the MIT/X11 software license

#ifndef TRIANGLES_TOR_CRYPTO_COMPAT_H
#define TRIANGLES_TOR_CRYPTO_COMPAT_H

#include <openssl/opensslv.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/ec.h>
#include <openssl/ecdh.h>
#include <openssl/engine.h>
#include <openssl/sha.h>
#include <string.h>
#include <stdlib.h>

// LibreSSL 3.x compatibility macros and functions

// Version detection for LibreSSL 3.x
#ifdef LIBRESSL_VERSION_NUMBER
#if LIBRESSL_VERSION_NUMBER >= 0x3000000fL
#define USING_LIBRESSL_3X
#endif
#endif

// ECDH compatibility for LibreSSL 3.x
#if defined(USING_LIBRESSL) || defined(USING_LIBRESSL_3X)

// LibreSSL 3.x removed ECDH_OpenSSL and ECDH_set_method
// We need to use EVP_PKEY_derive instead

static inline int ECDH_compute_key_compat(void *out, size_t outlen, 
                                          const EC_POINT *pub_key, 
                                          EC_KEY *ecdh, 
                                          void *(*KDF)(const void *in, size_t inlen, void *out, size_t *outlen))
{
    if (!out || !pub_key || !ecdh || outlen == 0) {
        return -1;
    }
    
    // Convert EC_KEY to EVP_PKEY for LibreSSL 3.x
    EVP_PKEY *pkey = EVP_PKEY_new();
    if (!pkey) {
        return -1;
    }
    
    if (EVP_PKEY_set1_EC_KEY(pkey, ecdh) != 1) {
        EVP_PKEY_free(pkey);
        return -1;
    }
    
    // Create peer public key
    EC_KEY *peer_key = EC_KEY_new();
    if (!peer_key) {
        EVP_PKEY_free(pkey);
        return -1;
    }
    
    const EC_GROUP *group = EC_KEY_get0_group(ecdh);
    if (!group || EC_KEY_set_group(peer_key, group) != 1) {
        EC_KEY_free(peer_key);
        EVP_PKEY_free(pkey);
        return -1;
    }
    
    if (EC_KEY_set_public_key(peer_key, pub_key) != 1) {
        EC_KEY_free(peer_key);
        EVP_PKEY_free(pkey);
        return -1;
    }
    
    EVP_PKEY *peer_pkey = EVP_PKEY_new();
    if (!peer_pkey) {
        EC_KEY_free(peer_key);
        EVP_PKEY_free(pkey);
        return -1;
    }
    
    if (EVP_PKEY_set1_EC_KEY(peer_pkey, peer_key) != 1) {
        EVP_PKEY_free(peer_pkey);
        EC_KEY_free(peer_key);
        EVP_PKEY_free(pkey);
        return -1;
    }
    
    // Perform key derivation using EVP_PKEY_derive
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new(pkey, NULL);
    if (!ctx) {
        EVP_PKEY_free(peer_pkey);
        EC_KEY_free(peer_key);
        EVP_PKEY_free(pkey);
        return -1;
    }
    
    if (EVP_PKEY_derive_init(ctx) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        EVP_PKEY_free(peer_pkey);
        EC_KEY_free(peer_key);
        EVP_PKEY_free(pkey);
        return -1;
    }
    
    if (EVP_PKEY_derive_set_peer(ctx, peer_pkey) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        EVP_PKEY_free(peer_pkey);
        EC_KEY_free(peer_key);
        EVP_PKEY_free(pkey);
        return -1;
    }
    
    size_t keylen = outlen;
    int ret = EVP_PKEY_derive(ctx, (unsigned char*)out, &keylen);
    
    // Apply KDF if provided
    if (ret > 0 && KDF) {
        size_t kdf_outlen = outlen;
        void *kdf_result = KDF(out, keylen, out, &kdf_outlen);
        if (!kdf_result) {
            ret = -1;
        } else {
            keylen = kdf_outlen;
        }
    }
    
    // Cleanup
    EVP_PKEY_CTX_free(ctx);
    EVP_PKEY_free(peer_pkey);
    EC_KEY_free(peer_key);
    EVP_PKEY_free(pkey);
    
    return (ret <= 0) ? -1 : (int)keylen;
}

// Enhanced ECDH compatibility wrapper with error checking
static inline int ECDH_compute_key_safe(void *out, size_t outlen,
                                        const EC_POINT *pub_key,
                                        EC_KEY *ecdh,
                                        void *(*KDF)(const void *, size_t, void *, size_t *))
{
    // Additional validation for LibreSSL 3.x
    if (!EC_KEY_check_key(ecdh)) {
        return -1;
    }
    
    const EC_GROUP *group = EC_KEY_get0_group(ecdh);
    if (!group) {
        return -1;
    }
    
    if (!EC_POINT_is_on_curve(group, pub_key, NULL)) {
        return -1;
    }
    
    return ECDH_compute_key_compat(out, outlen, pub_key, ecdh, KDF);
}

// Compatibility macros
#define ECDH_compute_key(out, outlen, pub_key, ecdh, KDF) \
    ECDH_compute_key_compat(out, outlen, pub_key, ecdh, KDF)

// ECDH_set_method is no longer needed in LibreSSL 3.x
#define ECDH_set_method(ecdh, method) (1)

// ECDH_OpenSSL is no longer available
#define ECDH_OpenSSL() (NULL)

// Additional ECDH method compatibility
static inline const void* ECDH_get_default_method(void) { return NULL; }
static inline int ECDH_set_default_method(const void *method) { return 1; }

#endif // USING_LIBRESSL || USING_LIBRESSL_3X

// RAND compatibility
#ifdef USING_LIBRESSL
// RAND_SSLeay was removed in LibreSSL 3.x
#ifndef RAND_SSLeay
#define RAND_SSLeay RAND_OpenSSL
#endif
#endif

// ENGINE compatibility stubs for LibreSSL 3.x
#if defined(USING_LIBRESSL) || defined(USING_LIBRESSL_3X)

// ENGINE functions are provided by LibreSSL 4.0 as deprecated stubs
// However we still need ENGINE_get_default_ECDH which is missing
static inline void* ENGINE_get_default_ECDH(void) {
    return NULL;
}

// DH accessor functions are provided by LibreSSL 4.0
// No need to redefine them

#endif // USING_LIBRESSL || USING_LIBRESSL_3X

// Base64 encoding compatibility
#if defined(USING_LIBRESSL) || defined(USING_LIBRESSL_3X)

// EVP_ENCODE_CTX is opaque in LibreSSL 3.x
static inline int base64_encode_compat(const unsigned char *in, int inlen, char *out)
{
    if (!in || !out || inlen < 0) {
        return -1;
    }
    
    EVP_ENCODE_CTX *ctx = EVP_ENCODE_CTX_new();
    if (!ctx) {
        return -1;
    }
    
    EVP_EncodeInit(ctx);
    
    int outlen = 0;
    int tmplen = 0;
    
    EVP_EncodeUpdate(ctx, (unsigned char*)out, &tmplen, in, inlen);
    outlen += tmplen;
    
    EVP_EncodeFinal(ctx, (unsigned char*)out + outlen, &tmplen);
    outlen += tmplen;
    
    EVP_ENCODE_CTX_free(ctx);
    
    // Null-terminate the output
    out[outlen] = '\0';
    
    return outlen;
}

static inline int base64_decode_compat(const char *in, int inlen, unsigned char *out)
{
    if (!in || !out || inlen < 0) {
        return -1;
    }
    
    EVP_ENCODE_CTX *ctx = EVP_ENCODE_CTX_new();
    if (!ctx) {
        return -1;
    }
    
    EVP_DecodeInit(ctx);
    
    int outlen = 0;
    int tmplen = 0;
    
    if (EVP_DecodeUpdate(ctx, out, &tmplen, (const unsigned char*)in, inlen) == -1) {
        EVP_ENCODE_CTX_free(ctx);
        return -1;
    }
    outlen += tmplen;
    
    if (EVP_DecodeFinal(ctx, out + outlen, &tmplen) == -1) {
        EVP_ENCODE_CTX_free(ctx);
        return -1;
    }
    outlen += tmplen;
    
    EVP_ENCODE_CTX_free(ctx);
    return outlen;
}

// Enhanced base64 functions with size calculation
static inline int base64_encode_len(int inlen)
{
    // Calculate required buffer size for base64 encoding
    // Formula: ((inlen + 2) / 3) * 4 + 1 (for null terminator)
    return ((inlen + 2) / 3) * 4 + 1;
}

static inline int base64_decode_len(const char *in, int inlen)
{
    if (!in || inlen < 0) {
        return -1;
    }
    
    // Estimate decoded length (may be slightly larger than actual)
    int padding = 0;
    if (inlen >= 2) {
        if (in[inlen - 1] == '=') padding++;
        if (in[inlen - 2] == '=') padding++;
    }
    
    return (inlen * 3) / 4 - padding;
}

#endif // USING_LIBRESSL || USING_LIBRESSL_3X

// Error handling compatibility
#if defined(USING_LIBRESSL) || defined(USING_LIBRESSL_3X)

// ERR_remove_state is deprecated
#define ERR_remove_state(pid) ERR_remove_thread_state(NULL)

// Additional error handling compatibility
static inline void ERR_remove_thread_state_compat(const void *tid) {
    (void)tid; // Suppress unused parameter warning
    ERR_remove_thread_state(NULL);
}

#define ERR_remove_thread_state(tid) ERR_remove_thread_state_compat(tid)

#endif // USING_LIBRESSL || USING_LIBRESSL_3X

// RAND compatibility
#if defined(USING_LIBRESSL) || defined(USING_LIBRESSL_3X)

// RAND_SSLeay was removed in LibreSSL 3.x
#ifndef RAND_SSLeay
#define RAND_SSLeay RAND_OpenSSL
#endif

// Additional RAND compatibility functions
static inline int RAND_set_rand_method_compat(const void *meth) {
    (void)meth; // Suppress unused parameter warning
    return 1; // Always succeed in LibreSSL 3.x
}

#define RAND_set_rand_method(meth) RAND_set_rand_method_compat(meth)

static inline const void* RAND_get_rand_method_compat(void) {
    return NULL; // LibreSSL 3.x doesn't expose method structures
}

#define RAND_get_rand_method() RAND_get_rand_method_compat()

#endif // USING_LIBRESSL || USING_LIBRESSL_3X

// SHA3-256 compatibility for Tor v3 operations (needed for all OpenSSL versions)

#include <openssl/sha.h>

// SHA3-256 wrapper for onion address checksum calculation
static inline int SHA3_256_compat(const unsigned char *data, size_t len, unsigned char *md) {
    if (!data || !md) {
        return 0;
    }

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) {
        return 0;
    }

    const EVP_MD *sha3_256 = EVP_sha3_256();
    if (!sha3_256) {
        EVP_MD_CTX_free(ctx);
        return 0;
    }

    if (EVP_DigestInit_ex(ctx, sha3_256, NULL) != 1) {
        EVP_MD_CTX_free(ctx);
        return 0;
    }

    if (EVP_DigestUpdate(ctx, data, len) != 1) {
        EVP_MD_CTX_free(ctx);
        return 0;
    }

    unsigned int md_len;
    if (EVP_DigestFinal_ex(ctx, md, &md_len) != 1) {
        EVP_MD_CTX_free(ctx);
        return 0;
    }

    EVP_MD_CTX_free(ctx);
    return (md_len == 32) ? 1 : 0; // SHA3-256 should produce 32 bytes
}

#ifndef SHA3_256
#define SHA3_256(data, len, md) SHA3_256_compat(data, len, md)
#endif

// Ed25519 compatibility for Tor v3 key generation
#if defined(USING_LIBRESSL) || defined(USING_LIBRESSL_3X)

// Ed25519 key generation wrapper
static inline EVP_PKEY* generate_ed25519_keypair_compat(void) {
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, NULL);
    if (!ctx) {
        return NULL;
    }
    
    if (EVP_PKEY_keygen_init(ctx) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }
    
    EVP_PKEY *pkey = NULL;
    if (EVP_PKEY_keygen(ctx, &pkey) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }
    
    EVP_PKEY_CTX_free(ctx);
    return pkey;
}

// Ed25519 raw key extraction
static inline int extract_ed25519_keys_compat(EVP_PKEY *pkey, 
                                             unsigned char *private_key, size_t *private_len,
                                             unsigned char *public_key, size_t *public_len) {
    if (!pkey || !private_key || !public_key || !private_len || !public_len) {
        return 0;
    }
    
    // Extract private key
    if (EVP_PKEY_get_raw_private_key(pkey, private_key, private_len) != 1) {
        return 0;
    }
    
    // Extract public key
    if (EVP_PKEY_get_raw_public_key(pkey, public_key, public_len) != 1) {
        return 0;
    }
    
    return 1;
}

#endif // USING_LIBRESSL || USING_LIBRESSL_3X

// Memory management compatibility
#if defined(USING_LIBRESSL) || defined(USING_LIBRESSL_3X)

// Secure memory clearing
static inline void OPENSSL_cleanse_compat(void *ptr, size_t len) {
    if (ptr && len > 0) {
        OPENSSL_cleanse(ptr, len);
    }
}

#define OPENSSL_cleanse(ptr, len) OPENSSL_cleanse_compat(ptr, len)

#endif // USING_LIBRESSL || USING_LIBRESSL_3X

#endif // TRIANGLES_TOR_CRYPTO_COMPAT_H