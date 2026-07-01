// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef TRIANGLES_WALLETDB_H
#define TRIANGLES_WALLETDB_H

#include "walletdb-batch.h"   // CWalletBatchTyped (the typed batch seam)
#include "base58.h"

class CKeyPool;
class CAccount;
class CAccountingEntry;
class CBlockLocator;         // forward decl — pulled in via db.h→main.h before
class CPubKey;
class CScript;
class CMasterKey;
class uint160;
class uint256;
class CWallet;               // pulled in via db.h→main.h→wallet.h before
class CWalletTx;             // forward decl — walletdb.h used to pull this in
                             // transitively via db.h; the seam removes that.

// Wallet-update counter used by the periodic flush thread (db.cpp defines it).
// Touched on every wallet write; needed regardless of backend so the daemon's
// auto-flush logic can detect changes to the wallet file.
extern unsigned int nWalletDBUpdated;

/** Error statuses for the wallet database */
enum DBErrors
{
    DB_LOAD_OK,
    DB_CORRUPT,
    DB_NONCRITICAL_ERROR,
    DB_TOO_NEW,
    DB_LOAD_FAIL,
    DB_NEED_REWRITE
};

class CKeyMetadata
{
public:
    static const int CURRENT_VERSION=1;
    int nVersion;
    int64_t nCreateTime; // 0 means unknown

    CKeyMetadata()
    {
        SetNull();
    }
    CKeyMetadata(int64_t nCreateTime_)
    {
        nVersion = CKeyMetadata::CURRENT_VERSION;
        nCreateTime = nCreateTime_;
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(this->nVersion);
        nVersion = this->nVersion;
        READWRITE(nCreateTime);
    )

    void SetNull()
    {
        nVersion = CKeyMetadata::CURRENT_VERSION;
        nCreateTime = 0;
    }
};



/** Access to the wallet database (wallet.dat) */
class CWalletDB : public CWalletBatchTyped
{
public:
    /**
     * Open (or create) the wallet database via the configured backend
     * (-walletdb, default SQLite). The legacy pszMode argument is accepted
     * for source compatibility but currently ignored — SQLite is always
     * opened read/write with create-if-missing.
     */
    CWalletDB(std::string strFilename, const char* pszMode="r+");
private:
    CWalletDB(const CWalletDB&);
    void operator=(const CWalletDB&);
public:

    bool WriteName(const std::string& strAddress, const std::string& strName);

    bool EraseName(const std::string& strAddress);

    bool WriteTx(uint256 hash, const CWalletTx& wtx)
    {
        nWalletDBUpdated++;
        return Write(std::make_pair(std::string("tx"), hash), wtx);
    }

    bool EraseTx(uint256 hash)
    {
        nWalletDBUpdated++;
        return Erase(std::make_pair(std::string("tx"), hash));
    }
    bool WriteKey(const CPubKey& vchPubKey, const CPrivKey& vchPrivKey, const CKeyMetadata &keyMeta)
    {
        nWalletDBUpdated++;

        if(!Write(std::make_pair(std::string("keymeta"), vchPubKey), keyMeta))
            return false;

        return Write(std::make_pair(std::string("key"), vchPubKey.Raw()), vchPrivKey, false);
    }

    bool WriteCryptedKey(const CPubKey& vchPubKey, const std::vector<unsigned char>& vchCryptedSecret, const CKeyMetadata &keyMeta)
    {
        nWalletDBUpdated++;
        bool fEraseUnencryptedKey = true;

        if(!Write(std::make_pair(std::string("keymeta"), vchPubKey), keyMeta))
            return false;

        if (!Write(std::make_pair(std::string("ckey"), vchPubKey.Raw()), vchCryptedSecret, false))
            return false;
        if (fEraseUnencryptedKey)
        {
            Erase(std::make_pair(std::string("key"), vchPubKey.Raw()));
            Erase(std::make_pair(std::string("wkey"), vchPubKey.Raw()));
        }
        return true;
    }

    bool WriteMasterKey(unsigned int nID, const CMasterKey& kMasterKey)
    {
        nWalletDBUpdated++;
        return Write(std::make_pair(std::string("mkey"), nID), kMasterKey, true);
    }

    bool WriteCScript(const uint160& hash, const CScript& redeemScript)
    {
        nWalletDBUpdated++;
        return Write(std::make_pair(std::string("cscript"), hash), redeemScript, false);
    }

    bool WriteBestBlock(const CBlockLocator& locator)
    {
        nWalletDBUpdated++;
        return Write(std::string("bestblock"), locator);
    }

    bool ReadBestBlock(CBlockLocator& locator)
    {
        return Read(std::string("bestblock"), locator);
    }

