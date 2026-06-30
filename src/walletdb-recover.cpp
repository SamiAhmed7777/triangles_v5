// Copyright (c) 2026 The Triangles developers.
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// Berkeley-only wallet recovery helpers. See walletdb-recover.h.

#include "walletdb-recover.h"
#include "wallet.h"

#include <db_cxx.h>
#include <boost/version.hpp>
#include <cstdio>
#include <filesystem>
#include <list>
#include <map>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

class CWalletScanState_BdbOnly {
public:
    unsigned int nKeys;
    unsigned int nCKeys;
    unsigned int nKeyMeta;
    bool fIsEncrypted;
    bool fAnyUnordered;
    int nFileVersion;
    std::vector<uint256> vWalletUpgrade;

    CWalletScanState_BdbOnly() {
        nKeys = nCKeys = nKeyMeta = 0;
        fIsEncrypted = false;
        fAnyUnordered = false;
        nFileVersion = 0;
    }
};

static bool IsKeyType_BdbOnly(const std::string& strType)
{
    return (strType == "key" || strType == "wkey" ||
            strType == "mkey" || strType == "ckey" ||
            strType == "hdmnemonic" || strType == "hdcmnemonic");
}

// Same logic as walletdb.cpp::ReadKeyValue, but the only places it is called
// here are Recover() (which scans records) and the resulting scan state. The
// same logic — duplicated locally to avoid dragging in the typed batch seam
// for a Berkeley-only escape hatch.
static bool ReadKeyValue_BdbOnly(CWallet* pwallet, CDataStream& ssKey, CDataStream& ssValue,
                                 CWalletScanState_BdbOnly& wss,
                                 std::string& strType, std::string& strErr)
{
    try {
        ssKey >> strType;
        if (strType == "name") {
            std::string strAddress;
            ssKey >> strAddress;
            ssValue >> pwallet->mapAddressBook[CTrianglesAddress(strAddress).Get()];
        } else if (strType == "tx") {
            uint256 hash;
            ssKey >> hash;
            CWalletTx& wtx = pwallet->mapWallet[hash];
            ssValue >> wtx;
            if (wtx.CheckTransaction() && (wtx.GetHash() == hash))
                wtx.BindWallet(pwallet);
            else {
                pwallet->mapWallet.erase(hash);
                return false;
            }
            if (31404 <= wtx.fTimeReceivedIsTxTime && wtx.fTimeReceivedIsTxTime <= 31703) {
                wss.vWalletUpgrade.push_back(hash);
            }
        } else if (strType == "acentry") {
            std::string strAccount;
            ssKey >> strAccount;
            uint64_t nNumber;
            ssKey >> nNumber;
            // Note: we intentionally do NOT bump nAccountingEntryNumber here.
            // That counter is file-static in walletdb.cpp; the recovery path
            // does not need the high-water mark because the salvaged records
            // are not re-ordered or re-emitted as new entries.
            (void)nNumber;
        } else if (strType == "key" || strType == "wkey") {
            std::vector<unsigned char> vchPubKey;
            ssKey >> vchPubKey;
            CKey key;
            if (strType == "key") {
                wss.nKeys++;
                CPrivKey pkey;
                ssValue >> pkey;
                key.SetPubKey(vchPubKey);
                if (!key.SetPrivKey(pkey))
                { strErr = "Recover: CPrivKey corrupt"; return false; }
                if (key.GetPubKey() != vchPubKey)
                { strErr = "Recover: CPrivKey pubkey inconsistency"; return false; }
                if (!key.IsValid())
                { strErr = "Recover: invalid CPrivKey"; return false; }
            } else {
                CWalletKey wkey;
                ssValue >> wkey;
                key.SetPubKey(vchPubKey);
                if (!key.SetPrivKey(wkey.vchPrivKey))
                { strErr = "Recover: CPrivKey corrupt"; return false; }
                if (key.GetPubKey() != vchPubKey)
                { strErr = "Recover: CWalletKey pubkey inconsistency"; return false; }
                if (!key.IsValid())
                { strErr = "Recover: invalid CWalletKey"; return false; }
            }
            if (!pwallet->LoadKey(key))
            { strErr = "Recover: LoadKey failed"; return false; }
        } else if (strType == "mkey") {
            unsigned int nID;
            ssKey >> nID;
            CMasterKey kMasterKey;
            ssValue >> kMasterKey;
            if (pwallet->mapMasterKeys.count(nID) != 0) {
                strErr = strprintf("Recover: duplicate CMasterKey id %u", nID);
                return false;
            }
            pwallet->mapMasterKeys[nID] = kMasterKey;
            if (pwallet->nMasterKeyMaxID < nID)
                pwallet->nMasterKeyMaxID = nID;
        } else if (strType == "ckey") {
            wss.nCKeys++;
            std::vector<unsigned char> vchPubKey;
            ssKey >> vchPubKey;
            std::vector<unsigned char> vchPrivKey;
            ssValue >> vchPrivKey;
            if (!pwallet->LoadCryptedKey(vchPubKey, vchPrivKey))
            { strErr = "Recover: LoadCryptedKey failed"; return false; }
            wss.fIsEncrypted = true;
        } else if (strType == "keymeta") {
            CPubKey vchPubKey;
            ssKey >> vchPubKey;
            CKeyMetadata keyMeta;
            ssValue >> keyMeta;
            wss.nKeyMeta++;
            pwallet->LoadKeyMetadata(vchPubKey, keyMeta);
            if (!pwallet->nTimeFirstKey ||
                (keyMeta.nCreateTime < pwallet->nTimeFirstKey))
                pwallet->nTimeFirstKey = keyMeta.nCreateTime;
        } else if (strType == "defaultkey") {
            ssValue >> pwallet->vchDefaultKey;
        } else if (strType == "pool") {
            int64_t nIndex;
            ssKey >> nIndex;
            CKeyPool keypool;
            ssValue >> keypool;
            pwallet->setKeyPool.insert(nIndex);
            CKeyID keyid = keypool.vchPubKey.GetID();
            if (pwallet->mapKeyMetadata.count(keyid) == 0)
                pwallet->mapKeyMetadata[keyid] = CKeyMetadata(keypool.nTime);
        } else if (strType == "hdmnemonic") {
            std::string m;
            ssValue >> m;
            pwallet->LoadHDMnemonic(m);
        } else if (strType == "hdcmnemonic") {
            std::pair<uint256, std::vector<unsigned char>> cm;
            ssValue >> cm;
            pwallet->LoadCryptedHDMnemonic(cm.first, cm.second);
        } else if (strType == "hdchain") {
            int64_t n;
            ssValue >> n;
            pwallet->nHDChainIndex = n;
        } else if (strType == "version") {
            ssValue >> wss.nFileVersion;
            if (wss.nFileVersion == 10300)
                wss.nFileVersion = 300;
        } else if (strType == "cscript") {
            uint160 hash;
            ssKey >> hash;
            CScript script;
            ssValue >> script;
            if (!pwallet->LoadCScript(script))
            { strErr = "Recover: LoadCScript failed"; return false; }
        } else if (strType == "orderposnext") {
            ssValue >> pwallet->nOrderPosNext;
        }
    } catch (...) {
        return false;
    }
    return true;
}

