// Copyright (c) 2026 The Triangles developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef TRIANGLES_ADDRESSINDEX_H
#define TRIANGLES_ADDRESSINDEX_H

#include "uint256.h"
#include "serialize.h"
#include <string>

/** Address types for the address index */
enum AddressType {
    ADDR_TYPE_P2PKH = 1,   // Pay-to-pubkey-hash (CKeyID)
    ADDR_TYPE_P2SH  = 2,   // Pay-to-script-hash (CScriptID)
};

/**
 * Key for address balance entries in LevelDB.
 * LevelDB key: "addrbal" + CAddressBalanceKey
 * LevelDB value: int64_t (balance in satoshis)
 */
struct CAddressBalanceKey {
    int nType;
    uint160 hashBytes;

    CAddressBalanceKey() : nType(0), hashBytes(0) {}
    CAddressBalanceKey(int type, const uint160& hash) : nType(type), hashBytes(hash) {}

    IMPLEMENT_SERIALIZE(
        READWRITE(nType);
        READWRITE(hashBytes);
    )
};

/**
 * Key for address UTXO entries in LevelDB.
 * LevelDB key: "addrutxo" + CAddressUtxoKey
 * LevelDB value: CAddressUtxoValue
 */
struct CAddressUtxoKey {
    int nType;
    uint160 hashBytes;
    uint256 txhash;
    int nIndex;

    CAddressUtxoKey() : nType(0), hashBytes(0), txhash(0), nIndex(0) {}
    CAddressUtxoKey(int type, const uint160& hash, const uint256& tx, int idx)
        : nType(type), hashBytes(hash), txhash(tx), nIndex(idx) {}

    IMPLEMENT_SERIALIZE(
        READWRITE(nType);
        READWRITE(hashBytes);
        READWRITE(txhash);
        READWRITE(nIndex);
    )
};

struct CAddressUtxoValue {
    int64_t nValue;
    int nHeight;
    CScript script;

    CAddressUtxoValue() : nValue(0), nHeight(0) {}
    CAddressUtxoValue(int64_t val, int height, const CScript& s)
        : nValue(val), nHeight(height), script(s) {}

    IMPLEMENT_SERIALIZE(
        READWRITE(nValue);
        READWRITE(nHeight);
        READWRITE(script);
    )
};

/**
 * Key for address transaction history entries in LevelDB.
 * LevelDB key: "addrtxid" + CAddressTxIdKey
 * LevelDB value: empty (key contains all info)
 */
struct CAddressTxIdKey {
    int nType;
    uint160 hashBytes;
    int nHeight;
    int nTxIndex; // position within block
    uint256 txhash;

    CAddressTxIdKey() : nType(0), hashBytes(0), nHeight(0), nTxIndex(0), txhash(0) {}
    CAddressTxIdKey(int type, const uint160& hash, int height, int txidx, const uint256& tx)
        : nType(type), hashBytes(hash), nHeight(height), nTxIndex(txidx), txhash(tx) {}

    IMPLEMENT_SERIALIZE(
        READWRITE(nType);
        READWRITE(hashBytes);
        READWRITE(nHeight);
        READWRITE(nTxIndex);
        READWRITE(txhash);
    )
};

extern bool fAddressIndex;

#endif // TRIANGLES_ADDRESSINDEX_H
