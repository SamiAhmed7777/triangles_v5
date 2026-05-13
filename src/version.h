// Copyright (c) 2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef TRIANGLES_VERSION_H
#define TRIANGLES_VERSION_H

#include "clientversion.h"
#include <string>

//
// client versioning
//

constexpr int CLIENT_VERSION =
                           1000000 * CLIENT_VERSION_MAJOR
                         +   10000 * CLIENT_VERSION_MINOR
                         +     100 * CLIENT_VERSION_REVISION
                         +       1 * CLIENT_VERSION_BUILD;

extern const std::string CLIENT_NAME;
extern const std::string CLIENT_BUILD;
extern const std::string CLIENT_DATE;

//
// database format versioning
//
constexpr int DATABASE_VERSION = 70509;

//
// network protocol versioning
//

constexpr int PROTOCOL_VERSION = 70206;

// v5 hard fork: require new protocol version (disconnects old nodes)
constexpr int MIN_PROTO_VERSION = 70205;

// Peers >= this version support the P2P UTXO snapshot protocol
// (getsnap/snap/getsnapchunk/snapchunk and the NODE_SNAPSHOT service flag).
constexpr int SNAPSHOT_PROTO_VERSION = 70206;

constexpr int INIT_PROTO_VERSION = 209;

// nTime field added to CAddress, starting with this version;
// if possible, avoid requesting addresses nodes older than this
constexpr int CADDR_TIME_VERSION = 70200;

// only request blocks from nodes outside this range of versions
constexpr int NOBLKS_VERSION_START = 0;
constexpr int NOBLKS_VERSION_END = 70203;

// BIP 0031, pong message, is enabled for all versions AFTER this one
constexpr int BIP0031_VERSION = 60000;

// "mempool" command, enhanced "getdata" behavior starts with this version:
constexpr int MEMPOOL_GD_VERSION = 60002;

#endif
