/*
 * Fast chain tip scanner for Triangles blk0001.dat
 * Computes Hash9 for each block header and builds the chain.
 *
 * Compile (MSYS2 MinGW64):
 *   cd src
 *   gcc -O2 -o scan_chain_tip.exe scan_chain_tip.c blake.c bmw.c groestl.c \
 *       skein.c jh.c keccak.c luffa.c cubehash.c shavite.c simd.c echo.c \
 *       hamsi.c fugue.c aes_helper.c -I. -lm
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

    /* Take first 32 bytes of final 64-byte hash */
    memcpy(output, hash[12], 32);
}

static void hash_to_hex(const uint8_t hash[32], char out[65]) {
    /* Display in big-endian (Bitcoin standard) */
    for (int i = 31; i >= 0; i--)
        sprintf(out + (31 - i) * 2, "%02x", hash[i]);
    out[64] = 0;
}

/* Simple hash table for block lookup (open addressing) */
#define HT_SIZE (1 << 22) /* 4M slots */
#define HT_MASK (HT_SIZE - 1)

typedef struct {
    uint8_t hash[32];       /* This block's Hash9 */
    uint8_t prevHash[32];   /* Previous block's hash */
    uint32_t nTime;
    uint8_t occupied;
} HTEntry;

static HTEntry* ht;

static uint32_t ht_index(const uint8_t hash[32]) {
    uint32_t idx;
    memcpy(&idx, hash, 4);
    return idx & HT_MASK;
}

static void ht_insert(const uint8_t hash[32], const uint8_t prevHash[32], uint32_t nTime) {
    uint32_t idx = ht_index(hash);
    while (ht[idx].occupied) {
        idx = (idx + 1) & HT_MASK;
    }
    memcpy(ht[idx].hash, hash, 32);
    memcpy(ht[idx].prevHash, prevHash, 32);
    ht[idx].nTime = nTime;
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

/* Track which hashes are used as prevHash (to find tips) */
#define PREV_HT_SIZE (1 << 22)
#define PREV_HT_MASK (PREV_HT_SIZE - 1)

static uint8_t (*prevSet)[32];
static uint8_t* prevOccupied;

static void prev_insert(const uint8_t hash[32]) {
    uint32_t idx;
    memcpy(&idx, hash, 4);
    idx &= PREV_HT_MASK;
    while (prevOccupied[idx]) {
        if (memcmp(prevSet[idx], hash, 32) == 0) return; /* already present */
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

int main(int argc, char* argv[]) {
    const char* filename = argc > 1 ? argv[1] : "blk0001.dat";

    FILE* f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "Cannot open %s\n", filename);
        return 1;
    }

    fseek(f, 0, SEEK_END);
    long fileSize = ftell(f);
    fseek(f, 0, SEEK_SET);

    printf("Scanning: %s (%.1f MB)\n", filename, fileSize / 1048576.0);
    fflush(stdout);

    /* Allocate hash tables */
    ht = (HTEntry*)calloc(HT_SIZE, sizeof(HTEntry));
    prevSet = (uint8_t(*)[32])calloc(PREV_HT_SIZE, 32);
    prevOccupied = (uint8_t*)calloc(PREV_HT_SIZE, 1);

    if (!ht || !prevSet || !prevOccupied) {
        fprintf(stderr, "Out of memory\n");
        return 1;
    }

    int blockCount = 0;
    unsigned char magic[4];
    uint32_t blockSize;
    uint8_t header[80];

    while (1) {
        long pos = ftell(f);

        if (fread(magic, 1, 4, f) != 4) break;

        if (memcmp(magic, MAGIC, 4) != 0) {
            fseek(f, pos + 1, SEEK_SET);
            continue;
        }

        if (fread(&blockSize, 4, 1, f) != 1) break;
        if (blockSize == 0 || blockSize > 4000000) {
            fseek(f, pos + 1, SEEK_SET);
            continue;
        }

        long headerPos = ftell(f);

        if (fread(header, 1, 80, f) != 80) break;

        /* Compute Hash9 */
        uint8_t blockHash[32];
        hash9(header, 80, blockHash);

        /* Extract prevHash and nTime from header */
        uint8_t prevHash[32];
        uint32_t nTime;
        memcpy(prevHash, header + 4, 32);
        memcpy(&nTime, header + 68, 4);

        ht_insert(blockHash, prevHash, nTime);
        prev_insert(prevHash);

        blockCount++;

        if (blockCount % 100000 == 0) {
            float pct = (float)ftell(f) / fileSize * 100;
            printf("  %d blocks (%.1f%%)...\n", blockCount, pct);
            fflush(stdout);
        }

        /* Skip rest of block */
        fseek(f, headerPos + blockSize, SEEK_SET);
    }

    fclose(f);
    printf("\nTotal blocks in file: %d\n\n", blockCount);

    /* Find tips: blocks whose hash is NOT in prevSet */
    printf("Finding chain tips...\n");

    int tipCount = 0;
    for (uint32_t i = 0; i < HT_SIZE; i++) {
        if (!ht[i].occupied) continue;
        if (!prev_contains(ht[i].hash)) {
            tipCount++;

            /* Walk back to find height */
            int height = 0;
            uint8_t current[32];
            memcpy(current, ht[i].hash, 32);

            while (1) {
                HTEntry* entry = ht_find(current);
                if (!entry) break;

                /* Check if prev is all zeros (genesis) */
                int isGenesis = 1;
                for (int j = 0; j < 32; j++) {
                    if (entry->prevHash[j] != 0) { isGenesis = 0; break; }
                }
                if (isGenesis) break;

                memcpy(current, entry->prevHash, 32);
                height++;
            }

            char hexHash[65];
            hash_to_hex(ht[i].hash, hexHash);

            printf("============================================================\n");
            printf("CHAIN TIP #%d:\n", tipCount);
            printf("  Height: %d\n", height);
            printf("  Hash:   0x%s\n", hexHash);
            printf("  Time:   %u\n", ht[i].nTime);

            /* Print last 20 blocks */
            printf("\nLast 20 blocks:\n");
            memcpy(current, ht[i].hash, 32);
            int blocksToPrint[20];
            char blockHashes[20][65];
            uint32_t blockTimes[20];
            int printed = 0;

            for (int k = 0; k < 20; k++) {
                hash_to_hex(current, blockHashes[k]);
                HTEntry* e = ht_find(current);
                if (!e) break;
                blocksToPrint[k] = height - k;
                blockTimes[k] = e->nTime;
                printed++;
                memcpy(current, e->prevHash, 32);
            }

            for (int k = printed - 1; k >= 0; k--) {
                printf("  %8d: 0x%s  (time=%u)\n", blocksToPrint[k], blockHashes[k], blockTimes[k]);
            }
            printf("============================================================\n\n");
        }
    }

    if (tipCount == 0) {
        printf("No chain tips found! Something went wrong.\n");
    }

    free(ht);
    free(prevSet);
    free(prevOccupied);

    return 0;
}
