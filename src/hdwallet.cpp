// Copyright (c) 2026 The Triangles developers
// Distributed under the MIT/X11 software license.
#include "hdwallet.h"
#include "bip39_english.h"

#include <cstring>
#include <algorithm>

#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#include <secp256k1.h>

namespace hd {

// ---- secp256k1 context (self-contained; independent of crypto_ecdsa) ------
static secp256k1_context* HDContext()
{
    static secp256k1_context* ctx = NULL;
    if (!ctx)
        ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);
    return ctx;
}

static void HmacSha512(const unsigned char* key, size_t keylen,
                       const unsigned char* data, size_t datalen,
                       unsigned char out[64])
{
    unsigned int len = 64;
    HMAC(EVP_sha512(), key, (int)keylen, data, datalen, out, &len);
}

// Binary search the (lexicographically sorted) BIP39 English wordlist.
static int WordIndex(const std::string& w)
{
    int lo = 0, hi = 2047;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        int c = w.compare(BIP39_WORDLIST_EN[mid]);
        if (c == 0) return mid;
        if (c < 0) hi = mid - 1; else lo = mid + 1;
    }
    return -1;
}

static std::vector<std::string> SplitWords(const std::string& s)
{
    std::vector<std::string> out;
    size_t i = 0, n = s.size();
    while (i < n) {
        while (i < n && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) i++;
        size_t j = i;
        while (j < n && !(s[j] == ' ' || s[j] == '\t' || s[j] == '\n' || s[j] == '\r')) j++;
        if (j > i) out.push_back(s.substr(i, j - i));
        i = j;
    }
    return out;
}

// ---- BIP39 ----------------------------------------------------------------
std::string GenerateMnemonic(int strengthBits)
{
    if (strengthBits != 128 && strengthBits != 256) strengthBits = 256;
    int entBytes = strengthBits / 8;
    std::vector<unsigned char> ent(entBytes);
    if (RAND_bytes(&ent[0], entBytes) != 1) return std::string();

    // checksum = first (ENT/32) bits of SHA256(entropy)
    unsigned char hash[32];
    SHA256(&ent[0], entBytes, hash);
    int csBits = strengthBits / 32;

    // bit buffer = entropy || checksum bits
    std::vector<unsigned char> bits = ent;
    bits.push_back(hash[0]); // up to 8 checksum bits live in hash[0]

    int totalBits = strengthBits + csBits;
    int words = totalBits / 11;
    std::string out;
    for (int i = 0; i < words; i++) {
        int idx = 0;
        for (int b = 0; b < 11; b++) {
            int bitpos = i * 11 + b;
            int byte = bitpos / 8, off = 7 - (bitpos % 8);
            int bit = (bits[byte] >> off) & 1;
            idx = (idx << 1) | bit;
        }
        if (i) out += ' ';
        out += BIP39_WORDLIST_EN[idx];
    }
    return out;
}

bool CheckMnemonic(const std::string& mnemonic)
{
    std::vector<std::string> w = SplitWords(mnemonic);
    size_t nw = w.size();
    if (nw != 12 && nw != 15 && nw != 18 && nw != 21 && nw != 24) return false;

    int totalBits = (int)nw * 11;
    int csBits = totalBits / 33;
    int entBits = totalBits - csBits;
    if (entBits % 8 != 0) return false;
    int entBytes = entBits / 8;

    // unpack 11-bit indices into a bit buffer
    std::vector<unsigned char> buf((totalBits + 7) / 8, 0);
    for (size_t i = 0; i < nw; i++) {
        int idx = WordIndex(w[i]);
        if (idx < 0) return false;
        for (int b = 0; b < 11; b++) {
            int bit = (idx >> (10 - b)) & 1;
            int bitpos = (int)i * 11 + b;
            int byte = bitpos / 8, off = 7 - (bitpos % 8);
            if (bit) buf[byte] |= (1 << off);
        }
    }
    std::vector<unsigned char> ent(buf.begin(), buf.begin() + entBytes);
    unsigned char hash[32];
    SHA256(&ent[0], entBytes, hash);
    // compare csBits checksum bits
    for (int b = 0; b < csBits; b++) {
        int bitpos = entBits + b;
        int byte = bitpos / 8, off = 7 - (bitpos % 8);
        int got = (buf[byte] >> off) & 1;
        int want = (hash[b / 8] >> (7 - (b % 8))) & 1;
        if (got != want) return false;
    }
    return true;
}

