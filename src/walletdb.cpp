// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// CWalletDB — typed, backend-agnostic wallet database access.
//
// After the cpp20-modernization rebase, CWalletDB derives from
// CWalletBatchTyped instead of the legacy Berkeley CDB. The typed Read/Write/
// Erase/Exists templates now come from the seam, and the underlying byte-level
// database is owned by a std::unique_ptr<WalletDatabase> that is selected by
// -walletdb (default: SQLite via walletdb-factory.cpp).
//
// What stays exactly the same:
//   - Every typed wrapper (WriteName, WriteTx, WriteKey, WriteMasterKey,
//     ReadPool, WriteSetting, ...) keeps its body — only the *base class*
//     changed, the templates resolve to the same signatures.
//   - LoadWallet, ReorderTransactions, ListAccountCreditDebit keep their
//     semantics byte-for-byte. The Berkeley cursor (GetCursor/ReadAtCursor)
//     is replaced with StartCursor()/NextRecord(), which iterate the whole
//     keyspace; range-seek call sites filter in the loop (the SQLite cursor
//     does not support keyed range seeks).
//
// What moves out:
//   - The Berkeley-only recovery helpers (Recover / ZapWalletTx) live in
//     walletdb-recover.{h,cpp} now. They operate on raw CDB / bitdb and are
//     invoked only on legacy (pre-migration) Berkeley wallet files.

#include "walletdb.h"
#include "wallet.h"
#include "walletdb-base.h"
#include "db.h"
#include "walletdb-recover.h"

#include <filesystem>
#include <memory>
#include <stdexcept>
#include <utility>

namespace fs = std::filesystem;

static uint64_t nAccountingEntryNumber = 0;
extern bool fWalletUnlockStakingOnly;

//
// Auto-backup wallet before flush/rewrite operations.
// Copies wallet.dat to wallet.dat.auto.bak if the backup is older than the wallet.
// Returns true if backup was created or already up to date.
//
bool AutoBackupWallet(const fs::path& walletPath)
{
    fs::path backupPath = walletPath.string() + ".auto.bak";
    try {
        if (!fs::exists(walletPath))
            return true;
        uintmax_t walletSize = fs::file_size(walletPath);
        if (walletSize < 1024) {
            printf("AutoBackupWallet: wallet.dat is only %llu bytes (possibly corrupt), skipping auto-backup\n",
                   (unsigned long long)walletSize);
            return false;
        }
        if (fs::exists(backupPath)) {
            uintmax_t backupSize = fs::file_size(backupPath);
            if (backupSize == walletSize)
                return true;
        }
        fs::copy_file(walletPath, backupPath, fs::copy_options::overwrite_existing);
        printf("AutoBackupWallet: backed up wallet.dat (%llu bytes) to wallet.dat.auto.bak\n",
               (unsigned long long)walletSize);
        return true;
    } catch (const fs::filesystem_error& e) {
        printf("AutoBackupWallet: failed - %s\n", e.what());
        return false;
    }
}

//
// CWalletDB
//

CWalletDB::CWalletDB(std::string strFilename, const char* /*pszMode*/)
    : CWalletBatchTyped()
{
    std::string strError;
    auto db = MakeWalletDatabase(strFilename, strError);
    if (!db) {
        // The factory documents the BDB branch as "not yet wired into the
        // seam". Surface the same error string to the caller so they see a
        // clear message instead of a generic open failure.
        throw std::runtime_error("CWalletDB: cannot open wallet '" + strFilename +
                                 "': " + (strError.empty() ? "unknown error" : strError));
    }
    if (!Open(std::move(db))) {
        throw std::runtime_error("CWalletDB: backend produced no batch for '" +
                                 strFilename + "'");
    }
}

bool CWalletDB::WriteName(const std::string& strAddress, const std::string& strName)
{
    nWalletDBUpdated++;
    return Write(std::make_pair(std::string("name"), strAddress), strName);
}

bool CWalletDB::EraseName(const std::string& strAddress)
{
    nWalletDBUpdated++;
    return Erase(std::make_pair(std::string("name"), strAddress));
}

bool CWalletDB::ReadAccount(const std::string& strAccount, CAccount& account)
{
    account.SetNull();
    return Read(std::make_pair(std::string("acc"), strAccount), account);
}

bool CWalletDB::WriteAccount(const std::string& strAccount, const CAccount& account)
{
    return Write(std::make_pair(std::string("acc"), strAccount), account);
}

