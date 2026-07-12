// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "main.h"
#include "net.h"
#include "init.h"
#include "trianglesrpc.h"
#include "addressindex.h"
#include "txdb.h"
#include "base58.h"
#include "utxosnapshot.h"
#include "checkpointpublisher.h"
#include "wallet.h"
#include "bootstrap.h"

#include <filesystem>

// Windows.h (transitively included) defines these as macros, clobbering Checkpoints:: enum values.
#ifdef STRICT
#undef STRICT
#endif
#ifdef ADVISORY
#undef ADVISORY
#endif
#ifdef PERMISSIVE
#undef PERMISSIVE
#endif

using namespace json_spirit;
using namespace std;

extern void TxToJSON(const CTransaction& tx, const uint256 hashBlock, json_spirit::Object& entry);
extern enum Checkpoints::CPMode CheckpointsMode;

double GetDifficulty(const CBlockIndex* blockindex)
{
    // Floating point number that is a multiple of the minimum difficulty,
    // minimum difficulty = 1.0.
    if (blockindex == nullptr)
    {
        if (pindexBest == nullptr)
            return 1.0;
        else
            blockindex = GetLastBlockIndex(pindexBest, false);
    }

    if (blockindex == nullptr)
        return 1.0;

    int nShift = (blockindex->nBits >> 24) & 0xff;

    double dDiff =
        (double)0x0000ffff / (double)(blockindex->nBits & 0x00ffffff);

    while (nShift < 29)
    {
        dDiff *= 256.0;
        nShift++;
    }
    while (nShift > 29)
    {
        dDiff /= 256.0;
        nShift--;
    }

    return dDiff;
}

double GetPoWMHashPS()
{
    if (pindexBest->nHeight >= CUTOFF_POW_BLOCK)
        return 0;

    int nPoWInterval = 72;
    int64_t nTargetSpacingWorkMin = 30, nTargetSpacingWork = 30;

    CBlockIndex* pindex = pindexGenesisBlock;
    CBlockIndex* pindexPrevWork = pindexGenesisBlock;

    while (pindex)
    {
        if (pindex->IsProofOfWork())
        {
            int64_t nActualSpacingWork = pindex->GetBlockTime() - pindexPrevWork->GetBlockTime();
            nTargetSpacingWork = ((nPoWInterval - 1) * nTargetSpacingWork + nActualSpacingWork + nActualSpacingWork) / (nPoWInterval + 1);
            nTargetSpacingWork = max(nTargetSpacingWork, nTargetSpacingWorkMin);
            pindexPrevWork = pindex;
        }

        pindex = pindex->pnext;
    }

    return GetDifficulty() * 4294.967296 / nTargetSpacingWork;
}

double GetPoSKernelPS()
{
    int nPoSInterval = 72;
    double dStakeKernelsTriedAvg = 0;
    int nStakesHandled = 0, nStakesTime = 0;

    CBlockIndex* pindex = pindexBest;;
    CBlockIndex* pindexPrevStake = nullptr;

    while (pindex && nStakesHandled < nPoSInterval)
    {
        if (pindex->IsProofOfStake())
        {
            dStakeKernelsTriedAvg += GetDifficulty(pindex) * 4294967296.0;
            nStakesTime += pindexPrevStake ? (pindexPrevStake->nTime - pindex->nTime) : 0;
            pindexPrevStake = pindex;
            nStakesHandled++;
        }

        pindex = pindex->pprev;
    }

    return nStakesTime ? dStakeKernelsTriedAvg / nStakesTime : 0;
}

Object blockToJSON(const CBlock& block, const CBlockIndex* blockindex, bool fPrintTransactionDetail)
{
    Object result;
    result.push_back(Pair("hash", block.GetHash().GetHex()));
    CMerkleTx txGen(block.vtx[0]);
    txGen.SetMerkleBranch(&block);
    result.push_back(Pair("confirmations", (int)txGen.GetDepthInMainChain()));
    result.push_back(Pair("size", (int)::GetSerializeSize(block, SER_NETWORK, PROTOCOL_VERSION)));
    result.push_back(Pair("height", blockindex->nHeight));
    result.push_back(Pair("version", block.nVersion));
    result.push_back(Pair("merkleroot", block.hashMerkleRoot.GetHex()));
    result.push_back(Pair("mint", ValueFromAmount(blockindex->nMint)));
    result.push_back(Pair("time", (int64_t)block.GetBlockTime()));
    result.push_back(Pair("nonce", (uint64_t)block.nNonce));
    result.push_back(Pair("bits", HexBits(block.nBits)));
    result.push_back(Pair("difficulty", GetDifficulty(blockindex)));
    result.push_back(Pair("blocktrust", leftTrim(blockindex->GetBlockTrust().GetHex(), '0')));
    result.push_back(Pair("chaintrust", leftTrim(blockindex->nChainTrust.GetHex(), '0')));
    if (blockindex->pprev)
        result.push_back(Pair("previousblockhash", blockindex->pprev->GetBlockHash().GetHex()));
    if (blockindex->pnext)
        result.push_back(Pair("nextblockhash", blockindex->pnext->GetBlockHash().GetHex()));

    result.push_back(Pair("flags", strprintf("%s%s", blockindex->IsProofOfStake()? "proof-of-stake" : "proof-of-work", blockindex->GeneratedStakeModifier()? " stake-modifier": "")));
    result.push_back(Pair("proofhash", blockindex->IsProofOfStake()? blockindex->hashProofOfStake.GetHex() : blockindex->GetBlockHash().GetHex()));
    result.push_back(Pair("entropybit", (int)blockindex->GetStakeEntropyBit()));
    result.push_back(Pair("modifier", strprintf("%016"PRIx64, blockindex->nStakeModifier)));
    result.push_back(Pair("modifierchecksum", strprintf("%08x", blockindex->nStakeModifierChecksum)));
    Array txinfo;
    for (const CTransaction& tx : block.vtx)
    {
        if (fPrintTransactionDetail)
        {
            Object entry;

            entry.push_back(Pair("txid", tx.GetHash().GetHex()));
            TxToJSON(tx, 0, entry);

            txinfo.push_back(entry);
        }
        else
            txinfo.push_back(tx.GetHash().GetHex());
    }

    result.push_back(Pair("tx", txinfo));

    if (block.IsProofOfStake())
        result.push_back(Pair("signature", HexStr(block.vchBlockSig.begin(), block.vchBlockSig.end())));

    return result;
}

