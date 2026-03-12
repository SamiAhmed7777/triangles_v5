// Copyright (c) 2015 The Bitcoin Core developers
// Copyright (c) 2026 The Triangles developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef TRIANGLES_ZMQ_PUBLISH_NOTIFIER_H
#define TRIANGLES_ZMQ_PUBLISH_NOTIFIER_H

#ifdef ENABLE_ZMQ

#include <string>

class CBlock;
class CTransaction;

class CZMQPublishNotifier
{
private:
    void* pcontext;
    void* psocket;
    std::string address;
    bool fInitialized;

public:
    CZMQPublishNotifier();
    ~CZMQPublishNotifier();

    bool Initialize(const std::string& address);
    void Shutdown();

    bool NotifyBlock(const CBlock& block);
    bool NotifyBlockHash(const uint256& hash);
    bool NotifyTransaction(const CTransaction& tx);
    bool NotifyTransactionHash(const uint256& hash);
};

extern CZMQPublishNotifier* pzmqNotifier;

#endif // ENABLE_ZMQ
#endif // TRIANGLES_ZMQ_PUBLISH_NOTIFIER_H
