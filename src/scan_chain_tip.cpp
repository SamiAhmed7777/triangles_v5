// Fast chain tip scanner - reads blk0001.dat and finds the chain tip
// Uses Hash9 to compute real block hashes, but skips full validation
// Compile: g++ -O2 -o scan_chain_tip scan_chain_tip.cpp -I. -std=c++11

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <map>
#include <vector>
#include <string>

// Hash9 includes
#include "hashblock.h"

// Triangles mainnet magic
static const unsigned char pchMagic[4] = { 0x70, 0x35, 0x22, 0x05 };

struct BlockHeader {
    int32_t nVersion;
    uint8_t hashPrevBlock[32];
    uint8_t hashMerkleRoot[32];
    uint32_t nTime;
    uint32_t nBits;
    uint32_t nNonce;
};

struct BlockInfo {
    uint8_t prevHash[32];
    uint32_t nTime;
    long filePos;
};

static void hash9(const void* input, size_t len, uint8_t* output) {
    // Use the Hash9 function from hashblock.h
    uint256 result = Hash9((const char*)input, (const char*)input + len);
    memcpy(output, &result, 32);
}

static std::string hashToHex(const uint8_t* hash) {
    char buf[65];
    // Display in big-endian (standard Bitcoin display format)
    for (int i = 31; i >= 0; i--)
        sprintf(buf + (31 - i) * 2, "%02x", hash[i]);
    buf[64] = 0;
    return std::string(buf);
}

int main(int argc, char* argv[]) {
    const char* filename = argc > 1 ? argv[1] : "blk0001.dat";

    FILE* f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "Cannot open %s\n", filename);
        return 1;
    }

    // Get file size
    fseek(f, 0, SEEK_END);
    long fileSize = ftell(f);
    fseek(f, 0, SEEK_SET);

    printf("Scanning %s (%ld bytes, %.1f MB)\n", filename, fileSize, fileSize / 1048576.0);

    // Map: block_hash -> BlockInfo
    // We store hashes as 32-byte arrays, use string keys for the map
    std::map<std::string, BlockInfo> blocks;
    // Set of all prevHashes (to find the tip = block not referenced as prev)
    std::map<std::string, bool> isPrevOf;

    int blockCount = 0;
    unsigned char magic[4];
    uint32_t blockSize;
    BlockHeader header;

    while (true) {
        long pos = ftell(f);

        // Read magic
        if (fread(magic, 1, 4, f) != 4) break;

        if (memcmp(magic, pchMagic, 4) != 0) {
            // Scan forward for magic
            fseek(f, pos + 1, SEEK_SET);
            continue;
        }

        // Read block size
        if (fread(&blockSize, 4, 1, f) != 1) break;
        if (blockSize == 0 || blockSize > 4000000) {
            fseek(f, pos + 1, SEEK_SET);
            continue;
        }

        long headerPos = ftell(f);

        // Read header
        if (fread(&header, sizeof(header), 1, f) != 1) break;

        // Compute Hash9 of header
        uint8_t blockHash[32];
        hash9(&header, 80, blockHash);

        std::string hashKey((char*)blockHash, 32);
        std::string prevKey((char*)header.hashPrevBlock, 32);

        BlockInfo info;
        memcpy(info.prevHash, header.hashPrevBlock, 32);
        info.nTime = header.nTime;
        info.filePos = pos;

        blocks[hashKey] = info;
        isPrevOf[prevKey] = true;

        blockCount++;

        if (blockCount % 100000 == 0) {
            float pct = (float)ftell(f) / fileSize * 100;
            printf("  %d blocks scanned (%.1f%%)...\n", blockCount, pct);
            fflush(stdout);
        }

        // Skip rest of block
        long remaining = blockSize - 80;
        if (remaining > 0)
            fseek(f, headerPos + blockSize, SEEK_SET);
    }

    fclose(f);
    printf("\nTotal blocks in file: %d\n", blockCount);

    // Find chain tips: blocks whose hash is NOT in isPrevOf
    printf("\nSearching for chain tips...\n");
    std::vector<std::pair<std::string, BlockInfo>> tips;
    for (auto& kv : blocks) {
        if (isPrevOf.find(kv.first) == isPrevOf.end()) {
            tips.push_back(kv);
        }
    }

    printf("Found %zu chain tip(s)\n\n", tips.size());

    // For each tip, walk back to find the height
    for (auto& tip : tips) {
        std::string current = tip.first;
        int height = 0;

        // Walk back through the chain
        while (true) {
            auto it = blocks.find(current);
            if (it == blocks.end()) break;

            std::string prevKey((char*)it->second.prevHash, 32);

            // Check if prevHash is all zeros (genesis)
            bool isGenesis = true;
            for (int i = 0; i < 32; i++) {
                if (it->second.prevHash[i] != 0) { isGenesis = false; break; }
            }
            if (isGenesis) break;

            current = prevKey;
            height++;
        }

        uint8_t tipHash[32];
        memcpy(tipHash, tip.first.data(), 32);

        printf("============================================================\n");
        printf("CHAIN TIP:\n");
        printf("  Height: %d\n", height);
        printf("  Hash:   0x%s\n", hashToHex(tipHash).c_str());
        printf("  Time:   %u\n", tip.second.nTime);

        // Also print last 10 blocks before tip
        printf("\nLast 10 blocks before tip:\n");
        current = tip.first;
        std::vector<std::pair<int, std::string>> lastBlocks;
        for (int i = 0; i <= 10 && blocks.find(current) != blocks.end(); i++) {
            uint8_t h[32];
            memcpy(h, current.data(), 32);
            lastBlocks.push_back({height - i, hashToHex(h)});
            std::string prevKey((char*)blocks[current].prevHash, 32);
            current = prevKey;
        }
        for (auto it = lastBlocks.rbegin(); it != lastBlocks.rend(); ++it) {
            printf("  %8d: 0x%s\n", it->first, it->second.c_str());
        }
        printf("============================================================\n\n");
    }

    return 0;
}
