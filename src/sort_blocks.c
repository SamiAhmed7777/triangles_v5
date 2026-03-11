/*
 * Block sorter for Triangles blk0001.dat
 * Reads all blocks, identifies the longest chain, and writes a new file
 * with ONLY main-chain blocks in height order (genesis first).
 *
 * This eliminates orphan/fork blocks and ensures the wallet can import
 * the entire chain in a single fast pass with zero REORGANIZE operations.
 *
 * Compile (MSYS2 MinGW64):
 *   cd src
 *   gcc -O2 -o sort_blocks.exe sort_blocks.c blake.c bmw.c groestl.c \
 *       skein.c jh.c keccak.c luffa.c cubehash.c shavite.c simd.c echo.c \
 *       hamsi.c fugue.c aes_helper.c -I. -lm
 *
 * Usage:
 *   sort_blocks.exe <input_blk0001.dat> <output_sorted.dat>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "sph_blake.h"
#include "sph_bmw.h"
#include "sph_groestl.h"
#include "sph_jh.h"
#include "sph_keccak.h"
#include "sph_skein.h"
#include "sph_luffa.h"
#include "sph_cubehash.h"
#include "sph_shavite.h"
#include "sph_simd.h"
#include "sph_echo.h"
#include "sph_hamsi.h"
#include "sph_fugue.h"

/* Triangles mainnet magic bytes */
static const unsigned char MAGIC[4] = { 0x70, 0x35, 0x22, 0x05 };

/* Hash9: 13-step cascade hash */
static void hash9(const void* input, size_t len, uint8_t output[32]) {
    uint8_t hash[17][64];

    sph_blake512_context ctx_blake;
    sph_bmw512_context ctx_bmw;
    sph_groestl512_context ctx_groestl;
    sph_skein512_context ctx_skein;
    sph_jh512_context ctx_jh;
    sph_keccak512_context ctx_keccak;
    sph_luffa512_context ctx_luffa;
    sph_cubehash512_context ctx_cubehash;
    sph_shavite512_context ctx_shavite;
    sph_simd512_context ctx_simd;
    sph_echo512_context ctx_echo;
    sph_hamsi512_context ctx_hamsi;
    sph_fugue512_context ctx_fugue;

    sph_blake512_init(&ctx_blake);
    sph_blake512(&ctx_blake, input, len);
    sph_blake512_close(&ctx_blake, hash[0]);

    sph_bmw512_init(&ctx_bmw);
    sph_bmw512(&ctx_bmw, hash[0], 64);
    sph_bmw512_close(&ctx_bmw, hash[1]);

    sph_groestl512_init(&ctx_groestl);
    sph_groestl512(&ctx_groestl, hash[1], 64);
    sph_groestl512_close(&ctx_groestl, hash[2]);

    sph_skein512_init(&ctx_skein);
    sph_skein512(&ctx_skein, hash[2], 64);
    sph_skein512_close(&ctx_skein, hash[3]);

    sph_jh512_init(&ctx_jh);
    sph_jh512(&ctx_jh, hash[3], 64);
    sph_jh512_close(&ctx_jh, hash[4]);

    sph_keccak512_init(&ctx_keccak);
    sph_keccak512(&ctx_keccak, hash[4], 64);
    sph_keccak512_close(&ctx_keccak, hash[5]);

    sph_luffa512_init(&ctx_luffa);
    sph_luffa512(&ctx_luffa, hash[5], 64);
    sph_luffa512_close(&ctx_luffa, hash[6]);

    sph_cubehash512_init(&ctx_cubehash);
    sph_cubehash512(&ctx_cubehash, hash[6], 64);
    sph_cubehash512_close(&ctx_cubehash, hash[7]);

    sph_shavite512_init(&ctx_shavite);
    sph_shavite512(&ctx_shavite, hash[7], 64);
    sph_shavite512_close(&ctx_shavite, hash[8]);

    sph_simd512_init(&ctx_simd);
    sph_simd512(&ctx_simd, hash[8], 64);
    sph_simd512_close(&ctx_simd, hash[9]);

    sph_echo512_init(&ctx_echo);
    sph_echo512(&ctx_echo, hash[9], 64);
    sph_echo512_close(&ctx_echo, hash[10]);

    sph_hamsi512_init(&ctx_hamsi);
    sph_hamsi512(&ctx_hamsi, hash[10], 64);
    sph_hamsi512_close(&ctx_hamsi, hash[11]);

    sph_fugue512_init(&ctx_fugue);
    sph_fugue512(&ctx_fugue, hash[11], 64);
    sph_fugue512_close(&ctx_fugue, hash[12]);

    memcpy(output, hash[12], 32);
}

