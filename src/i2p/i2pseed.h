#ifndef TRIANGLES_I2PSEED_H
#define TRIANGLES_I2PSEED_H

// Hardcoded I2P seed nodes for initial peer discovery.
// These are .b32.i2p addresses (Destination hashes).
// Nodes must run i2pd (embedded or external) with a server tunnel
// forwarding to the Triangles P2P port.
//
// NOTE: .b32.i2p addresses are derived from the destination's public key.
// They are generated when the node first creates its I2P tunnel keys.
// These addresses were captured from running daemons via getnetworkinfo
// on 2026-08-05. See i2pseed-capture-2026-08-05.md for the raw outputs.
//
// Dynamic seeds are also available at:
//   https://seeds.cryptographic-triangles.org/i2p-seeds.txt
static const char *strMainNetI2PSeed[][1] = {
    // SAMI-PC - authoritative wallet node (main PC). Captured 2026-08-05.
    {"fecv4pomdm47epuadgrpkvxzjqfqwsjfc7t7xadwaac5bislyrhq.b32.i2p"},
    // DNS2 - primary bootstrap server (194.233.88.206). Captured 2026-08-05.
    {"7d5gujh6tw6xbd2uquedhpm3ixoglsgt3nkfqb4b5lvunhjdb2kq.b32.i2p"},
    // DNS3 - canonical chain reference (74.208.167.19). Captured 2026-08-05.
    {"jdrpj364rmdule7rw2jdl63wvk3kbaivuje7wyhayugjbxvgbj2a.b32.i2p"},
    {nullptr}
};

static const char *strTestNetI2PSeed[][1] = {
    {nullptr}
};

#endif