bool BerkeleyRecoverWallet(CDBEnv& dbenv, std::string filename, bool fOnlyKeys)
{
    int64_t now = GetTime();
    std::string newFilename = strprintf("wallet.%"PRId64".bak", now);

    int result = dbenv.dbenv.dbrename(NULL, filename.c_str(), NULL,
                                      newFilename.c_str(), DB_AUTO_COMMIT);
    if (result == 0)
        printf("Renamed %s to %s\n", filename.c_str(), newFilename.c_str());
    else {
        printf("Failed to rename %s to %s\n", filename.c_str(), newFilename.c_str());
        return false;
    }

    std::vector<CDBEnv::KeyValPair> salvagedData;
    bool allOK = dbenv.Salvage(newFilename, true, salvagedData);
    if (salvagedData.empty()) {
        printf("Salvage(aggressive) found no records in %s.\n", newFilename.c_str());
        return false;
    }
    printf("Salvage(aggressive) found %"PRIszu" records\n", salvagedData.size());

    bool fSuccess = allOK;
    Db* pdbCopy = new Db(&dbenv.dbenv, 0);
    int ret = pdbCopy->open(NULL, filename.c_str(), "main", DB_BTREE, DB_CREATE, 0);
    if (ret > 0) {
        printf("Cannot create database file %s\n", filename.c_str());
        return false;
    }
    CWallet dummyWallet;
    CWalletScanState_BdbOnly wss;

    DbTxn* ptxn = dbenv.TxnBegin();
    for (CDBEnv::KeyValPair& row : salvagedData) {
        if (fOnlyKeys) {
            CDataStream ssKey(row.first, SER_DISK, CLIENT_VERSION);
            CDataStream ssValue(row.second, SER_DISK, CLIENT_VERSION);
            std::string strType, strErr;
            bool fReadOK = ReadKeyValue_BdbOnly(&dummyWallet, ssKey, ssValue,
                                                wss, strType, strErr);
            if (!IsKeyType_BdbOnly(strType))
                continue;
            if (!fReadOK) {
                printf("WARNING: BerkeleyRecoverWallet skipping %s: %s\n",
                       strType.c_str(), strErr.c_str());
                continue;
            }
        }
        Dbt datKey(&row.first[0], row.first.size());
        Dbt datValue(&row.second[0], row.second.size());
        int ret2 = pdbCopy->put(ptxn, &datKey, &datValue, DB_NOOVERWRITE);
        if (ret2 > 0)
            fSuccess = false;
    }
    ptxn->commit(0);
    pdbCopy->close(0);
    delete pdbCopy;

    return fSuccess;
}