Value getbestblockhash(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getbestblockhash\n"
            "Returns the hash of the best block in the longest block chain.");

    return hashBestChain.GetHex();
}

Value getblockcount(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getblockcount\n"
            "Returns the number of blocks in the longest block chain.");

    return nBestHeight;
}


Value getdifficulty(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getdifficulty\n"
            "Returns the difficulty as a multiple of the minimum difficulty.");

    Object obj;
    obj.push_back(Pair("proof-of-work",        GetDifficulty()));
    obj.push_back(Pair("proof-of-stake",       GetDifficulty(GetLastBlockIndex(pindexBest, true))));
    obj.push_back(Pair("search-interval",      (int)nLastCoinStakeSearchInterval));
    return obj;
}


Value settxfee(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 1 || AmountFromValue(params[0]) < MIN_TX_FEE)
        throw runtime_error(
            "settxfee <amount>\n"
            "<amount> is a real and is rounded to the nearest 0.01");

    nTransactionFee = AmountFromValue(params[0]);
    nTransactionFee = (nTransactionFee / CENT) * CENT;  // round to cent

    return true;
}

Value getrawmempool(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getrawmempool\n"
            "Returns all transaction ids in memory pool.");

    vector<uint256> vtxid;
    mempool.queryHashes(vtxid);

    Array a;
    for (const uint256& hash : vtxid)
        a.push_back(hash.ToString());

    return a;
}

Value getblockhash(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "getblockhash <index>\n"
            "Returns hash of block in best-block-chain at <index>.");

    int nHeight = params[0].get_int();
    if (nHeight < 0 || nHeight > nBestHeight)
        throw runtime_error("Block number out of range.");

    CBlockIndex* pblockindex = FindBlockByHeight(nHeight);
    if (!pblockindex || !pblockindex->phashBlock)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block height not available in local block index");
    return pblockindex->phashBlock->GetHex();
}

Value getblock(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "getblock <hash> [txinfo]\n"
            "txinfo optional to print more detailed tx info\n"
            "Returns details of a block with given block-hash.");

    std::string strHash = params[0].get_str();
    uint256 hash(strHash);

    if (mapBlockIndex.count(hash) == 0)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found");

    CBlock block;
    CBlockIndex* pblockindex = mapBlockIndex[hash];
    block.ReadFromDisk(pblockindex, true);

    return blockToJSON(block, pblockindex, params.size() > 1 ? params[1].get_bool() : false);
}

Value getblockbynumber(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "getblock <number> [txinfo]\n"
            "txinfo optional to print more detailed tx info\n"
            "Returns details of a block with given block-number.");

    int nHeight = params[0].get_int();
    if (nHeight < 0 || nHeight > nBestHeight)
        throw runtime_error("Block number out of range.");

    CBlockIndex* pblockindex = FindBlockByHeight(nHeight);
    if (!pblockindex || !pblockindex->phashBlock)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block height not available in local block index");

    CBlock block;
    block.ReadFromDisk(pblockindex, true);

    return blockToJSON(block, pblockindex, params.size() > 1 ? params[1].get_bool() : false);
}

Value getblockheader(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "getblockheader <hash> [verbose=true]\n"
            "If verbose is false, returns hex-encoded block header.\n"
            "If verbose is true, returns an Object with block header information.");

    std::string strHash = params[0].get_str();
    uint256 hash(strHash);

    if (mapBlockIndex.count(hash) == 0)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found");

    CBlockIndex* pblockindex = mapBlockIndex[hash];

    bool fVerbose = true;
    if (params.size() > 1)
        fVerbose = params[1].get_bool();

    if (!fVerbose)
    {
        CBlock blockHeader = pblockindex->GetBlockHeader();
        CDataStream ssBlock(SER_NETWORK, PROTOCOL_VERSION);
        ssBlock << blockHeader;
        return HexStr(ssBlock.begin(), ssBlock.end());
    }

    Object result;
    result.push_back(Pair("hash", pblockindex->GetBlockHash().GetHex()));
    result.push_back(Pair("confirmations", pindexBest->nHeight - pblockindex->nHeight + 1));
    result.push_back(Pair("height", pblockindex->nHeight));
    result.push_back(Pair("version", pblockindex->nVersion));
    result.push_back(Pair("merkleroot", pblockindex->hashMerkleRoot.GetHex()));
    result.push_back(Pair("mint", ValueFromAmount(pblockindex->nMint)));
    result.push_back(Pair("time", (int64_t)pblockindex->GetBlockTime()));
    result.push_back(Pair("nonce", (uint64_t)pblockindex->nNonce));
    result.push_back(Pair("bits", HexBits(pblockindex->nBits)));
    result.push_back(Pair("difficulty", GetDifficulty(pblockindex)));
    result.push_back(Pair("blocktrust", leftTrim(pblockindex->GetBlockTrust().GetHex(), '0')));
    result.push_back(Pair("chaintrust", leftTrim(pblockindex->nChainTrust.GetHex(), '0')));
    result.push_back(Pair("flags", strprintf("%s%s",
        pblockindex->IsProofOfStake() ? "proof-of-stake" : "proof-of-work",
        pblockindex->GeneratedStakeModifier() ? " stake-modifier" : "")));
    result.push_back(Pair("proofhash", pblockindex->IsProofOfStake() ? pblockindex->hashProofOfStake.GetHex() : pblockindex->GetBlockHash().GetHex()));
    result.push_back(Pair("entropybit", (int)pblockindex->GetStakeEntropyBit()));
    result.push_back(Pair("modifier", strprintf("%016"PRIx64, pblockindex->nStakeModifier)));
    result.push_back(Pair("modifierchecksum", strprintf("%08x", pblockindex->nStakeModifierChecksum)));
    if (pblockindex->pprev)
        result.push_back(Pair("previousblockhash", pblockindex->pprev->GetBlockHash().GetHex()));
    if (pblockindex->pnext)
        result.push_back(Pair("nextblockhash", pblockindex->pnext->GetBlockHash().GetHex()));

    return result;
}