static void hash_to_hex(const uint8_t hash[32], char out[65]) {
    for (int i = 31; i >= 0; i--)
        sprintf(out + (31 - i) * 2, "%02x", hash[i]);
    out[64] = 0;
}

/* Hash table for block lookup (open addressing) */
#define HT_SIZE (1 << 22) /* 4M slots */
#define HT_MASK (HT_SIZE - 1)

typedef struct {
    uint8_t hash[32];
    uint8_t prevHash[32];
    uint32_t nTime;
    int64_t filePos;      /* Position of block data in file (after magic+size) */
    uint32_t blockSize;   /* Serialized block size */
    int32_t height;       /* Cached height (-1 = uncomputed) */
    uint8_t occupied;
} HTEntry;

static HTEntry* ht;

static uint32_t ht_index(const uint8_t hash[32]) {
    uint32_t idx;
    memcpy(&idx, hash, 4);
    return idx & HT_MASK;
}

static void ht_insert(const uint8_t hash[32], const uint8_t prevHash[32],
                       uint32_t nTime, int64_t filePos, uint32_t blockSize) {
    uint32_t idx = ht_index(hash);
    while (ht[idx].occupied) {
        /* If same hash already exists, keep the first occurrence */
        if (memcmp(ht[idx].hash, hash, 32) == 0)
            return;
        idx = (idx + 1) & HT_MASK;
    }
    memcpy(ht[idx].hash, hash, 32);
    memcpy(ht[idx].prevHash, prevHash, 32);
    ht[idx].nTime = nTime;
    ht[idx].filePos = filePos;
    ht[idx].blockSize = blockSize;
    ht[idx].height = -1;
    ht[idx].occupied = 1;
}

static HTEntry* ht_find(const uint8_t hash[32]) {
    uint32_t idx = ht_index(hash);
    while (ht[idx].occupied) {
        if (memcmp(ht[idx].hash, hash, 32) == 0)
            return &ht[idx];
        idx = (idx + 1) & HT_MASK;
    }
    return NULL;
}

/*
 * Compute height of a block with memoization.
 * Uses iterative walk-back with a stack to avoid deep recursion.
 * Returns -1 if chain is broken.
 */
static int compute_height(HTEntry* entry) {
    if (!entry) return -1;
    if (entry->height >= 0) return entry->height;

    /* Walk back collecting entries that need heights */
    #define STACK_SIZE 2500000
    static HTEntry** stack = NULL;
    if (!stack) {
        stack = (HTEntry**)malloc(STACK_SIZE * sizeof(HTEntry*));
        if (!stack) { fprintf(stderr, "Out of memory (stack)\n"); return -1; }
    }

    int sp = 0;
    HTEntry* cur = entry;

    while (cur && cur->height < 0) {
        /* Check for genesis (prevHash all zeros) */
        int isGenesis = 1;
        for (int j = 0; j < 32; j++) {
            if (cur->prevHash[j] != 0) { isGenesis = 0; break; }
        }
        if (isGenesis) {
            cur->height = 0;
            break;
        }

        if (sp >= STACK_SIZE) return -1; /* chain too long */
        stack[sp++] = cur;
        cur = ht_find(cur->prevHash);
    }

    /* Now unwind the stack, setting heights */
    int h = cur ? cur->height : -1;
    for (int i = sp - 1; i >= 0; i--) {
        if (h < 0) {
            stack[i]->height = -1;
        } else {
            h++;
            stack[i]->height = h;
        }
    }

    return entry->height;
}

/* Track prevHash references to find chain tips */
#define PREV_HT_SIZE (1 << 22)
#define PREV_HT_MASK (PREV_HT_SIZE - 1)

static uint8_t (*prevSet)[32];
static uint8_t* prevOccupied;

