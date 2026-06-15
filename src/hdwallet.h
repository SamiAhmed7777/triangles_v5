// Copyright (c) 2026 The Triangles developers
// Distributed under the MIT/X11 software license.
//
// Native BIP39 (mnemonic) + BIP32 (HD) key derivation for Triangles.
// Produces keys identical to the TRIdock web wallet (derivation path
// m/44'/2222'/0'/0/i, coin type 2222), so a 24-word phrase round-trips
// between the Qt/daemon wallet and the web wallet.
#ifndef TRIANGLES_HDWALLET_H
#define TRIANGLES_HDWALLET_H

#include <string>
#include <vector>
#include <cstdint>
#include <cstddef>

namespace hd {

static const uint32_t HARDENED      = 0x80000000u;
static const uint32_t TRI_COIN_TYPE = 2222u;   // matches triWallet.js

// A BIP32 extended private key (private scalar + chain code).
struct ExtKey {
    unsigned char key[32];
    unsigned char chaincode[32];
    bool valid;
    ExtKey() : valid(false) { }
};

// ---- BIP39 ----------------------------------------------------------------
// Generate a new mnemonic. strengthBits must be 128 (12 words) or 256 (24).
std::string GenerateMnemonic(int strengthBits = 256);
// Validate word membership + checksum.
bool CheckMnemonic(const std::string& mnemonic);
// PBKDF2-HMAC-SHA512(mnemonic, "mnemonic"+passphrase, 2048) -> 64-byte seed.
bool MnemonicToSeed(const std::string& mnemonic, const std::string& passphrase,
                    unsigned char seed64[64]);

// ---- BIP32 ----------------------------------------------------------------
bool MasterFromSeed(const unsigned char* seed, size_t seedlen, ExtKey& out);
bool CKDpriv(const ExtKey& parent, uint32_t index, ExtKey& child);
bool DerivePath(const ExtKey& master, const std::vector<uint32_t>& path, ExtKey& out);

// ---- High level -----------------------------------------------------------
// Derive the 32-byte private scalar for m/44'/coinType'/account'/change/index.
bool DeriveTriangles(const std::string& mnemonic, const std::string& passphrase,
                     uint32_t account, uint32_t change, uint32_t index,
                     unsigned char privOut[32]);

} // namespace hd
#endif // TRIANGLES_HDWALLET_H