Value estimatefee(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "estimatefee <nblocks>\n"
            "Returns an estimated fee per kilobyte for a transaction to be\n"
            "confirmed within nblocks blocks.\n"
            "Triangles uses a static minimum fee structure.");

    return ValueFromAmount(nTransactionFee > 0 ? nTransactionFee : MIN_TX_FEE);
}

Value gettxoutsetinfo(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "gettxoutsetinfo\n"
            "Returns statistics about the unspent transaction output set.");

    Object obj;
    obj.push_back(Pair("height", (int)nBestHeight));
    obj.push_back(Pair("bestblock", hashBestChain.GetHex()));
    obj.push_back(Pair("total_amount", ValueFromAmount(pindexBest->nMoneySupply)));
    return obj;
}

static void GetActiveChainVector(std::vector<CBlockIndex*>& chain)
{
    chain.clear();

    if (!pindexBest)
        throw runtime_error("recalculatesupply: no best block");

    for (CBlockIndex* pindex = pindexBest; pindex; pindex = pindex->pprev)
        chain.push_back(pindex);

    std::reverse(chain.begin(), chain.end());
}

static int64_t ComputeActiveChainSupplyFromBlocks(const std::vector<CBlockIndex*>& chain, int& nBlocksScanned, int& nTransactionsScanned)
{
    nBlocksScanned = 0;
    nTransactionsScanned = 0;

    auto txdb_holder = MakeChainDB("r"); CTxDBBase& txdb = *txdb_holder;
    int64_t nSupply = 0;

    for (std::vector<CBlockIndex*>::const_iterator pindexIt = chain.begin(); pindexIt != chain.end(); ++pindexIt)
    {
        CBlockIndex* pindex = *pindexIt;
        if (!pindex)
            throw runtime_error("recalculatesupply: null active-chain block index");

        if (pindex->nHeight == 0)
        {
            nBlocksScanned++;
            continue;
        }

        CBlock block;
        int64_t nBlockValueIn = 0;
        int64_t nBlockValueOut = 0;

        if (!block.ReadFromDisk(pindex))
            throw runtime_error(strprintf("recalculatesupply: failed reading block at height %d", pindex->nHeight));

        for (std::vector<CTransaction>::const_iterator txIt = block.vtx.begin(); txIt != block.vtx.end(); ++txIt)
        {
            const CTransaction& tx = *txIt;
            nTransactionsScanned++;
            nBlockValueOut += tx.GetValueOut();

            if (!tx.IsCoinBase())
            {
                for (std::vector<CTxIn>::const_iterator txinIt = tx.vin.begin(); txinIt != tx.vin.end(); ++txinIt)
                {
                    const CTxIn& txin = *txinIt;
                    CTxIndex txindex;
                    CTransaction txPrev;
                    if (!txPrev.ReadFromDisk(txdb, txin.prevout, txindex))
                        throw runtime_error(strprintf(
                            "recalculatesupply: failed reading prevout %s:%u while processing height %d",
                            txin.prevout.hash.ToString().c_str(), txin.prevout.n, pindex->nHeight));

                    if (txin.prevout.n >= txPrev.vout.size())
                        throw runtime_error(strprintf(
                            "recalculatesupply: prevout index %u out of range for tx %s at height %d",
                            txin.prevout.n, txin.prevout.hash.ToString().c_str(), pindex->nHeight));

                    nBlockValueIn += txPrev.vout[txin.prevout.n].nValue;
                }
            }
        }

        nSupply += (nBlockValueOut - nBlockValueIn);
        nBlocksScanned++;
    }

    return nSupply;
}

