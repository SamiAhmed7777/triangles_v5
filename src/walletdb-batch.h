// Copyright (c) 2026 The Triangles developers.
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// Typed, backend-agnostic wallet batch — the bridge between CWalletDB's typed
// record calls and the raw byte-level WalletBatch interface (walletdb-base.h).
//
// It reproduces the exact serialization behavior of the old Berkeley CDB
// (CDataStream with SER_DISK / CLIENT_VERSION), so the bytes written are
// identical regardless of backend and CWalletDB's call sites need only change
// their base class — the Read/Write/Erase/Exists template calls are unchanged.
//
// CWalletDB is intended to derive from CWalletBatchTyped (replacing `: public
// CDB`). The Berkeley cursor methods CWalletDB used directly (GetAtCursor,
// ReadAtCursor with DB_NEXT/DB_SET_RANGE) map onto StartCursor()/NextRecord()
// here, which iterate the whole keyspace; range-seek call sites filter in the
// loop, as the SQLite cursor does not support keyed range seeks.

#ifndef TRIANGLES_WALLETDB_BATCH_H
#define TRIANGLES_WALLETDB_BATCH_H

#include "walletdb-base.h"
#include "serialize.h"   // CDataStream, SER_DISK
#include "version.h"     // CLIENT_VERSION

#include <memory>
#include <stdexcept>
#include <string>

class CWalletBatchTyped
{
public:
    // Default-constructed handle is unusable until Open() runs. Subclasses
    // (CWalletDB) call Open() once they have opened a WalletDatabase.
    CWalletBatchTyped() = default;
    virtual ~CWalletBatchTyped() { Close(); }

    // Open a fresh batch against the given database. Closes any previously
    // open batch+database. Returns false (and leaves the handle null) if the
    // database fails to produce a batch.
    bool Open(std::unique_ptr<WalletDatabase> db)
    {
        Close();
        if (!db)
            return false;
        m_database = std::move(db);
        m_batch = m_database->MakeBatch(/*flush_on_close=*/true);
        if (!m_batch) {
            m_database.reset();
            return false;
        }
        return true;
    }

    void Close()
    {
        m_batch.reset();
        m_database.reset();
    }
    bool IsNull() const { return m_batch == nullptr; }

    // ── Transactions ─────────────────────────────────────────────────────────
    bool TxnBegin()  { return m_batch && m_batch->TxnBegin(); }
    bool TxnCommit() { return m_batch && m_batch->TxnCommit(); }
    bool TxnAbort()  { return m_batch && m_batch->TxnAbort(); }

protected:
    std::unique_ptr<WalletDatabase> m_database;
    std::unique_ptr<WalletBatch> m_batch;

    // ── Typed accessors (serialize key/value, dispatch to the raw batch) ──────
    template <typename K, typename T>
    bool Read(const K& key, T& value)
    {
        if (!m_batch) return false;
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.reserve(1000);
        ssKey << key;
        KeyBytes vKey(ssKey.begin(), ssKey.end());

        ValueBytes vValue;
        if (!m_batch->ReadKey(vKey, vValue))
            return false;
        try {
            CDataStream ssValue(reinterpret_cast<const char*>(vValue.data()),
                                reinterpret_cast<const char*>(vValue.data()) + vValue.size(),
                                SER_DISK, CLIENT_VERSION);
            ssValue >> value;
        } catch (const std::exception&) {
            return false;
        }
        return true;
    }

    template <typename K, typename T>
    bool Write(const K& key, const T& value, bool fOverwrite = true)
    {
        if (!m_batch) return false;
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.reserve(1000);
        ssKey << key;
        KeyBytes vKey(ssKey.begin(), ssKey.end());

        CDataStream ssValue(SER_DISK, CLIENT_VERSION);
        ssValue.reserve(10000);
        ssValue << value;
        ValueBytes vValue(ssValue.begin(), ssValue.end());

        return m_batch->WriteKey(vKey, vValue, fOverwrite);
    }

    template <typename K>
    bool Erase(const K& key)
    {
        if (!m_batch) return false;
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.reserve(1000);
        ssKey << key;
        KeyBytes vKey(ssKey.begin(), ssKey.end());
        return m_batch->EraseKey(vKey);
    }

    template <typename K>
    bool Exists(const K& key)
    {
        if (!m_batch) return false;
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.reserve(1000);
        ssKey << key;
        KeyBytes vKey(ssKey.begin(), ssKey.end());
        return m_batch->HasKey(vKey);
    }

    // ── Cursor ────────────────────────────────────────────────────────────────
    // Replaces CDB::GetCursor()/ReadAtCursor(). Open a cursor, then call
    // NextRecord() repeatedly: returns true and fills the streams while records
    // remain, false at end-of-data, and sets fError on failure.
    std::unique_ptr<WalletCursor> StartCursor()
    {
        if (!m_batch) return nullptr;
        return m_batch->GetNewCursor();
    }

    bool NextRecord(WalletCursor& cursor, CDataStream& ssKey, CDataStream& ssValue, bool& fError)
    {
        fError = false;
        KeyBytes vKey;
        ValueBytes vValue;
        switch (cursor.Next(vKey, vValue)) {
        case WalletCursorStatus::MORE:
            ssKey.SetType(SER_DISK);
            ssKey.clear();
            ssKey.write(reinterpret_cast<const char*>(vKey.data()), vKey.size());
            ssValue.SetType(SER_DISK);
            ssValue.clear();
            ssValue.write(reinterpret_cast<const char*>(vValue.data()), vValue.size());
            return true;
        case WalletCursorStatus::DONE:
            return false;
        case WalletCursorStatus::FAIL:
        default:
            fError = true;
            return false;
        }
    }
};

#endif // TRIANGLES_WALLETDB_BATCH_H
