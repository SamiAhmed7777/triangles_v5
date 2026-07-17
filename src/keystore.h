// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef TRIANGLES_KEYSTORE_H
#define TRIANGLES_KEYSTORE_H

#include "crypter.h"
#include "util_signal.h"
#include "sync.h"

#include <utility>

class CScript;

/** A virtual base class for key stores */
class CKeyStore
{
protected:
    mutable CCriticalSection cs_KeyStore;

public:
    virtual ~CKeyStore() {}

    // Add a key to the store.
    virtual bool AddKey(const CKey& key) =0;

    // Check whether a key corresponding to a given address is present in the store.
    virtual bool HaveKey(const CKeyID &address) const =0;
    virtual bool GetKey(const CKeyID &address, CKey& keyOut) const =0;
    virtual void GetKeys(std::set<CKeyID> &setAddress) const =0;
    virtual bool GetPubKey(const CKeyID &address, CPubKey& vchPubKeyOut) const;

    // Support for BIP 0013 : see https://en.bitcoin.it/wiki/BIP_0013
    virtual bool AddCScript(const CScript& redeemScript) =0;
    virtual bool HaveCScript(const CScriptID &hash) const =0;
    virtual bool GetCScript(const CScriptID &hash, CScript& redeemScriptOut) const =0;

    virtual bool GetSecret(const CKeyID &address, CSecret& vchSecret, bool &fCompressed) const
    {
        CKey key;
        if (!GetKey(address, key))
            return false;
        vchSecret = key.GetSecret(fCompressed);
        return true;
    }
};

using KeyMap = std::map<CKeyID, std::pair<CSecret, bool>>;
using ScriptMap = std::map<CScriptID, CScript>;

/** Basic key store, that keeps keys in an address->secret map */
class CBasicKeyStore : public CKeyStore
{
protected:
    KeyMap mapKeys;
    ScriptMap mapScripts;

public:
    bool AddKey(const CKey& key);
    bool HaveKey(const CKeyID &address) const
    {
        bool result;
        {
            LOCK(cs_KeyStore);
            result = (mapKeys.count(address) > 0);
        }
        return result;
    }
    void GetKeys(std::set<CKeyID> &setAddress) const
    {
        setAddress.clear();
        {
            LOCK(cs_KeyStore);
            for (const auto& [key, val] : mapKeys)
            {
                setAddress.insert(key);
            }
        }
    }
    bool GetKey(const CKeyID &address, CKey &keyOut) const
    {
        {
            LOCK(cs_KeyStore);
            if (auto mi = mapKeys.find(address); mi != mapKeys.end())
            {
                keyOut.Reset();
                keyOut.SetSecret(mi->second.first, mi->second.second);
                return true;
            }
        }
        return false;
    }
    virtual bool AddCScript(const CScript& redeemScript);
    virtual bool HaveCScript(const CScriptID &hash) const;
    virtual bool GetCScript(const CScriptID &hash, CScript& redeemScriptOut) const;
};

using CryptedKeyMap = std::map<CKeyID, std::pair<CPubKey, std::vector<unsigned char>>>;

/** Keystore which keeps the private keys encrypted.
 * It derives from the basic key store, which is used if no encryption is active.
 */
class CCryptoKeyStore : public CBasicKeyStore
{
private:
    // if fUseCrypto is true, mapKeys must be empty
    // if fUseCrypto is false, vMasterKey must be empty
    bool fUseCrypto;

protected:
    CryptedKeyMap mapCryptedKeys;
    CKeyingMaterial vMasterKey;

    bool SetCrypted();

    // Stage and commit wallet-key encryption separately so callers can make
    // the on-disk update atomic before discarding plaintext keys in memory.
    bool PrepareKeyEncryption(CKeyingMaterial& vMasterKeyIn,
                              CryptedKeyMap& cryptedKeysOut) const;
    bool CommitKeyEncryption(CryptedKeyMap&& cryptedKeys);

    // Encrypt previously unencrypted keys in memory.
    bool EncryptKeys(CKeyingMaterial& vMasterKeyIn);

    bool Unlock(const CKeyingMaterial& vMasterKeyIn);

public:
    CCryptoKeyStore() : fUseCrypto(false)
    {
    }

    bool IsCrypted() const
    {
        return fUseCrypto;
    }

    bool IsLocked() const
    {
        if (!IsCrypted())
            return false;
        bool result;
        {
            LOCK(cs_KeyStore);
            result = vMasterKey.empty();
        }
        return result;
    }

    bool LockKeyStore();

    virtual bool AddCryptedKey(const CPubKey &vchPubKey, const std::vector<unsigned char> &vchCryptedSecret);
    bool AddKey(const CKey& key);
    bool HaveKey(const CKeyID &address) const
    {
        {
            LOCK(cs_KeyStore);
            if (!IsCrypted())
                return CBasicKeyStore::HaveKey(address);
            return mapCryptedKeys.count(address) > 0;
        }
        return false;
    }
    bool GetKey(const CKeyID &address, CKey& keyOut) const;
    bool GetPubKey(const CKeyID &address, CPubKey& vchPubKeyOut) const;
    void GetKeys(std::set<CKeyID> &setAddress) const
    {
        LOCK(cs_KeyStore);
        if (!IsCrypted())
        {
            CBasicKeyStore::GetKeys(setAddress);
            return;
        }
        setAddress.clear();
        for (const auto& [key, val] : mapCryptedKeys)
        {
            setAddress.insert(key);
        }
    }

    /* Wallet status (encrypted, locked) changed.
     * Note: Called without locks held.
     */
    CSignal<void(CCryptoKeyStore*)> NotifyStatusChanged;
};

#endif