Value recalculatesupply(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
            "recalculatesupply [apply=false]\n"
            "Rebuilds money supply by walking the active chain from genesis and summing (valueOut - valueIn) per block.\n"
            "Also returns the current UTXO-set total for comparison.\n"
            "If apply=true, rewrites nMoneySupply for every block on the active chain and persists the repaired values.\n"
            "\nThis is intended for repairing corrupted money-supply tracking after chain/index incidents.");

    bool fApply = false;
    if (params.size() == 1)
        fApply = params[0].get_bool();

    LOCK(cs_main);

    if (!pindexBest)
        throw runtime_error("recalculatesupply: no best block");

    auto txdbRead_holder = MakeChainDB("r"); CTxDBBase& txdbRead = *txdbRead_holder;
    int nUtxoCount = 0;
    int64_t nUtxoSupply = txdbRead.SumUtxoValues(nUtxoCount);

    std::vector<CBlockIndex*> activeChain;
    GetActiveChainVector(activeChain);

    int nBlocksScanned = 0;
    int nTransactionsScanned = 0;
    int64_t nHistoricalSupply = ComputeActiveChainSupplyFromBlocks(activeChain, nBlocksScanned, nTransactionsScanned);

    int64_t nOldTipSupply = pindexBest->nMoneySupply;

    if (fApply)
    {
        auto txdbWrite_holder = MakeChainDB(); CTxDBBase& txdbWrite = *txdbWrite_holder;
        int64_t nRunningSupply = 0;

        for (std::vector<CBlockIndex*>::const_iterator pindexIt = activeChain.begin(); pindexIt != activeChain.end(); ++pindexIt)
        {
            CBlockIndex* pindex = *pindexIt;
            if (!pindex)
                throw runtime_error("recalculatesupply: null active-chain block index during apply");

            if (pindex->nHeight == 0)
            {
                pindex->nMoneySupply = 0;
                if (!txdbWrite.WriteBlockIndex(CDiskBlockIndex(pindex)))
                    throw runtime_error("recalculatesupply: failed to persist genesis block index during apply");
                continue;
            }

            CBlock block;
            int64_t nBlockValueIn = 0;
            int64_t nBlockValueOut = 0;

            if (!block.ReadFromDisk(pindex))
                throw runtime_error(strprintf("recalculatesupply: failed reading block at height %d during apply", pindex->nHeight));

            for (std::vector<CTransaction>::const_iterator txIt = block.vtx.begin(); txIt != block.vtx.end(); ++txIt)
            {
                const CTransaction& tx = *txIt;
                nBlockValueOut += tx.GetValueOut();

                if (!tx.IsCoinBase())
                {
                    for (std::vector<CTxIn>::const_iterator txinIt = tx.vin.begin(); txinIt != tx.vin.end(); ++txinIt)
                    {
                        const CTxIn& txin = *txinIt;
                        CTxIndex txindex;
                        CTransaction txPrev;
                        if (!txPrev.ReadFromDisk(txdbWrite, txin.prevout, txindex))
                            throw runtime_error(strprintf(
                                "recalculatesupply: failed reading prevout %s:%u during apply at height %d",
                                txin.prevout.hash.ToString().c_str(), txin.prevout.n, pindex->nHeight));
                        if (txin.prevout.n >= txPrev.vout.size())
                            throw runtime_error(strprintf(
                                "recalculatesupply: prevout index %u out of range during apply for tx %s at height %d",
                                txin.prevout.n, txin.prevout.hash.ToString().c_str(), pindex->nHeight));
                        nBlockValueIn += txPrev.vout[txin.prevout.n].nValue;
                    }
                }
            }

            nRunningSupply += (nBlockValueOut - nBlockValueIn);
            pindex->nMoneySupply = nRunningSupply;

            if (!txdbWrite.WriteBlockIndex(CDiskBlockIndex(pindex)))
                throw runtime_error(strprintf("recalculatesupply: failed to persist block index at height %d", pindex->nHeight));
        }
    }

    Object result;
    result.push_back(Pair("height", (int)nBestHeight));
    result.push_back(Pair("tip_bestblock", hashBestChain.GetHex()));
    result.push_back(Pair("old_tip_supply", ValueFromAmount(nOldTipSupply)));
    result.push_back(Pair("recalculated_chain_supply", ValueFromAmount(nHistoricalSupply)));
    result.push_back(Pair("utxo_supply", ValueFromAmount(nUtxoSupply)));
    result.push_back(Pair("tip_vs_recalculated", ValueFromAmount(nHistoricalSupply - nOldTipSupply)));
    result.push_back(Pair("utxo_vs_recalculated", ValueFromAmount(nHistoricalSupply - nUtxoSupply)));
    result.push_back(Pair("blocks_scanned", nBlocksScanned));
    result.push_back(Pair("transactions_scanned", nTransactionsScanned));
    result.push_back(Pair("utxo_count", nUtxoCount));
    result.push_back(Pair("applied", fApply));
    return result;
}

// Walks every non-coinbase transaction input across [start_height, end_height]
// and runs the existing VerifySignature path. Reports counts and the first 100
// failures so the caller can spot regressions when the underlying ECDSA
// implementation changes (e.g. OpenSSL EC -> libsecp256k1).
Value auditsignatures(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 2)
        throw runtime_error(
            "auditsignatures [start_height] [end_height]\n"
            "Walk the active chain in [start_height, end_height] (inclusive) and run\n"
            "VerifySignature on every non-coinbase input. Returns counts plus up to 100\n"
            "failures.\n"
            "Defaults: start = max(1, tip-1000), end = tip.\n"
            "Pre-migration this should always report 0 failures; post-migration any non-zero\n"
            "result identifies a behavioural regression in the new ECDSA path.");

    LOCK(cs_main);

    if (!pindexBest)
        throw runtime_error("auditsignatures: no best block");

    int tip = nBestHeight;
    int start = (params.size() > 0) ? params[0].get_int() : std::max(1, tip - 1000);
    int end   = (params.size() > 1) ? params[1].get_int() : tip;

    if (start < 1)        throw runtime_error("auditsignatures: start_height must be >= 1");
    if (end > tip)        throw runtime_error("auditsignatures: end_height exceeds tip");
    if (start > end)      throw runtime_error("auditsignatures: start_height > end_height");

    // Build forward walk by descending from tip.
    CBlockIndex* pindex = pindexBest;
    while (pindex && pindex->nHeight > end)
        pindex = pindex->pprev;

    std::vector<CBlockIndex*> walk;
    while (pindex && pindex->nHeight >= start) {
        walk.push_back(pindex);
        pindex = pindex->pprev;
    }
    std::reverse(walk.begin(), walk.end());

    int     nBlocksScanned  = 0;
    int64_t nInputsChecked  = 0;
    int64_t nInputsFailed   = 0;
    Array   failures;
    const size_t kMaxFailures = 100;

    auto recordFailure = [&](int height, const uint256& txid, unsigned int vin, const char* reason) {
        ++nInputsFailed;
        if (failures.size() >= kMaxFailures) return;
        Object f;
        f.push_back(Pair("height", height));
        f.push_back(Pair("txid",   txid.GetHex()));
        f.push_back(Pair("vin",    (int)vin));
        f.push_back(Pair("reason", reason));
        failures.push_back(f);
    };

    for (CBlockIndex* pi : walk) {
        CBlock block;
        if (!block.ReadFromDisk(pi, true)) {
            ++nBlocksScanned;
            continue;
        }
        for (const CTransaction& tx : block.vtx) {
            if (tx.IsCoinBase()) continue;
            for (unsigned int i = 0; i < tx.vin.size(); ++i) {
                const COutPoint& prev = tx.vin[i].prevout;
                CTransaction txFrom;
                uint256 hashBlock;
                if (!GetTransaction(prev.hash, txFrom, hashBlock)) {
                    recordFailure(pi->nHeight, tx.GetHash(), i, "prevout transaction not found");
                    continue;
                }
                if (prev.n >= txFrom.vout.size()) {
                    recordFailure(pi->nHeight, tx.GetHash(), i, "prevout index out of range");
                    continue;
                }
                ++nInputsChecked;
                if (!VerifySignature(txFrom, tx, i, 0))
                    recordFailure(pi->nHeight, tx.GetHash(), i, "VerifySignature returned false");
            }
        }
        ++nBlocksScanned;
        if (nBlocksScanned % 1000 == 0)
            printf("auditsignatures: scanned %d blocks, %lld inputs, %lld failures\n",
                   nBlocksScanned, (long long)nInputsChecked, (long long)nInputsFailed);
    }

    Object result;
    result.push_back(Pair("start_height",   start));
    result.push_back(Pair("end_height",     end));
    result.push_back(Pair("blocks_scanned", nBlocksScanned));
    result.push_back(Pair("inputs_checked", nInputsChecked));
    result.push_back(Pair("inputs_failed",  nInputsFailed));
    result.push_back(Pair("failures",       failures));
    return result;
}