bool BerkeleyZapWalletTx(const std::string& strWalletFile)
{
    printf("BerkeleyZapWalletTx: erasing transaction records from %s\n",
           strWalletFile.c_str());

    // Walk the Berkeley file directly. The CDB wrapper hides its members, but
    // the underlying Db* / Dbc* API is the same thing the wrapper does.
    DbEnv env(0u);
    env.set_error_stream(&std::cerr);
    u_int32_t envFlags = DB_CREATE | DB_INIT_MPOOL | DB_PRIVATE;
    if (env.open(GetDataDir().string().c_str(), envFlags, 0) != 0) {
        printf("BerkeleyZapWalletTx: cannot open Berkeley environment\n");
        return false;
    }

    bool ok = false;
    {
        Db db(&env, 0);
        if (db.open(nullptr, strWalletFile.c_str(), "main", DB_BTREE, DB_RDONLY, 0) != 0) {
            printf("BerkeleyZapWalletTx: failed to open wallet database\n");
            env.close(0);
            return false;
        }

        Dbc* pcursor = nullptr;
        if (db.cursor(nullptr, &pcursor, 0) != 0) {
            printf("BerkeleyZapWalletTx: failed to get cursor\n");
            db.close(0);
            env.close(0);
            return false;
        }

        std::vector<uint256> vTxHash;
        Dbt datKey, datValue;
        while (pcursor->get(&datKey, &datValue, DB_NEXT) == 0) {
            try {
                CDataStream ssKey(static_cast<const char*>(datKey.get_data()),
                                   static_cast<const char*>(datKey.get_data()) + datKey.get_size(),
                                   SER_DISK, CLIENT_VERSION);
                std::string strType;
                ssKey >> strType;
                if (strType == "tx") {
                    uint256 hash;
                    ssKey >> hash;
                    vTxHash.push_back(hash);
                }
            } catch (...) {
                // Skip records we cannot decode — salvage logic is best-effort.
            }
        }
        pcursor->close();
        db.close(0);

        // Second pass: re-open the file in r/w mode and erase the collected tx
        // records. Two separate connections keep the read pass free of the
        // BDB cursor lifetime rules.
        if (db.open(nullptr, strWalletFile.c_str(), "main", DB_BTREE, DB_CREATE, 0) != 0) {
            printf("BerkeleyZapWalletTx: failed to reopen wallet for erase\n");
            env.close(0);
            return false;
        }

        int nErased = 0;
        for (const uint256& hash : vTxHash) {
            CDataStream ssKey(SER_DISK, CLIENT_VERSION);
            ssKey << std::make_pair(std::string("tx"), hash);
            Dbt datKey2(&ssKey[0], ssKey.size());
            int rc = db.del(nullptr, &datKey2, 0);
            if (rc == 0 || rc == DB_NOTFOUND)
                ++nErased;
        }
        db.close(0);
        printf("BerkeleyZapWalletTx: erased %d of %d transaction records\n",
               nErased, (int)vTxHash.size());
        ok = true;
    }
    env.close(0);
    return ok;
}