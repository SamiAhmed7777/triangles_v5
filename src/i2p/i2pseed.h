#ifndef TRIANGLES_I2PSEED_H
#define TRIANGLES_I2PSEED_H

// Hardcoded I2P seed nodes for initial peer discovery.
// These are .b32.i2p addresses (Destination hashes).
// Nodes must run i2pd with a server tunnel forwarding to the Triangles P2P port.
//
// NOTE: .b32.i2p addresses are derived from the destination's public key.
// They are generated when the node first creates its I2P tunnel keys.
// Replace these placeholders with actual seed node addresses once deployed.
//
// Dynamic seeds will also be available at:
//   https://seeds.cryptographic-triangles.org/i2p-seeds.txt
static const char *strMainNetI2PSeed[][1] = {
    // DNS2 - primary bootstrap server
    // Generate .b32.i2p by: i2pd --datadir=/root/.triangles/i2p_data
    //   then read: i2p_data/triangles-p2p-keys.dat → b32 address
    // Placeholder until seed nodes are deployed:
    // {"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx.b32.i2p"},
    {nullptr}
};

static const char *strTestNetI2PSeed[][1] = {
    {nullptr}
};

#endif