// triangles: get information of sync-checkpoint
Value getcheckpoint(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getcheckpoint\n"
            "Show info of synchronized checkpoint.\n");

    Object result;
    CBlockIndex* pindexCheckpoint;

    result.push_back(Pair("synccheckpoint", Checkpoints::hashSyncCheckpoint.ToString().c_str()));
    pindexCheckpoint = mapBlockIndex[Checkpoints::hashSyncCheckpoint];
    result.push_back(Pair("height", pindexCheckpoint->nHeight));
    result.push_back(Pair("timestamp", DateTimeStrFormat(pindexCheckpoint->GetBlockTime()).c_str()));

    // Check that the block satisfies synchronized checkpoint
    if (CheckpointsMode == Checkpoints::STRICT)
        result.push_back(Pair("policy", "strict"));

    if (CheckpointsMode == Checkpoints::ADVISORY)
        result.push_back(Pair("policy", "advisory"));

    if (CheckpointsMode == Checkpoints::PERMISSIVE)
        result.push_back(Pair("policy", "permissive"));

    if (mapArgs.count("-checkpointkey"))
        result.push_back(Pair("checkpointmaster", true));

    return result;
}

Value getblockchaininfo(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getblockchaininfo\n"
            "Returns an object containing various state info regarding block chain processing.");

    Object obj, diff;
    obj.push_back(Pair("chain", fTestNet ? string("test") : string("main")));
    obj.push_back(Pair("blocks", (int)nBestHeight));
    obj.push_back(Pair("headers", (int)nBestHeight));
    obj.push_back(Pair("bestblockhash", hashBestChain.GetHex()));

    diff.push_back(Pair("proof-of-work", GetDifficulty()));
    diff.push_back(Pair("proof-of-stake", GetDifficulty(GetLastBlockIndex(pindexBest, true))));
    obj.push_back(Pair("difficulty", diff));

    obj.push_back(Pair("moneysupply", ValueFromAmount(pindexBest->nMoneySupply)));
    obj.push_back(Pair("timeoffset", (int64_t)GetTimeOffset()));
    obj.push_back(Pair("connections", (int)vNodes.size()));
    obj.push_back(Pair("errors", GetWarnings("statusbar")));
    return obj;
}

Value gencheckpoints(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
            "gencheckpoints [interval]\n"
            "Generates hardcoded checkpoint entries for checkpoints.cpp.\n"
            "Outputs C++ map entries for every <interval> blocks (default 5000)\n"
            "from genesis to current tip, ready to paste into the source code.");

    int nInterval = 5000;
    if (params.size() > 0)
        nInterval = params[0].get_int();
    if (nInterval < 1)
        throw runtime_error("Interval must be >= 1");

    std::string result;
    result += "// Generated by gencheckpoints RPC at height " + std::to_string(nBestHeight) + "\n";
    result += "static MapCheckpoints mapCheckpoints = {\n";

    // Always include genesis
    CBlockIndex* pindex = mapBlockIndex[hashBestChain];
    while (pindex->pprev)
        pindex = pindex->pprev;

    bool first = true;
    while (pindex)
    {
        if (pindex->nHeight % nInterval == 0 || pindex->nHeight == nBestHeight)
        {
            if (!first)
                result += ",\n";
            result += "    {" + std::to_string(pindex->nHeight) + ", uint256(\"0x"
                    + pindex->GetBlockHash().GetHex() + "\")}";
            first = false;
        }
        pindex = pindex->pnext;
    }
    result += "\n};\n";

    return result;
}

