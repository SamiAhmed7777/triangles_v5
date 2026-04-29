
#ifndef TRIANGLES_ONIONSEED_H
#define TRIANGLES_ONIONSEED_H

// Hardcoded onion seed nodes for initial peer discovery.
// Also fetched dynamically via https://seeds.cryptographic-triangles.org/seeds.txt
static const char *strMainNetOnionSeed[][1] = {
    // DNS2 - primary bootstrap server (DNS2, 194.233.88.206)
    {"gxvrhv3qitnc6kobrhsrse46bmcfitnybapor3or3oczzuxn6hfzxyid.onion"},
    // DNS3 - canonical chain reference (74.208.167.19)
    {"i6tk7soznftvoibtskwlezviskiererhjndpsmrff4kaxw7jnd5izfqd.onion"},
    // Contabo seed 1 (173.212.201.200)
    {"cuazgshkl53hzgq4ejndcalezsefwloddxqqqigoe6kshwgqpn2dqzyd.onion"},
    // Contabo seed 2
    {"jmq5rovbovbg3erm7drv35rep5cpusu5chd6yuquvffphazliwj5cmid.onion"},
    // Contabo seed 3
    {"q27mry33e4mveo6lib3ktkp3ayovdcdphxw2vnkdocot3udedszkfpad.onion"},
    // Contabo seed 4
    {"2szbeqcq32orxbvja6a5xrmbqoqpggwf24ck5bjyzerkivkqplmmquid.onion"},
    {NULL}
};

static const char *strTestNetOnionSeed[][1] = {
    {NULL}
};

#endif