bool CWalletDB::WriteAccountingEntry(const uint64_t nAccEntryNum, const CAccountingEntry& acentry)
{
    return Write(std::make_tuple(std::string("acentry"), acentry.strAccount, nAccEntryNum), acentry);
}

bool CWalletDB::WriteAccountingEntry(const CAccountingEntry& acentry)
{
    return WriteAccountingEntry(++nAccountingEntryNumber, acentry);
}

int64_t CWalletDB::GetAccountCreditDebit(const std::string& strAccount)
{
    std::list<CAccountingEntry> entries;
    ListAccountCreditDebit(strAccount, entries);

    int64_t nCreditDebit = 0;
    for (const CAccountingEntry& entry : entries)
        nCreditDebit += entry.nCreditDebit;

    return nCreditDebit;
}

//
// ListAccountCreditDebit — Berkeley's DB_SET_RANGE seeked to the first record
// whose key was >= ("acentry", strAccount, 0). The SQLite cursor iterates the
// whole keyspace; we instead walk every record and filter in the loop. The
// serialization of ("acentry", strAccount, nEntryNo) means the per-account
// entries cluster together and a string comparison still gives the correct
// grouping (each ("acentry", strAccount, n) key starts with the literal
// "acentry" prefix followed by the account name, which is a flat string).
//
void CWalletDB::ListAccountCreditDebit(const std::string& strAccount, std::list<CAccountingEntry>& entries)
{
    const bool fAllAccounts = (strAccount == "*");

    auto cursor = StartCursor();
    if (!cursor)
        throw std::runtime_error("CWalletDB::ListAccountCreditDebit() : cannot create DB cursor");

    while (true) {
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        CDataStream ssValue(SER_DISK, CLIENT_VERSION);
        bool fError = false;
        if (!NextRecord(*cursor, ssKey, ssValue, fError)) {
            if (fError)
                throw std::runtime_error("CWalletDB::ListAccountCreditDebit() : error scanning DB");
            break;
        }

        // Unserialize. Unlike the Berkeley cursor -- which iterated in sorted
        // key order and was positioned at the ("acentry", strAccount) prefix
        // via DB_SET_RANGE, so it could stop at the first non-matching record --
        // the SQLite cursor scans the whole keyspace in unspecified order.
        // We must therefore skip non-matching records and keep scanning.
        std::string strType;
        ssKey >> strType;
        if (strType != "acentry")
            continue;
        CAccountingEntry acentry;
        ssKey >> acentry.strAccount;
        if (!fAllAccounts && acentry.strAccount != strAccount)
            continue;

        ssValue >> acentry;
        ssKey >> acentry.nEntryNo;
        entries.push_back(acentry);
    }
}

DBErrors CWalletDB::ReorderTransactions(CWallet* pwallet)
{
    LOCK(pwallet->cs_wallet);
    typedef std::pair<CWalletTx*, CAccountingEntry*> TxPair;
    typedef std::multimap<int64_t, TxPair> TxItems;
    TxItems txByTime;

    for (auto it = pwallet->mapWallet.begin(); it != pwallet->mapWallet.end(); ++it) {
        CWalletTx* wtx = &it->second;
        txByTime.insert(std::make_pair(wtx->nTimeReceived, TxPair(wtx, (CAccountingEntry*)0)));
    }
    std::list<CAccountingEntry> acentries;
    ListAccountCreditDebit("", acentries);
    for (CAccountingEntry& entry : acentries) {
        txByTime.insert(std::make_pair(entry.nTime, TxPair((CWalletTx*)0, &entry)));
    }

    int64_t& nOrderPosNext = pwallet->nOrderPosNext;
    nOrderPosNext = 0;
    std::vector<int64_t> nOrderPosOffsets;
    for (auto it = txByTime.begin(); it != txByTime.end(); ++it) {
        CWalletTx* const pwtx = it->second.first;
        CAccountingEntry* const pacentry = it->second.second;
        int64_t& nOrderPos = (pwtx != 0) ? pwtx->nOrderPos : pacentry->nOrderPos;

        if (nOrderPos == -1) {
            nOrderPos = nOrderPosNext++;
            nOrderPosOffsets.push_back(nOrderPos);

            if (pacentry)
                if (!WriteAccountingEntry(pacentry->nEntryNo, *pacentry))
                    return DB_LOAD_FAIL;
        } else {
            int64_t nOrderPosOff = 0;
            for (const int64_t& nOffsetStart : nOrderPosOffsets) {
                if (nOrderPos >= nOffsetStart)
                    ++nOrderPosOff;
            }
            nOrderPos += nOrderPosOff;
            nOrderPosNext = std::max(nOrderPosNext, nOrderPos + 1);

            if (!nOrderPosOff)
                continue;

            if (pwtx) {
                if (!WriteTx(pwtx->GetHash(), *pwtx))
                    return DB_LOAD_FAIL;
            } else {
                if (!WriteAccountingEntry(pacentry->nEntryNo, *pacentry))
                    return DB_LOAD_FAIL;
            }
        }
    }

    return DB_LOAD_OK;
}