bool MnemonicToSeed(const std::string& mnemonic, const std::string& passphrase,
                    unsigned char seed64[64])
{
    std::string salt = "mnemonic" + passphrase;
    int rc = PKCS5_PBKDF2_HMAC(mnemonic.c_str(), (int)mnemonic.size(),
                               (const unsigned char*)salt.c_str(), (int)salt.size(),
                               2048, EVP_sha512(), 64, seed64);
    return rc == 1;
}

// ---- BIP32 ----------------------------------------------------------------
bool MasterFromSeed(const unsigned char* seed, size_t seedlen, ExtKey& out)
{
    unsigned char I[64];
    HmacSha512((const unsigned char*)"Bitcoin seed", 12, seed, seedlen, I);
    memcpy(out.key, I, 32);
    memcpy(out.chaincode, I + 32, 32);
    if (!secp256k1_ec_seckey_verify(HDContext(), out.key)) return false;
    out.valid = true;
    return true;
}

bool CKDpriv(const ExtKey& parent, uint32_t index, ExtKey& child)
{
    if (!parent.valid) return false;
    secp256k1_context* ctx = HDContext();
    unsigned char data[37];
    size_t dlen = 0;
    if (index & HARDENED) {
        data[0] = 0x00;
        memcpy(data + 1, parent.key, 32);
        dlen = 33;
    } else {
        // serP(point(parent.key)) = 33-byte compressed pubkey
        secp256k1_pubkey pk;
        if (!secp256k1_ec_pubkey_create(ctx, &pk, parent.key)) return false;
        size_t plen = 33;
        secp256k1_ec_pubkey_serialize(ctx, data, &plen, &pk, SECP256K1_EC_COMPRESSED);
        dlen = 33;
    }
    data[dlen + 0] = (index >> 24) & 0xff;
    data[dlen + 1] = (index >> 16) & 0xff;
    data[dlen + 2] = (index >> 8) & 0xff;
    data[dlen + 3] = index & 0xff;
    dlen += 4;

    unsigned char I[64];
    HmacSha512(parent.chaincode, 32, data, dlen, I);

    memcpy(child.key, parent.key, 32);
    // child = (IL + parent) mod n ; rejects invalid (IL>=n or result 0)
    if (!secp256k1_ec_seckey_tweak_add(ctx, child.key, I)) return false;
    memcpy(child.chaincode, I + 32, 32);
    child.valid = true;
    return true;
}

bool DerivePath(const ExtKey& master, const std::vector<uint32_t>& path, ExtKey& out)
{
    ExtKey cur = master;
    for (size_t i = 0; i < path.size(); i++) {
        ExtKey nxt;
        if (!CKDpriv(cur, path[i], nxt)) return false;
        cur = nxt;
    }
    out = cur;
    return true;
}

bool DeriveTriangles(const std::string& mnemonic, const std::string& passphrase,
                     uint32_t account, uint32_t change, uint32_t index,
                     unsigned char privOut[32])
{
    unsigned char seed[64];
    if (!MnemonicToSeed(mnemonic, passphrase, seed)) return false;
    ExtKey master;
    if (!MasterFromSeed(seed, 64, master)) return false;
    std::vector<uint32_t> path;
    path.push_back(44u | HARDENED);
    path.push_back(TRI_COIN_TYPE | HARDENED);
    path.push_back(account | HARDENED);
    path.push_back(change);
    path.push_back(index);
    ExtKey leaf;
    if (!DerivePath(master, path, leaf)) return false;
    memcpy(privOut, leaf.key, 32);
    return true;
}

} // namespace hd
