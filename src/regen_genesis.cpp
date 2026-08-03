// Utility to regenerate the genesis block on disk
#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>

// Include necessary headers
#include "main.h"
#include "serialize.h"
#include "util.h"

int main() {
    printf("Regenerating genesis block...\n");

    // Create genesis transaction
    const char* pszTimestamp = "july 16 2014, I'm deh besht mang, I deeed et!";
    CTransaction txNew;
    txNew.nVersion = 1;
    txNew.nTime = 1405500418;
    txNew.vin.resize(1);
    txNew.vout.resize(1);
    txNew.vin[0].scriptSig = CScript()
        << 486604799
        << CBigNum(9999)
        << vector<unsigned char>((const unsigned char*)pszTimestamp,
                                  (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
    txNew.vout[0].SetEmpty();

    // Create genesis block
    CBlock block;
    block.nVersion = 1;
    block.nTime = 1405500418;
    block.nBits = bnProofOfWorkLimit.GetCompact();
    block.nNonce = 43;
    block.hashPrevBlock = 0;
    block.vtx.push_back(txNew);
    block.hashMerkleRoot = block.BuildMerkleTree();

    printf("Genesis block hash: %s\n", block.GetHash().ToString().c_str());
    printf("Expected:           %s\n", hashGenesisBlockOfficial.ToString().c_str());
    printf("Match: %s\n", block.GetHash() == hashGenesisBlockOfficial ? "YES" : "NO");

    // Write to a temporary file first
    std::string tmpfile = "/tmp/genesis_block.dat";
    {
        std::ofstream file(tmpfile, std::ios::binary);
        if (!file) {
            fprintf(stderr, "Cannot create %s\n", tmpfile.c_str());
            return 1;
        }
        CDataStream ss(SER_DISK, CLIENT_VERSION);
        ss << block;
        file.write((const char*)ss.data(), ss.size());
    }

    printf("Genesis block written to %s (%zu bytes)\n", tmpfile.c_str(), std::filesystem::file_size(tmpfile));

    // Verify by reading back
    {
        std::ifstream file(tmpfile, std::ios::binary);
        if (!file) {
            fprintf(stderr, "Cannot read %s\n", tmpfile.c_str());
            return 1;
        }
        CDataStream ss(SER_DISK, CLIENT_VERSION);
        std::vector<unsigned char> buffer(std::filesystem::file_size(tmpfile));
        file.read((char*)buffer.data(), buffer.size());
        ss.write((const char*)buffer.data(), buffer.size());

        CBlock verifyBlock;
        ss >> verifyBlock;

        printf("Verified hash: %s\n", verifyBlock.GetHash().ToString().c_str());
        printf("Verification: %s\n", verifyBlock.GetHash() == block.GetHash() ? "PASS" : "FAIL");
    }

    return 0;
}