class CWalletScanState {
public:
    unsigned int nKeys;
    unsigned int nCKeys;
    unsigned int nKeyMeta;
    bool fIsEncrypted;
    bool fAnyUnordered;
    int nFileVersion;
    std::vector<uint256> vWalletUpgrade;

    CWalletScanState() {
        nKeys = nCKeys = nKeyMeta = 0;
        fIsEncrypted = false;
        fAnyUnordered = false;
        nFileVersion = 0;
    }
};

static bool IsKeyType(const std::string& strType)
{
    return (strType == "key" || strType == "wkey" ||
            strType == "mkey" || strType == "ckey" ||
            strType == "hdmnemonic" || strType == "hdcmnemonic" ||
            strType == "hdpassphrase" || strType == "hdcpassphrase");
}

static bool ReadKeyValue(CWallet* pwallet, CDataStream& ssKey, CDataStream& ssValue,
                         CWalletScanState& wss, std::string& strType, std::string& strErr)
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
                if (!ssValue.empty()) {
                    char fTmp;
                    char fUnused;
                    ssValue >> fTmp >> fUnused >> wtx.strFromAccount;
                    strErr = strprintf("LoadWallet() upgrading tx ver=%d %d '%s' %s",
                                       wtx.fTimeReceivedIsTxTime, fTmp, wtx.strFromAccount.c_str(), hash.ToString().c_str());
                    wtx.fTimeReceivedIsTxTime = fTmp;
                } else {
                    strErr = strprintf("LoadWallet() repairing tx ver=%d %s",
                                       wtx.fTimeReceivedIsTxTime, hash.ToString().c_str());
                    wtx.fTimeReceivedIsTxTime = 0;
                }
                wss.vWalletUpgrade.push_back(hash);
            }

            if (wtx.nOrderPos == -1)
                wss.fAnyUnordered = true;
        } else if (strType == "acentry") {
            std::string strAccount;
            ssKey >> strAccount;
            uint64_t nNumber;
            ssKey >> nNumber;
            if (nNumber > nAccountingEntryNumber)
                nAccountingEntryNumber = nNumber;
            if (!wss.fAnyUnordered) {
                CAccountingEntry acentry;
                ssValue >> acentry;
                if (acentry.nOrderPos == -1)
                    wss.fAnyUnordered = true;
            }
        } else if (strType == "key" || strType == "wkey") {
            std::vector<unsigned char> vchPubKey;
            ssKey >> vchPubKey;
            CKey key;
            if (strType == "key") {
                wss.nKeys++;
                CPrivKey pkey;
                ssValue >> pkey;
                key.SetPubKey(vchPubKey);
                if (!key.SetPrivKey(pkey)) {
                    strErr = "Error reading wallet database: CPrivKey corrupt";
                    return false;
                }
                if (key.GetPubKey() != vchPubKey) {
                    strErr = "Error reading wallet database: CPrivKey pubkey inconsistency";
                    return false;
                }
                if (!key.IsValid()) {
                    strErr = "Error reading wallet database: invalid CPrivKey";
                    return false;
                }
            } else {
                CWalletKey wkey;
                ssValue >> wkey;
                key.SetPubKey(vchPubKey);
                if (!key.SetPrivKey(wkey.vchPrivKey)) {
                    strErr = "Error reading wallet database: CPrivKey corrupt";
                    return false;
                }
                if (key.GetPubKey() != vchPubKey) {
                    strErr = "Error reading wallet database: CWalletKey pubkey inconsistency";
                    return false;
                }
                if (!key.IsValid()) {
                    strErr = "Error reading wallet database: invalid CWalletKey";
                    return false;
                }
            }
            if (!pwallet->LoadKey(key)) {
                strErr = "Error reading wallet database: LoadKey failed";
                return false;
            }
        } else if (strType == "mkey") {
            unsigned int nID;
            ssKey >> nID;
            CMasterKey kMasterKey;
            ssValue >> kMasterKey;
            if (pwallet->mapMasterKeys.count(nID) != 0) {
                strErr = strprintf("Error reading wallet database: duplicate CMasterKey id %u", nID);
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
            if (!pwallet->LoadCryptedKey(vchPubKey, vchPrivKey)) {
                strErr = "Error reading wallet database: LoadCryptedKey failed";
                return false;
            }
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
        } else if (strType == "hdpassphrase") {
            std::string p;
            ssValue >> p;
            pwallet->LoadHDPassphrase(p);
        } else if (strType == "hdcpassphrase") {
            std::pair<uint256, std::vector<unsigned char>> cp;
            ssValue >> cp;
            pwallet->LoadCryptedHDPassphrase(cp.first, cp.second);
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
            if (!pwallet->LoadCScript(script)) {
                strErr = "Error reading wallet database: LoadCScript failed";
                return false;
            }
        } else if (strType == "orderposnext") {
            ssValue >> pwallet->nOrderPosNext;
        }
    } catch (...) {
        return false;
    }
    return true;
}

