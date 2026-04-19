// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "main.h"
#include "db.h"
#include "txdb.h"
#include "init.h"
#include "miner.h"
#include "trianglesrpc.h"

using namespace json_spirit;
using namespace std;

extern unsigned int nTargetSpacing;

Value getsubsidy(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
            "getsubsidy [nTarget]\n"
            "Returns proof-of-work subsidy value for the specified value of target.");

    unsigned int nBits = 0;

    if (params.size() != 0)
    {
        CBigNum bnTarget(uint256(params[0].get_str()));
        nBits = bnTarget.GetCompact();
    }
    else
    {
        nBits = GetNextTargetRequired(pindexBest, false);
    }

    return (uint64_t)GetProofOfWorkReward(0);
}

Value getmininginfo(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getmininginfo\n"
            "Returns an object containing mining-related information.");

    uint64_t nMinWeight = 0, nMaxWeight = 0, nWeight = 0;
    pwalletMain->GetStakeWeight(*pwalletMain, nMinWeight, nMaxWeight, nWeight);

    Object obj, diff, weight;
    obj.push_back(Pair("blocks",        (int)nBestHeight));
    obj.push_back(Pair("currentblocksize",(uint64_t)nLastBlockSize));
    obj.push_back(Pair("currentblocktx",(uint64_t)nLastBlockTx));

    diff.push_back(Pair("proof-of-work",        GetDifficulty()));
    diff.push_back(Pair("proof-of-stake",       GetDifficulty(GetLastBlockIndex(pindexBest, true))));
    diff.push_back(Pair("search-interval",      (int)nLastCoinStakeSearchInterval));
    obj.push_back(Pair("difficulty",    diff));

    obj.push_back(Pair("blockvalue",    (uint64_t)GetProofOfWorkReward(0)));
    obj.push_back(Pair("netmhashps",     GetPoWMHashPS()));
    obj.push_back(Pair("netstakeweight", GetPoSKernelPS()));
    obj.push_back(Pair("errors",        GetWarnings("statusbar")));
    obj.push_back(Pair("pooledtx",      (uint64_t)mempool.size()));

    weight.push_back(Pair("minimum",    (uint64_t)nMinWeight));
    weight.push_back(Pair("maximum",    (uint64_t)nMaxWeight));
    weight.push_back(Pair("combined",  (uint64_t)nWeight));
    obj.push_back(Pair("stakeweight", weight));

    obj.push_back(Pair("stakeinterest",    (uint64_t)COIN_YEAR_REWARD));
    obj.push_back(Pair("testnet",       fTestNet));
    return obj;
}

Value getstakinginfo(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getstakinginfo\n"
            "Returns an object containing staking-related information.\n"
            "Includes diagnostic details about why staking may be disabled.");

    uint64_t nMinWeight = 0, nMaxWeight = 0, nWeight = 0;
    pwalletMain->GetStakeWeight(*pwalletMain, nMinWeight, nMaxWeight, nWeight);

    uint64_t nNetworkWeight = GetPoSKernelPS();
    bool staking = nLastCoinStakeSearchInterval && nWeight;
    int nExpectedTime = staking ? (nTargetSpacing * nNetworkWeight / nWeight) : -1;

    // Diagnostic: determine why staking might be disabled
    Array stakingDisabledReasons;
    if (!GetBoolArg("-staking", true))
        stakingDisabledReasons.push_back("staking disabled via -staking=0 flag");

    if (pwalletMain->IsLocked())
        stakingDisabledReasons.push_back("wallet is locked (use walletpassphrase <pw> <timeout> true)");

    if (vNodes.empty())
        stakingDisabledReasons.push_back("no network connections (need at least 1 peer)");

    if (IsInitialBlockDownload())
        stakingDisabledReasons.push_back("initial block download in progress");

    if (nWeight == 0)
        stakingDisabledReasons.push_back("no mature coins available (coins need 520 confirmations)");

    if (!staking && nLastCoinStakeSearchInterval == 0)
        stakingDisabledReasons.push_back("stake miner thread not running");

    Object obj;

    obj.push_back(Pair("enabled", GetBoolArg("-staking", true)));
    obj.push_back(Pair("staking", staking));
    obj.push_back(Pair("errors", GetWarnings("statusbar")));

    obj.push_back(Pair("currentblocksize", (uint64_t)nLastBlockSize));
    obj.push_back(Pair("currentblocktx", (uint64_t)nLastBlockTx));
    obj.push_back(Pair("pooledtx", (uint64_t)mempool.size()));

    obj.push_back(Pair("difficulty", GetDifficulty(GetLastBlockIndex(pindexBest, true))));
    obj.push_back(Pair("search-interval", (int)nLastCoinStakeSearchInterval));

    obj.push_back(Pair("weight", (uint64_t)nWeight));
    obj.push_back(Pair("netstakeweight", (uint64_t)nNetworkWeight));

    obj.push_back(Pair("expectedtime", nExpectedTime));

    // Add detailed diagnostics
    obj.push_back(Pair("walletlocked", pwalletMain->IsLocked()));
    obj.push_back(Pair("walletunlockedforstakingonly", pwalletMain->fWalletUnlockStakingOnly));
    obj.push_back(Pair("connections", (int)vNodes.size()));
    obj.push_back(Pair("initialblockdownload", IsInitialBlockDownload()));
    obj.push_back(Pair("maturecoins", nWeight > 0));

    if (!stakingDisabledReasons.empty())
        obj.push_back(Pair("staking_disabled_reasons", stakingDisabledReasons));

    return obj;
}

// PoW mining RPCs (getwork, getworkex, getblocktemplate, submitblock) removed.
// PoW ended at block 9000 (CUTOFF_POW_BLOCK). These dead-code mining pool
// interfaces were removed to reduce false-positive antivirus detections,
// since AV engines pattern-match nonce-incrementing loops and mining pool
// protocols as "cryptominer" signatures.


