
#ifndef TRIANGLES_ONIONSEED_H
#define TRIANGLES_ONIONSEED_H

// hidden service seeds
// v5 hard fork: v2 onion seeds removed (Tor v2 deprecated Oct 2021)
// v3 onion seeds will be added when bootstrap nodes are deployed
static const char *strMainNetOnionSeed[][1] = {
    // TODO: Add v3 .onion seed addresses (56-char format) when nodes are live
    // Example: {"exampleexampleexampleexampleexampleexampleexampleexampl.onion"},
    {NULL}
};

static const char *strTestNetOnionSeed[][1] = {
    {NULL}
};

#endif
