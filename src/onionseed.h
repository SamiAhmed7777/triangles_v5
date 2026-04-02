
#ifndef TRIANGLES_ONIONSEED_H
#define TRIANGLES_ONIONSEED_H

// Onion seeds are now fetched dynamically via HTTP seed list.
// No hardcoded onion addresses - they go stale when Tor services restart.
// See: seeds.cryptographic-triangles.org
static const char *strMainNetOnionSeed[][1] = {
    {NULL}
};

static const char *strTestNetOnionSeed[][1] = {
    {NULL}
};

#endif