DBErrors CWalletDB::LoadWallet(CWallet* pwallet)
{
    pwallet->vchDefaultKey = CPubKey();
    CWalletScanState wss;
    bool fNoncriticalErrors = false;
    DBErrors result = DB_LOAD_OK;

    try {
        LOCK(pwallet->cs_wallet);
        int nMinVersion = 0;
        if (Read((std::string)"minversion", nMinVersion)) {
            if (nMinVersion > CLIENT_VERSION)
                return DB_TOO_NEW;
            pwallet->LoadMinVersion(nMinVersion);
        }

        // Full scan over the keyspace via the typed seam. Range-seek call
        // sites filter inside ReadKeyValue's switch by strType, so a flat scan
        // visits every record exactly as the Berkeley DB_NEXT cursor did.
        auto cursor = StartCursor();
        if (!cursor) {
            printf("Error getting wallet database cursor\n");
            return DB_CORRUPT;
        }

        while (true) {
            CDataStream ssKey(SER_DISK, CLIENT_VERSION);
            CDataStream ssValue(SER_DISK, CLIENT_VERSION);
            bool fError = false;
            if (!NextRecord(*cursor, ssKey, ssValue, fError)) {
                if (fError) {
                    printf("Error reading next record from wallet database\n");
                    return DB_CORRUPT;
                }
                break; // DONE
            }

            std::string strType, strErr;
            if (!ReadKeyValue(pwallet, ssKey, ssValue, wss, strType, strErr)) {
                if (IsKeyType(strType))
                    result = DB_CORRUPT;
                else {
                    fNoncriticalErrors = true;
                    if (strType == "tx")
                        SoftSetBoolArg("-rescan", true);
                }
            }
            if (!strErr.empty())
                printf("%s\n", strErr.c_str());
        }
    } catch (...) {
        result = DB_CORRUPT;
    }

    if (fNoncriticalErrors && result == DB_LOAD_OK)
        result = DB_NONCRITICAL_ERROR;

    if (result != DB_LOAD_OK)
        return result;

    printf("nFileVersion = %d\n", wss.nFileVersion);
    printf("Keys: %u plaintext, %u encrypted, %u w/ metadata, %u total\n",
           wss.nKeys, wss.nCKeys, wss.nKeyMeta, wss.nKeys + wss.nCKeys);

    if ((wss.nKeys + wss.nCKeys) != wss.nKeyMeta)
        pwallet->nTimeFirstKey = 1;

    for (uint256 hash : wss.vWalletUpgrade)
        WriteTx(hash, pwallet->mapWallet[hash]);

    if (wss.fIsEncrypted && (wss.nFileVersion == 40000 || wss.nFileVersion == 50000))
        return DB_NEED_REWRITE;

    if (wss.nFileVersion < CLIENT_VERSION)
        WriteVersion(CLIENT_VERSION);

    if (wss.fAnyUnordered)
        result = ReorderTransactions(pwallet);

    return result;
}

