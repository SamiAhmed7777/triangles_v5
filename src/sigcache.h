// Copyright (c) 2024 The Triangles developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef TRIANGLES_SCRIPT_VERIFY_CACHE_H
#define TRIANGLES_SCRIPT_VERIFY_CACHE_H

#include "uint256.h"
#include "sync.h"

#include <openssl/sha.h>
#include <cstring>
#include <unordered_set>

/**
 * High-level script verification cache keyed by (txid, input_index).
 * Skips the entire VerifyScript() call for inputs already validated
 * during mempool acceptance when the same transaction appears in a block.
 *
 * Complements the lower-level CSignatureCache in script.cpp which caches
 * individual ECDSA signature checks.
 *
 * ~256KB memory footprint at 32K entries.
 */
class CScriptVerifyCache
{
private:
    static const unsigned int MAX_CACHE_SIZE = 32768;

    struct Uint256Hasher {
        size_t operator()(const uint256& v) const {
            return *reinterpret_cast<const size_t*>(v.begin());
        }
    };

    mutable CCriticalSection cs;
    std::unordered_set<uint256, Uint256Hasher> setValid;

    uint256 ComputeKey(const uint256& txid, unsigned int nIn) const
    {
        unsigned char data[36]; // 32 bytes txid + 4 bytes input index
        memcpy(data, txid.begin(), 32);
        memcpy(data + 32, &nIn, 4);
        uint256 result;
        SHA256(data, 36, (unsigned char*)&result);
        return result;
    }

public:
    bool Get(const uint256& txid, unsigned int nIn) const
    {
        LOCK(cs);
        return setValid.count(ComputeKey(txid, nIn)) > 0;
    }

    void Set(const uint256& txid, unsigned int nIn)
    {
        LOCK(cs);
        if (setValid.size() >= MAX_CACHE_SIZE)
        {
            // Evict half the cache when full
            auto it = setValid.begin();
            unsigned int nEvict = MAX_CACHE_SIZE / 2;
            while (nEvict > 0 && it != setValid.end()) {
                it = setValid.erase(it);
                --nEvict;
            }
        }
        setValid.insert(ComputeKey(txid, nIn));
    }
};

#endif // TRIANGLES_SCRIPT_VERIFY_CACHE_H