// publishcheckpoint [interval] [signing_address] [output_path]
//
// Builds a signed-checkpoints JSON document for every <interval> blocks
// from genesis to the current chain tip, signs it with the private key of
// <signing_address> (defaults to the wallet's default receiving address),
// and writes the result to <output_path> (defaults to the standard
// bootstrap server location).
//
// The output file is what gets uploaded to
// https://bootstrap.cryptographic-triangles.org/signed-checkpoints.json
// and consumed by the daemon's startup-time LoadSignedCheckpoints().
//
// Returns the full JSON document (so the operator can inspect it before
// uploading). Also writes it to disk so a cron-style uploader can pick it up.
//
// Example:
//   triangles-cli publishcheckpoint 5000 \
//       TG8f76yktTxDrT7JJymY3wVAusXiD3fVvX \
//       /var/www/triangles-bootstrap/signed-checkpoints.json
//
// The signing address MUST be in the trusted signers list at every node
// that consumes this document, or the document will be rejected at startup.
Value publishcheckpoint(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 3)
        throw runtime_error(
            "publishcheckpoint [interval] [signing_address] [output_path]\n"
            "Build a signed-checkpoints JSON document and optionally write it to disk.\n"
            "\nArguments:\n"
            "1. interval          (numeric, optional, default=5000)  blocks between checkpoints\n"
            "2. signing_address   (string, optional)                 wallet address to sign with (default: wallet default)\n"
            "3. output_path       (string, optional)                 where to write the JSON (default: bootstrap server path)\n"
            "\nResult:\n"
            "{ json: '...', path: '...', entries: N, signing_address: '...', sha256: '...' }\n"
            "\nThe 'signing_address' MUST be in every consumer's trusted signers list,\n"
            "otherwise the document will be rejected at startup.");

    if (!pwalletMain)
        throw JSONRPCError(RPC_WALLET_ERROR, "Wallet not loaded");

    // 1. Resolve signing address — explicit param wins; otherwise pull from
    //    the keypool (the wallet's stable receiving address). Operators can
    //    always override via the signing_address argument if they want to
    //    pin a specific key.
    std::string strSigningAddr;
    if (params.size() >= 2 && !params[1].get_str().empty()) {
        strSigningAddr = params[1].get_str();
    } else {
        CPubKey pubKey;
        if (!pwalletMain->GetKeyFromPool(pubKey, /*fAllowReuse=*/true)) {
            throw JSONRPCError(RPC_WALLET_ERROR,
                "publishcheckpoint: cannot determine default signing address — "
                "please specify explicitly via the signing_address argument");
        }
        strSigningAddr = CTrianglesAddress(pubKey.GetID()).ToString();
    }

    // 2. Validate signing address and resolve to key
    CTrianglesAddress addr(strSigningAddr);
    if (!addr.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY,
            "publishcheckpoint: invalid signing address " + strSigningAddr);
    CKeyID keyID;
    if (!addr.GetKeyID(keyID))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY,
            "publishcheckpoint: address does not refer to a key");
    EnsureWalletIsUnlocked();  // signmessage-style signing needs unlocked wallet
    CKey key;
    if (!pwalletMain->GetKey(keyID, key))
        throw JSONRPCError(RPC_WALLET_ERROR,
            "publishcheckpoint: private key for " + strSigningAddr + " not available");

    // 3. Resolve interval
    int nInterval = 5000;
    if (params.size() >= 1) {
        nInterval = params[0].get_int();
        if (nInterval < 1)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "interval must be >= 1");
    }

    // 4. Walk the chain backward from pindexBest, collecting checkpoints
    //    every <interval> blocks. Always include the chain tip (nBestHeight).
    std::vector<Checkpoints::SignedCheckpoint> entries;
    CBlockIndex* pindex = mapBlockIndex[hashBestChain];
    if (!pindex)
        throw JSONRPCError(RPC_INTERNAL_ERROR, "publishcheckpoint: no best chain");
    int64_t nNow = GetTime();
    while (pindex) {
        if (pindex->nHeight % nInterval == 0 || pindex == mapBlockIndex[hashBestChain]) {
            Checkpoints::SignedCheckpoint e;
            e.nHeight = pindex->nHeight;
            // Serialize hash as lowercase hex WITHOUT 0x prefix, no leading zeros
            e.hashHex = pindex->GetBlockHash().GetHex();
            e.nTimestamp = nNow;
            entries.push_back(e);
        }
        if (pindex->nHeight == 0) break;
        pindex = pindex->pprev;
    }
    if (entries.empty())
        throw JSONRPCError(RPC_INTERNAL_ERROR, "publishcheckpoint: no entries generated");

    // 5. Build the canonical message + sign it
    std::string message = Checkpoints::SerializeEntriesForSigning(entries);
    CDataStream ss(SER_GETHASH, 0);
    ss << strMessageMagic;
    ss << message;
    std::vector<unsigned char> vchSig;
    if (!key.SignCompact(Hash(ss.begin(), ss.end()), vchSig))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY,
            "publishcheckpoint: SignCompact failed");
    std::string sigBase64 = EncodeBase64(&vchSig[0], vchSig.size());

    // 6. Build the JSON document
    std::string json, buildErr;
    if (!Checkpoints::BuildSignedCheckpointsJson(
            entries, strSigningAddr, sigBase64, message, json, buildErr)) {
        throw JSONRPCError(RPC_INTERNAL_ERROR,
            "publishcheckpoint: BuildSignedCheckpointsJson failed: " + buildErr);
    }

    // 7. Self-verify before returning — defense in depth. If our own signed
    //    document doesn't verify, we want to know immediately rather than
    //    ship a bad document.
    std::vector<Checkpoints::SignedCheckpoint> verifyEntries;
    std::string verifySigner;
    std::string verifyErr;
    if (!Checkpoints::VerifySignedCheckpoints(json, verifyEntries, verifySigner, verifyErr)) {
        throw JSONRPCError(RPC_INTERNAL_ERROR,
            "publishcheckpoint: self-verification FAILED: " + verifyErr);
    }
    if (verifyEntries.size() != entries.size()) {
        throw JSONRPCError(RPC_INTERNAL_ERROR,
            "publishcheckpoint: self-verification returned wrong entry count");
    }

    // 8. Optionally write to disk
    std::string outPath;
    if (params.size() >= 3 && !params[2].get_str().empty()) {
        outPath = params[2].get_str();
    } else {
        outPath = Checkpoints::SIGNED_CHECKPOINTS_DEFAULT_OUT;
    }
    FILE* f = fopen(outPath.c_str(), "wb");
    if (f) {
        fwrite(json.data(), 1, json.size(), f);
        fclose(f);
        printf("publishcheckpoint: wrote %zu entries (%zu bytes) to %s\n",
               entries.size(), json.size(), outPath.c_str());
    } else {
        // Don't fail the RPC just because the disk write failed — the operator
        // still has the JSON in the response and can save it manually.
        printf("publishcheckpoint: WARNING — could not write to %s, returning JSON in response\n",
               outPath.c_str());
        outPath = "";
    }

    // 9. Compute a hex SHA256 of the JSON for operator verification
    //    (uses the standard util helper; available everywhere)
    std::string sha = Hash(reinterpret_cast<const unsigned char*>(json.data()),
                           reinterpret_cast<const unsigned char*>(json.data() + json.size())
                          ).ToString();

    Object result;
    result.push_back(Pair("entries", (int)entries.size()));
    result.push_back(Pair("signing_address", strSigningAddr));
    result.push_back(Pair("path", outPath));
    result.push_back(Pair("sha256", sha.substr(0, 16) + "..."));
    result.push_back(Pair("json", json));
    return result;
}

// ============================================================================
// Address index RPC commands
// ============================================================================

/**
 * Helper: parse an address string and return (nType, hashBytes).
 * Throws JSONRPCError on invalid address.
 */
static bool ParseAddress(const std::string& strAddr, int& nType, uint160& hashBytes)
{
    CTrianglesAddress addr(strAddr);
    if (!addr.IsValid())
        return false;

    CTxDestination dest = addr.Get();
    const CKeyID* keyId = std::get_if<CKeyID>(&dest);
    if (keyId) {
        nType = ADDR_TYPE_P2PKH;
        hashBytes = *keyId;
        return true;
    }
    const CScriptID* scriptId = std::get_if<CScriptID>(&dest);
    if (scriptId) {
        nType = ADDR_TYPE_P2SH;
        hashBytes = *scriptId;
        return true;
    }
    return false;
}