    bool WriteOrderPosNext(int64_t nOrderPosNext)
    {
        nWalletDBUpdated++;
        return Write(std::string("orderposnext"), nOrderPosNext);
    }

    bool WriteDefaultKey(const CPubKey& vchPubKey)
    {
        nWalletDBUpdated++;
        return Write(std::string("defaultkey"), vchPubKey.Raw());
    }

    bool WriteHDMnemonic(const std::string& mnemonic) {
        nWalletDBUpdated++;
        Erase(std::string("hdcmnemonic"));
        return Write(std::string("hdmnemonic"), mnemonic);
    }
    bool WriteHDCryptedMnemonic(const uint256& iv, const std::vector<unsigned char>& cipher) {
        nWalletDBUpdated++;
        Erase(std::string("hdmnemonic"));
        return Write(std::string("hdcmnemonic"), std::make_pair(iv, cipher));
    }
    bool WriteHDChain(int64_t nIndex) {
        nWalletDBUpdated++;
        return Write(std::string("hdchain"), nIndex);
    }
    // BIP39 passphrase ("25th word"). Same plaintext/crypted lifecycle as the
    // mnemonic: exactly one of the two records exists at a time; both absent
    // means no passphrase (legacy wallets and the common case).
    bool WriteHDPassphrase(const std::string& passphrase) {
        nWalletDBUpdated++;
        Erase(std::string("hdcpassphrase"));
        return Write(std::string("hdpassphrase"), passphrase);
    }
    bool WriteHDCryptedPassphrase(const uint256& iv, const std::vector<unsigned char>& cipher) {
        nWalletDBUpdated++;
        Erase(std::string("hdpassphrase"));
        return Write(std::string("hdcpassphrase"), std::make_pair(iv, cipher));
    }
    bool EraseHDPassphrase() {
        nWalletDBUpdated++;
        Erase(std::string("hdpassphrase"));
        Erase(std::string("hdcpassphrase"));
        return true;
    }

    bool ReadPool(int64_t nPool, CKeyPool& keypool)
    {
        return Read(std::make_pair(std::string("pool"), nPool), keypool);
    }

    bool WritePool(int64_t nPool, const CKeyPool& keypool)
    {
        nWalletDBUpdated++;
        return Write(std::make_pair(std::string("pool"), nPool), keypool);
    }

    bool ErasePool(int64_t nPool)
    {
        nWalletDBUpdated++;
        return Erase(std::make_pair(std::string("pool"), nPool));
    }

    // Settings are no longer stored in wallet.dat; these are
    // used only for backwards compatibility:
    template<typename T>
    bool ReadSetting(const std::string& strKey, T& value)
    {
        return Read(std::make_pair(std::string("setting"), strKey), value);
    }
    template<typename T>
    bool WriteSetting(const std::string& strKey, const T& value)
    {
        nWalletDBUpdated++;
        return Write(std::make_pair(std::string("setting"), strKey), value);
    }
    bool EraseSetting(const std::string& strKey)
    {
        nWalletDBUpdated++;
        return Erase(std::make_pair(std::string("setting"), strKey));
    }

    bool WriteMinVersion(int nVersion)
    {
        return Write(std::string("minversion"), nVersion);
    }

    // Mirrors the legacy CDB::WriteVersion / ReadVersion; explicitly retained
    // because LoadWallet() upgrades the on-disk version to CLIENT_VERSION.
    bool WriteVersion(int nVersion)
    {
        return Write(std::string("version"), nVersion);
    }
    bool ReadVersion(int& nVersion)
    {
        nVersion = 0;
        return Read(std::string("version"), nVersion);
    }

    bool ReadAccount(const std::string& strAccount, CAccount& account);
    bool WriteAccount(const std::string& strAccount, const CAccount& account);
private:
    bool WriteAccountingEntry(const uint64_t nAccEntryNum, const CAccountingEntry& acentry);
public:
    bool WriteAccountingEntry(const CAccountingEntry& acentry);
    int64_t GetAccountCreditDebit(const std::string& strAccount);
    void ListAccountCreditDebit(const std::string& strAccount, std::list<CAccountingEntry>& acentries);

    DBErrors ReorderTransactions(CWallet*);
    DBErrors LoadWallet(CWallet* pwallet);
    // NOTE: Recover() / ZapWalletTx() are Berkeley-only escape hatches. They
    // live in walletdb-recover.{h,cpp} (which still depends on db.h / db_cxx.h).
    // After wallet migration to SQLite those helpers are invoked on the
    // .bdb.bak copy at startup, never on the live wallet.
};

#endif // TRIANGLES_WALLETDB_H