void ThreadFlushWalletDB(void* parg)
{
    RenameThread("Triangles-wallet");

    const std::string& strFile = ((const std::string*)parg)[0];
    static bool fOneThread;
    if (fOneThread)
        return;
    fOneThread = true;
    if (!GetBoolArg("-flushwallet", true))
        return;

    // With SQLite the per-commit synchronous=FULL already guarantees durability,
    // so the periodic Berkeley flush path is unnecessary. We still bump
    // nLastFlushed against nWalletDBUpdated to keep the counter observable in
    // logs, but no I/O is performed against the wallet.
    if (ResolveWalletDbKind() == WalletDbKind::SQLite) {
        unsigned int nLastSeen = nWalletDBUpdated;
        unsigned int nLastFlushed = nWalletDBUpdated;
        while (!fShutdown) {
            MilliSleep(500);
            if (nLastSeen != nWalletDBUpdated) {
                nLastSeen = nWalletDBUpdated;
            }
            if (nLastFlushed != nWalletDBUpdated) {
                nLastFlushed = nWalletDBUpdated;
                // No-op: SQLite WAL/checkpoint is internal to the connection.
            }
        }
        return;
    }

    // Legacy Berkeley path — kept for the unlikely case that someone is still
    // running with -walletdb=bdb before the adapter is finished. The new
    // factory currently rejects -walletdb=bdb, so this branch is effectively
    // unreachable in this build, but is preserved to keep the function shape
    // intact for any future BerkeleyDatabase adapter.
    unsigned int nLastSeen = nWalletDBUpdated;
    unsigned int nLastFlushed = nWalletDBUpdated;
    int64_t nLastWalletUpdate = GetTime();
    while (!fShutdown) {
        MilliSleep(500);
        if (nLastSeen != nWalletDBUpdated) {
            nLastSeen = nWalletDBUpdated;
            nLastWalletUpdate = GetTime();
        }
        if (nLastFlushed != nWalletDBUpdated && GetTime() - nLastWalletUpdate >= 2) {
            TRY_LOCK(bitdb.cs_db, lockDb);
            if (lockDb) {
                int nRefCount = 0;
                std::map<std::string, int>::iterator mi = bitdb.mapFileUseCount.begin();
                while (mi != bitdb.mapFileUseCount.end()) {
                    nRefCount += mi->second;
                    mi++;
                }
                if (nRefCount == 0 && !fShutdown) {
                    auto mi = bitdb.mapFileUseCount.find(strFile);
                    if (mi != bitdb.mapFileUseCount.end()) {
                        printf("Flushing wallet.dat\n");
                        nLastFlushed = nWalletDBUpdated;
                        int64_t nStart = GetTimeMillis();
                        fs::path walletPath = GetDataDir() / strFile;
                        AutoBackupWallet(walletPath);
                        bitdb.CloseDb(strFile);
                        bitdb.CheckpointLSN(strFile);
                        bitdb.mapFileUseCount.erase(mi++);
                        printf("Flushed wallet.dat %" PRId64 "ms\n", GetTimeMillis() - nStart);
                    }
                }
            }
        }
    }
}

bool BackupWallet(const CWallet& wallet, const std::string& strDest)
{
    if (!wallet.fFileBacked)
        return false;

    // For the SQLite backend, the database is a single file — copy directly
    // (after a checkpoint flush to fold any -wal into the main file).
    if (ResolveWalletDbKind() == WalletDbKind::SQLite) {
        fs::path pathSrc = GetDataDir() / wallet.strWalletFile;
        fs::path pathDest(strDest);
        if (fs::is_directory(pathDest))
            pathDest /= wallet.strWalletFile;
        std::error_code ec;
        fs::copy_file(pathSrc, pathDest, fs::copy_options::overwrite_existing, ec);
        if (ec) {
            printf("error copying wallet.dat to %s - %s\n",
                   pathDest.string().c_str(), ec.message().c_str());
            return false;
        }
        printf("copied wallet.dat to %s\n", pathDest.string().c_str());
        return true;
    }

    // Legacy Berkeley flush-then-copy path. See ThreadFlushWalletDB for the
    // corresponding periodic flush logic.
    while (!fShutdown) {
        {
            LOCK(bitdb.cs_db);
            if (!bitdb.mapFileUseCount.count(wallet.strWalletFile) ||
                bitdb.mapFileUseCount[wallet.strWalletFile] == 0) {
                bitdb.CloseDb(wallet.strWalletFile);
                bitdb.CheckpointLSN(wallet.strWalletFile);
                bitdb.mapFileUseCount.erase(wallet.strWalletFile);

                fs::path pathSrc = GetDataDir() / wallet.strWalletFile;
                fs::path pathDest(strDest);
                if (fs::is_directory(pathDest))
                    pathDest /= wallet.strWalletFile;

                try {
                    fs::copy_file(pathSrc, pathDest, fs::copy_options::overwrite_existing);
                    printf("copied wallet.dat to %s\n", pathDest.string().c_str());
                    return true;
                } catch (const fs::filesystem_error& e) {
                    printf("error copying wallet.dat to %s - %s\n",
                           pathDest.string().c_str(), e.what());
                    return false;
                }
            }
        }
        MilliSleep(100);
    }
    return false;
}