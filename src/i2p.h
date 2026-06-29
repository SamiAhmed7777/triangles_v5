// Copyright (c) 2024 Triangles developers
// I2P (SAM v3) transport support
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// This module gives Triangles real I2P connectivity that mirrors the existing
// embedded-Tor design: instead of a SOCKS proxy it talks the SAM v3 protocol
// to a locally running I2P router (i2pd or Java I2P) and obtains a persistent
// I2P destination whose ".b32.i2p" address is shown alongside the .onion
// address.  The wallet:
//   * creates / loads a persistent destination (i2p_private_key in datadir),
//   * runs a STREAM session so peers can dial us,
//   * accepts inbound I2P streams and feeds them to the net layer,
//   * dials outbound ".b32.i2p" peers through the same session.
//
// A running I2P router with its SAM bridge enabled (default 127.0.0.1:7656) is
// required; nothing is bundled.  Enable with -i2p and optionally -i2psam=host:port.

#ifndef TRIANGLES_I2P_H
#define TRIANGLES_I2P_H

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

#include "compat.h"   // SOCKET / INVALID_SOCKET

// Default SAM bridge endpoint exposed by i2pd / Java I2P.
#define I2P_DEFAULT_SAM_HOST "127.0.0.1"
#define I2P_DEFAULT_SAM_PORT 7656

// Manages a single persistent I2P STREAM session over SAM v3.
class CI2PSession
{
public:
    static CI2PSession* GetInstance();

    // Bring the session up: connect to the SAM bridge, load/generate the
    // persistent destination and start accepting inbound streams.
    // Returns false (and logs) if no router/SAM bridge is reachable.
    bool Start();

    // Tear the session down and stop the accept loop.
    void Stop();

    bool IsEnabled() const { return fEnabled.load(); }
    bool IsActive() const { return fActive.load(); }

    // Our own ".b32.i2p" address (empty until the session is up).
    std::string GetB32Address();

    // Dial a remote ".b32.i2p" (or full base64 destination) through the
    // session.  On success hSocketRet is a connected, blocking data socket the
    // caller can hand to a CNode.  The caller takes ownership of the socket.
    bool Connect(const std::string& strDest, SOCKET& hSocketRet);

private:
    CI2PSession();
    ~CI2PSession();

    // --- low level SAM helpers ---
    bool SamConnect(SOCKET& hSocketRet);                 // raw TCP to the bridge
    bool SamHandshake(SOCKET hSocket);                   // HELLO VERSION
    bool SamSendLine(SOCKET hSocket, const std::string& strLine);
    bool SamRecvLine(SOCKET hSocket, std::string& strLineRet);
    static std::string SamGetValue(const std::string& strReply, const std::string& strKey);

    bool LoadOrCreateDestination(std::string& strPrivKeyRet);
    bool CreateSession();                                 // SESSION CREATE
    bool ResolveMyB32();                                  // NAMING LOOKUP ME
    void AcceptLoop();                                    // inbound STREAM ACCEPT

    // Compute the ".b32.i2p" address from a base64 (I2P alphabet) destination.
    static std::string DestToB32(const std::string& strB64Dest);

    std::string samHost;
    int samPort;
    std::string sessionId;
    std::string privateKey;   // persistent destination private key (base64)
    std::string b32Address;   // our own .b32.i2p
    SOCKET hSession;          // long-lived control socket owning the session

    std::atomic<bool> fEnabled;
    std::atomic<bool> fActive;
    std::atomic<bool> fShutdown;
    std::thread acceptThread;
    std::mutex cs;
};

// Convenience: start/stop from init.cpp.
bool StartI2P();
void StopI2P();

#endif // TRIANGLES_I2P_H