Value getaddressbalance(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "getaddressbalance {\"addresses\":[\"addr\",...]}\n"
            "Returns the balance for address(es).\n"
            "Requires -addressindex=1.\n"
            "\nArguments:\n"
            "1. {\"addresses\":[\"addr\",...]}  (object) JSON object with address array\n"
            "\nResult:\n"
            "{\n"
            "  \"balance\"   : n,      (numeric) The current balance in satoshis\n"
            "  \"received\"  : n       (numeric) The total received in satoshis\n"
            "}");

    if (!fAddressIndex)
        throw JSONRPCError(RPC_MISC_ERROR, "Address index not enabled. Start with -addressindex=1");

    Object addrObj = params[0].get_obj();
    Array addrArray = find_value(addrObj, "addresses").get_array();

    int64_t nTotalBalance = 0;

    for (unsigned int i = 0; i < addrArray.size(); i++)
    {
        std::string strAddr = addrArray[i].get_str();
        int nType;
        uint160 hashBytes;
        if (!ParseAddress(strAddr, nType, hashBytes))
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address: " + strAddr);

        int64_t nBalance = 0;
        auto txdb_holder = MakeChainDB("r"); CTxDBBase& txdb = *txdb_holder;
        txdb.ReadAddressBalance(nType, hashBytes, nBalance);
        nTotalBalance += nBalance;
    }

    Object result;
    result.push_back(Pair("balance", nTotalBalance));
    return result;
}

Value getaddressutxos(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "getaddressutxos {\"addresses\":[\"addr\",...]}\n"
            "Returns all unspent outputs for address(es).\n"
            "Requires -addressindex=1.\n"
            "\nArguments:\n"
            "1. {\"addresses\":[\"addr\",...]}  (object) JSON object with address array\n"
            "\nResult:\n"
            "[{\n"
            "  \"address\"  : \"addr\",  (string) The address\n"
            "  \"txid\"     : \"hash\",  (string) The transaction id\n"
            "  \"outputIndex\" : n,      (numeric) The output index\n"
            "  \"satoshis\" : n,         (numeric) The amount in satoshis\n"
            "  \"height\"   : n          (numeric) The block height\n"
            "},...]");

    if (!fAddressIndex)
        throw JSONRPCError(RPC_MISC_ERROR, "Address index not enabled. Start with -addressindex=1");

    Object addrObj = params[0].get_obj();
    Array addrArray = find_value(addrObj, "addresses").get_array();

    Array result;
    auto txdb_holder = MakeChainDB("r"); CTxDBBase& txdb = *txdb_holder;

    for (unsigned int i = 0; i < addrArray.size(); i++)
    {
        std::string strAddr = addrArray[i].get_str();
        int nType;
        uint160 hashBytes;
        if (!ParseAddress(strAddr, nType, hashBytes))
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address: " + strAddr);

        std::vector<std::pair<COutPoint, std::pair<int64_t, int> > > vUtxos;
        txdb.GetAddressUtxos(nType, hashBytes, vUtxos);

        for (unsigned int j = 0; j < vUtxos.size(); j++)
        {
            Object utxo;
            utxo.push_back(Pair("address", strAddr));
            utxo.push_back(Pair("txid", vUtxos[j].first.hash.GetHex()));
            utxo.push_back(Pair("outputIndex", (int)vUtxos[j].first.n));
            utxo.push_back(Pair("satoshis", vUtxos[j].second.first));
            utxo.push_back(Pair("height", vUtxos[j].second.second));
            result.push_back(utxo);
        }
    }

    return result;
}

Value getaddresstxids(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "getaddresstxids {\"addresses\":[\"addr\",...],\"start\":n,\"end\":n}\n"
            "Returns the transaction ids for address(es).\n"
            "Requires -addressindex=1.\n"
            "\nArguments:\n"
            "1. {\"addresses\":[\"addr\",...],  (object) JSON object\n"
            "    \"start\":n,                   (numeric, optional) Start block height (default 0)\n"
            "    \"end\":n}                     (numeric, optional) End block height (default current)\n"
            "\nResult:\n"
            "[\"txid\",...]  (array of strings) Transaction ids");

    if (!fAddressIndex)
        throw JSONRPCError(RPC_MISC_ERROR, "Address index not enabled. Start with -addressindex=1");

    Object addrObj = params[0].get_obj();
    Array addrArray = find_value(addrObj, "addresses").get_array();

    int nStartHeight = 0;
    int nEndHeight = nBestHeight;

    Value startVal = find_value(addrObj, "start");
    if (startVal.type() == int_type)
        nStartHeight = startVal.get_int();

    Value endVal = find_value(addrObj, "end");
    if (endVal.type() == int_type)
        nEndHeight = endVal.get_int();

    Array result;
    auto txdb_holder = MakeChainDB("r"); CTxDBBase& txdb = *txdb_holder;

    // Use a set to deduplicate txids across multiple addresses
    std::set<uint256> setTxIds;

    for (unsigned int i = 0; i < addrArray.size(); i++)
    {
        std::string strAddr = addrArray[i].get_str();
        int nType;
        uint160 hashBytes;
        if (!ParseAddress(strAddr, nType, hashBytes))
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address: " + strAddr);

        std::vector<uint256> vTxIds;
        txdb.GetAddressTxIds(nType, hashBytes, nStartHeight, nEndHeight, vTxIds);

        for (unsigned int j = 0; j < vTxIds.size(); j++)
        {
            if (setTxIds.insert(vTxIds[j]).second)
                result.push_back(vTxIds[j].GetHex());
        }
    }

    return result;
}

