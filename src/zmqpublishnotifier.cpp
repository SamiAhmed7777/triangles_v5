// Copyright (c) 2015 The Bitcoin Core developers
// Copyright (c) 2026 The Triangles developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifdef ENABLE_ZMQ

#include "zmqpublishnotifier.h"
#include "main.h"
#include "serialize.h"
#include "util.h"

#include <zmq.h>
#include <string.h>

CZMQPublishNotifier* pzmqNotifier = NULL;

CZMQPublishNotifier::CZMQPublishNotifier()
    : pcontext(NULL), psocket(NULL), fInitialized(false)
{
}

CZMQPublishNotifier::~CZMQPublishNotifier()
{
    Shutdown();
}

bool CZMQPublishNotifier::Initialize(const std::string& addr)
{
    address = addr;
    pcontext = zmq_ctx_new();
    if (!pcontext)
    {
        printf("ZMQ: Failed to create context\n");
        return false;
    }

    psocket = zmq_socket(pcontext, ZMQ_PUB);
    if (!psocket)
    {
        printf("ZMQ: Failed to create socket\n");
        zmq_ctx_destroy(pcontext);
        pcontext = NULL;
        return false;
    }

    int rc = zmq_bind(psocket, address.c_str());
    if (rc != 0)
    {
        printf("ZMQ: Failed to bind to %s: %s\n", address.c_str(), zmq_strerror(errno));
        zmq_close(psocket);
        zmq_ctx_destroy(pcontext);
        psocket = NULL;
        pcontext = NULL;
        return false;
    }

    printf("ZMQ: Publishing notifications on %s\n", address.c_str());
    fInitialized = true;
    return true;
}

void CZMQPublishNotifier::Shutdown()
{
    if (psocket)
    {
        zmq_close(psocket);
        psocket = NULL;
    }
    if (pcontext)
    {
        zmq_ctx_destroy(pcontext);
        pcontext = NULL;
    }
    fInitialized = false;
}

bool CZMQPublishNotifier::NotifyBlockHash(const uint256& hash)
{
    if (!fInitialized) return false;

    const char* topic = "hashblock";
    zmq_send(psocket, topic, strlen(topic), ZMQ_SNDMORE);
    zmq_send(psocket, hash.begin(), 32, 0);
    return true;
}

bool CZMQPublishNotifier::NotifyBlock(const CBlock& block)
{
    if (!fInitialized) return false;

    uint256 hash = block.GetHash();
    NotifyBlockHash(hash);

    // Also publish raw block
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << block;
    const char* topic = "rawblock";
    zmq_send(psocket, topic, strlen(topic), ZMQ_SNDMORE);
    zmq_send(psocket, &(*ss.begin()), ss.size(), 0);
    return true;
}

bool CZMQPublishNotifier::NotifyTransactionHash(const uint256& hash)
{
    if (!fInitialized) return false;

    const char* topic = "hashtx";
    zmq_send(psocket, topic, strlen(topic), ZMQ_SNDMORE);
    zmq_send(psocket, hash.begin(), 32, 0);
    return true;
}

bool CZMQPublishNotifier::NotifyTransaction(const CTransaction& tx)
{
    if (!fInitialized) return false;

    uint256 hash = tx.GetHash();
    NotifyTransactionHash(hash);

    // Also publish raw tx
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << tx;
    const char* topic = "rawtx";
    zmq_send(psocket, topic, strlen(topic), ZMQ_SNDMORE);
    zmq_send(psocket, &(*ss.begin()), ss.size(), 0);
    return true;
}

#endif // ENABLE_ZMQ