static void prev_insert(const uint8_t hash[32]) {
    uint32_t idx;
    memcpy(&idx, hash, 4);
    idx &= PREV_HT_MASK;
    while (prevOccupied[idx]) {
        if (memcmp(prevSet[idx], hash, 32) == 0) return;
        idx = (idx + 1) & PREV_HT_MASK;
    }
    memcpy(prevSet[idx], hash, 32);
    prevOccupied[idx] = 1;
}

static int prev_contains(const uint8_t hash[32]) {
    uint32_t idx;
    memcpy(&idx, hash, 4);
    idx &= PREV_HT_MASK;
    while (prevOccupied[idx]) {
        if (memcmp(prevSet[idx], hash, 32) == 0) return 1;
        idx = (idx + 1) & PREV_HT_MASK;
    }
    return 0;
}

/* Block reference for sorted output */
typedef struct {
    int64_t filePos;
    uint32_t blockSize;
} BlockRef;

int main(int argc, char* argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <input_blk0001.dat> <output_sorted.dat>\n", argv[0]);
        return 1;
    }

    const char* inputFile = argv[1];
    const char* outputFile = argv[2];

    FILE* f = fopen(inputFile, "rb");
    if (!f) {
        fprintf(stderr, "Cannot open input: %s\n", inputFile);
        return 1;
    }

    fseek(f, 0, SEEK_END);
    long long fileSize = ftell(f);
    fseek(f, 0, SEEK_SET);

    printf("=== Phase 1: Scanning blocks ===\n");
    printf("Input: %s (%.1f MB)\n", inputFile, fileSize / 1048576.0);
    fflush(stdout);

    /* Allocate hash tables */
    ht = (HTEntry*)calloc(HT_SIZE, sizeof(HTEntry));
    prevSet = (uint8_t(*)[32])calloc(PREV_HT_SIZE, 32);
    prevOccupied = (uint8_t*)calloc(PREV_HT_SIZE, 1);

    if (!ht || !prevSet || !prevOccupied) {
        fprintf(stderr, "Out of memory (hash tables)\n");
        return 1;
    }

    int blockCount = 0;
    unsigned char magic[4];
    uint32_t blkSize;
    uint8_t header[80];

    while (1) {
        long long pos = ftell(f);

        if (fread(magic, 1, 4, f) != 4) break;

        if (memcmp(magic, MAGIC, 4) != 0) {
            fseek(f, (long)(pos + 1), SEEK_SET);
            continue;
        }

        if (fread(&blkSize, 4, 1, f) != 1) break;
        if (blkSize == 0 || blkSize > 4000000) {
            fseek(f, (long)(pos + 1), SEEK_SET);
            continue;
        }

        long long blockDataPos = ftell(f);

        if (fread(header, 1, 80, f) != 80) break;

        uint8_t blockHash[32];
        hash9(header, 80, blockHash);

        uint8_t prevHash[32];
        uint32_t nTime;
        memcpy(prevHash, header + 4, 32);
        memcpy(&nTime, header + 68, 4);

        ht_insert(blockHash, prevHash, nTime, blockDataPos, blkSize);
        prev_insert(prevHash);

        blockCount++;

        if (blockCount % 100000 == 0) {
            float pct = (float)ftell(f) / fileSize * 100;
            printf("  %d blocks scanned (%.1f%%)...\n", blockCount, pct);
            fflush(stdout);
        }

        /* Skip rest of block */
        fseek(f, (long)(blockDataPos + blkSize), SEEK_SET);
    }

    printf("Total blocks in file: %d\n\n", blockCount);

    /* === Phase 2: Compute heights and find longest chain tip === */
    printf("=== Phase 2: Computing block heights (memoized) ===\n");
    fflush(stdout);

    int bestHeight = -1;
    uint8_t bestTipHash[32];
    int tipCount = 0;
    int heightsComputed = 0;

    for (uint32_t i = 0; i < HT_SIZE; i++) {
        if (!ht[i].occupied) continue;
        if (!prev_contains(ht[i].hash)) {
            tipCount++;

            /* Use memoized height computation */
            int height = compute_height(&ht[i]);

            if (tipCount <= 5 || height > bestHeight) {
                char hexHash[65];
                hash_to_hex(ht[i].hash, hexHash);
                printf("  Tip #%d: height=%d hash=0x%s\n", tipCount, height, hexHash);
            }

            if (tipCount % 1000 == 0) {
                printf("  ... %d tips processed so far (best=%d) ...\n", tipCount, bestHeight);
                fflush(stdout);
            }

            if (height > bestHeight) {
                bestHeight = height;
                memcpy(bestTipHash, ht[i].hash, 32);
            }
        }
    }

    if (bestHeight < 0) {
        fprintf(stderr, "No chain tips found!\n");
        fclose(f);
        return 1;
    }

    printf("\nTotal tips: %d, Best chain: height=%d\n\n", tipCount, bestHeight);

    /* === Phase 3: Walk chain from tip to genesis, collect block refs === */
    printf("=== Phase 3: Building sorted chain ===\n");
    fflush(stdout);

    int chainLen = bestHeight + 1; /* +1 for genesis */
    BlockRef* chain = (BlockRef*)malloc(chainLen * sizeof(BlockRef));
    if (!chain) {
        fprintf(stderr, "Out of memory (chain array for %d blocks)\n", chainLen);
        fclose(f);
        return 1;
    }

    /* Walk from tip backwards, filling array in reverse */
    uint8_t current[32];
    memcpy(current, bestTipHash, 32);
    int idx = bestHeight;

    while (idx >= 0) {
        HTEntry* entry = ht_find(current);
        if (!entry) {
            fprintf(stderr, "Chain broken at index %d!\n", idx);
            fclose(f);
            return 1;
        }

        chain[idx].filePos = entry->filePos;
        chain[idx].blockSize = entry->blockSize;

        /* Check for genesis */
        int isGenesis = 1;
        for (int j = 0; j < 32; j++) {
            if (entry->prevHash[j] != 0) { isGenesis = 0; break; }
        }
        if (isGenesis) {
            if (idx != 0) {
                fprintf(stderr, "Genesis found at index %d, expected 0!\n", idx);
            }
            break;
        }

        memcpy(current, entry->prevHash, 32);
        idx--;
    }

    printf("Chain collected: %d blocks (genesis to tip)\n\n", chainLen);

    /* Free hash tables - no longer needed */
    free(ht);
    free(prevSet);
    free(prevOccupied);

    /* === Phase 4: Write sorted output file === */
    printf("=== Phase 4: Writing sorted output ===\n");
    printf("Output: %s\n", outputFile);
    fflush(stdout);

    FILE* out = fopen(outputFile, "wb");
    if (!out) {
        fprintf(stderr, "Cannot open output: %s\n", outputFile);
        fclose(f);
        return 1;
    }

    /* Allocate a read buffer (largest block could be ~1MB) */
    uint8_t* buf = (uint8_t*)malloc(4000000);
    if (!buf) {
        fprintf(stderr, "Out of memory (block buffer)\n");
        fclose(f);
        fclose(out);
        return 1;
    }

    long long bytesWritten = 0;
    for (int i = 0; i < chainLen; i++) {
        /* Seek to block data position in input file */
        fseek(f, (long)chain[i].filePos, SEEK_SET);

        /* Read block data */
        if (fread(buf, 1, chain[i].blockSize, f) != chain[i].blockSize) {
            fprintf(stderr, "Read error at block %d (pos=%lld, size=%u)\n",
                    i, (long long)chain[i].filePos, chain[i].blockSize);
            break;
        }

        /* Write: magic + size + block data */
        fwrite(MAGIC, 1, 4, out);
        fwrite(&chain[i].blockSize, 4, 1, out);
        fwrite(buf, 1, chain[i].blockSize, out);

        bytesWritten += 8 + chain[i].blockSize;

        if ((i + 1) % 100000 == 0 || i == chainLen - 1) {
            printf("  %d / %d blocks written (%.1f MB)...\n",
                   i + 1, chainLen, bytesWritten / 1048576.0);
            fflush(stdout);
        }
    }

    fclose(out);
    fclose(f);
    free(buf);
    free(chain);

    printf("\nDone! Wrote %d main-chain blocks (%.1f MB) to %s\n",
           chainLen, bytesWritten / 1048576.0, outputFile);
    printf("Import with: triangles-qt.exe -datadir=... -loadblock=%s\n", outputFile);

    return 0;
}
