// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "main.h"
#include "net.h"
#include "trianglesrpc.h"
#include "addressindex.h"
#include "txdb.h"
#include "base58.h"
#include "utxosnapshot.h"

using namespace json_spirit;
using namespace std;

extern void TxToJSON(const CTransaction& tx, const uint256 hashBlock, json_spirit::Object& entry);
extern enum Checkpoints::CPMode CheckpointsMode;

double GetDifficulty(const CBlockIndex* blockindex)
{
    // Floating point number that is a multiple of the minimum difficulty,
    // minimum difficulty = 1.0.
    if (blockindex == NULL)
    {
        if (pindexBest == NULL)
            return 1.0;
        else
            blockindex = GetLastBlockIndex(pindexBest, false);
    }

    if (blockindex == NULL)
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
    CBlockIndex* pindexPrevStake = NULL;

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
    result.push_back(Pair("time", (boost::int64_t)block.GetBlockTime()));
    result.push_back(Pair("nonce", (boost::uint64_t)block.nNonce));
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

    CBlock block;
    CBlockIndex* pblockindex = mapBlockIndex[hashBestChain];
    while (pblockindex->nHeight > nHeight)
        pblockindex = pblockindex->pprev;

    uint256 hash = *pblockindex->phashBlock;

    pblockindex = mapBlockIndex[hash];
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
    result.push_back(Pair("time", (boost::int64_t)pblockindex->GetBlockTime()));
    result.push_back(Pair("nonce", (boost::uint64_t)pblockindex->nNonce));
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

    CTxDB txdb("r");
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

    CTxDB txdbRead("r");
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
        CTxDB txdbWrite;
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
    obj.push_back(Pair("timeoffset", (boost::int64_t)GetTimeOffset()));
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
        CTxDB txdb("r");
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
    CTxDB txdb("r");

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
    CTxDB txdb("r");

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
        CTxDB txdb;
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

            pindexWalk->pprev->pnext = NULL;
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

        CTxDB txdb;
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
            "Dumps the current UTXO set and recent block headers to a binary snapshot file.\n"
            "The snapshot can be used by new nodes to skip initial block download.\n"
            "\nArguments:\n"
            "1. filename   (string, required) Destination file path\n"
            "2. nheaders   (int, optional, default=2000) Number of block headers to include\n"
            "\nResult:\n"
            "{\n"
            "  \"filename\": \"...\",\n"
            "  \"height\": n,\n"
            "  \"blockhash\": \"...\",\n"
            "  \"file_size\": n\n"
            "}");

    string filename = params[0].get_str();
    unsigned int nHeaders = UTXO_SNAPSHOT_DEFAULT_HEADERS;
    if (params.size() > 1)
        nHeaders = params[1].get_int();

    if (nHeaders < 100)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "nheaders must be at least 100");

    boost::filesystem::path destPath(filename);
    std::string strError;

    if (!UtxoSnapshot::DumpSnapshot(destPath, nHeaders, strError))
        throw runtime_error("dumputxoset failed: " + strError);

    // Get file size
    int64_t nFileSize = 0;
    if (boost::filesystem::exists(destPath))
        nFileSize = (int64_t)boost::filesystem::file_size(destPath);

    Object result;
    result.push_back(Pair("filename", filename));
    result.push_back(Pair("height", nBestHeight));
    result.push_back(Pair("blockhash", hashBestChain.GetHex()));
    result.push_back(Pair("file_size", nFileSize));

    return result;
}