Value getchaintips(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getchaintips\n"
            "Return information about all known tips in the block tree,\n"
            "including the main chain as well as orphaned branches.\n"
            "Essential for diagnosing chain forks.");

    // Collect all block indices that are tips (nothing points to them as pprev)
    set<CBlockIndex*> setTips;

    {
        LOCK(cs_main);
        for (const auto& item : mapBlockIndex)
            setTips.insert(item.second);

        for (const auto& item : mapBlockIndex) {
            if (item.second->pprev)
                setTips.erase(item.second->pprev);
        }
    }

    Array res;
    LOCK(cs_main);
    for (CBlockIndex* tip : setTips)
    {
        Object obj;
        obj.push_back(Pair("height", tip->nHeight));
        obj.push_back(Pair("hash", tip->GetBlockHash().GetHex()));
        obj.push_back(Pair("chaintrust", tip->nChainTrust.GetHex()));

        int branchLen = 0;
        CBlockIndex* pWalk = tip;
        while (pWalk && !pWalk->IsInMainChain()) {
            branchLen++;
            pWalk = pWalk->pprev;
        }

        string status;
        if (tip == pindexBest)
            status = "active";
        else if (branchLen > 0)
            status = "valid-fork";
        else
            status = "unknown";

        obj.push_back(Pair("branchlen", branchLen));
        obj.push_back(Pair("status", status));

        if (pWalk && !tip->IsInMainChain())
            obj.push_back(Pair("forkpoint", pWalk->GetBlockHash().GetHex()));

        res.push_back(obj);
    }

    return res;
}

Value invalidateblock(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "invalidateblock <hash>\n"
            "Permanently marks a block as invalid and rewinds the chain.\n"
            "This forces the node to reorganize to the parent chain.\n"
            "Use reconsiderblock to undo.");

    string strHash = params[0].get_str();
    uint256 hash(strHash);

    LOCK(cs_main);

    if (mapBlockIndex.count(hash) == 0)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found");

    CBlockIndex* pindex = mapBlockIndex[hash];

    if (pindex->IsInMainChain())
    {
        auto txdb_holder = MakeChainDB(); CTxDBBase& txdb = *txdb_holder;
        if (!txdb.TxnBegin())
            throw runtime_error("Failed to begin transaction.");

        CBlockIndex* pindexWalk = pindexBest;

        // Disconnect blocks from best back to (but not including) pindex's parent
        while (pindexWalk && pindexWalk != pindex->pprev)
        {
            CBlock block;
            if (!block.ReadFromDisk(pindexWalk))
                throw runtime_error("Failed to read block from disk during invalidation.");

            if (!block.DisconnectBlock(txdb, pindexWalk))
                throw runtime_error("Failed to disconnect block during invalidation.");

            // Remove disconnected PoS blocks from setStakeSeen
            if (pindexWalk->IsProofOfStake())
            {
                extern set<pair<COutPoint, unsigned int> > setStakeSeen;
                setStakeSeen.erase(make_pair(pindexWalk->prevoutStake, pindexWalk->nStakeTime));
            }

            pindexWalk->pprev->pnext = nullptr;
            pindexWalk = pindexWalk->pprev;
        }

        // Update best block to the fork point
        if (pindex->pprev) {
            pindexBest = pindex->pprev;
            extern uint256 nBestChainTrust;
            nBestChainTrust = pindexBest->nChainTrust;
            nBestHeight = pindexBest->nHeight;
            txdb.WriteHashBestChain(pindexBest->GetBlockHash());
            if (!txdb.TxnCommit())
                throw runtime_error("Failed to commit transaction.");
            printf("invalidateblock: rewound chain to height %d hash %s\n",
                   pindexBest->nHeight, pindexBest->GetBlockHash().ToString().c_str());
        }
    }

    return Value::null;
}

Value reconsiderblock(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "reconsiderblock <hash>\n"
            "Reconsiders a previously invalidated block for activation.\n"
            "If it has more chain trust than current best, triggers a reorg.");

    string strHash = params[0].get_str();
    uint256 hash(strHash);

    LOCK(cs_main);

    if (mapBlockIndex.count(hash) == 0)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found");

    CBlockIndex* pindex = mapBlockIndex[hash];

    extern uint256 nBestChainTrust;
    if (pindex->nChainTrust > nBestChainTrust)
    {
        CBlock block;
        if (!block.ReadFromDisk(pindex))
            throw runtime_error("Failed to read block from disk.");

        auto txdb_holder = MakeChainDB(); CTxDBBase& txdb = *txdb_holder;
        block.SetBestChain(txdb, pindex);
        printf("reconsiderblock: reconsidered block %s at height %d, new best height=%d\n",
               hash.ToString().c_str(), pindex->nHeight, nBestHeight);
    }
    else
    {
        printf("reconsiderblock: block %s does not have more trust than current best\n",
               hash.ToString().c_str());
    }

    return Value::null;
}

Value dumputxoset(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "dumputxoset <filename> [nheaders]\n"
            "Dumps the current UTXO set and complete block index to a binary snapshot file.\n"
            "The snapshot can be used by new nodes to skip initial block download.\n"
            "\nArguments:\n"
            "1. filename   (string, required) Destination file path\n"
            "2. nheaders   (int, optional, deprecated and ignored)\n"
            "\nResult:\n"
            "{\n"
            "  \"filename\": \"...\",\n"
            "  \"height\": n,\n"
            "  \"blockhash\": \"...\",\n"
            "  \"file_size\": n\n"
            "}");

    string filename = params[0].get_str();
    // Keep accepting the legacy argument for RPC compatibility. Partial block
    // indexes prevent snapshot-loaded peers from serving fresh nodes.
    if (params.size() > 1)
        (void)params[1].get_int();

    std::filesystem::path destPath(filename);
    std::string strError;

    if (!UtxoSnapshot::DumpSnapshot(destPath, strError))
        throw runtime_error("dumputxoset failed: " + strError);

    // Get file size
    int64_t nFileSize = 0;
    if (std::filesystem::exists(destPath))
        nFileSize = (int64_t)std::filesystem::file_size(destPath);

    Object result;
    result.push_back(Pair("filename", filename));
    result.push_back(Pair("height", nBestHeight));
    result.push_back(Pair("blockhash", hashBestChain.GetHex()));
    result.push_back(Pair("file_size", nFileSize));

    return result;
}
