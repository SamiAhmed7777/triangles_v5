// Copyright (c) 2025-2026 Triangles developers
// Embedded I2P (i2pd) integration - runs an I2P router in-process
// Distributed under the MIT/X11 software license

#ifndef TRIANGLES_I2P_EMBEDDED_H
#define TRIANGLES_I2P_EMBEDDED_H

#include <string>
#include <atomic>

// Cross-platform socket handle for SAM v3 streaming API.
// On Windows this is the native SOCKET type; on POSIX it is int (fd).
#ifdef WIN32
#  include <winsock2.h>
   typedef SOCKET I2pSocket_t;
#  define I2P_INVALID_SOCKET  INVALID_SOCKET
#else
   typedef int I2pSocket_t;
#  define I2P_INVALID_SOCKET  (-1)
#endif

// ---------------------------------------------------------------------------
//  CI2PSamSocket — SAM v3 direct streaming socket
//
//  Wraps a raw TCP socket to the i2pd SAM bridge. After Connect() succeeds,
//  the underlying socket is a bidirectional byte stream to the I2P
//  destination with NO SOCKS overhead. The Triangles P2P layer can read and
//  write directly once ownership is taken via GetRawSocket().
//
//  Lifecycle:
//    1. Construct
//    2. Connect(dest_b32, port)  — performs SAM SESSION CREATE + STREAM CONNECT
//    3. GetRawSocket()           — take the fd for direct read/write
//    4. The fd must be closed by the caller (e.g. via CloseSocket())
//
//  If Connect() fails, GetLastError() returns a human-readable diagnostic.
// ---------------------------------------------------------------------------
class CI2PSamSocket
{
public:
    CI2PSamSocket();
    ~CI2PSamSocket();

    CI2PSamSocket(const CI2PSamSocket&) = delete;
    CI2PSamSocket& operator=(const CI2PSamSocket&) = delete;

    // Perform the full SAM v3 handshake (HELLO → SESSION CREATE → STREAM CONNECT)
    // to reach dest_b32 (a .b32.i2p hostname).  samHost/samPort identify the
    // local SAM bridge (default 127.0.0.1:7656).
    //
    // The |port| argument is accepted for API symmetry with the Tor SOCKS
    // connection factory but is not part of the SAM v3 STREAM CONNECT request
    // (I2P destinations are address-only; there is no TCP-style port).
    bool Connect(const std::string& dest_b32, int port,
                 const std::string& samHost = "127.0.0.1", int samPort = 7656);

    // Release ownership of the raw socket fd. After this call the object
    // will not close it and the caller is responsible for cleanup.
    // Returns I2P_INVALID_SOCKET if not connected.
    I2pSocket_t GetRawSocket();

    // Close the socket if still owned (no-op after GetRawSocket()).
    void CloseSocket();

    bool IsValid() const { return rawSocket != I2P_INVALID_SOCKET; }
    std::string GetLastError() const { return lastError; }

    // The base64 local destination returned by SESSION STATUS (may be empty).
    const std::string& GetLocalDestination() const { return localDestination; }

private:
    I2pSocket_t rawSocket;
    std::string sessionId;
    std::string localDestination;
    std::string lastError;
    std::string recvBuffer;   // partial SAM response buffering

    // --- SAM protocol helpers ---
    bool SamConnect(const std::string& host, int port);
    bool SendLine(const std::string& line);
    bool ReadLine(std::string& lineOut);
    static std::string ParseValue(const std::string& line, const std::string& key);
};

// Embedded I2P router state
class CI2PEmbedded
{
private:
    static CI2PEmbedded* instance;
    std::atomic<bool> running;
    int socksPort;          // i2pd SOCKS proxy port (for outbound .i2p connections)
    int samPort;            // i2pd SAM bridge port (for SAM v3 protocol)
    int serverPort;         // Triangles P2P listen port (for incoming I2P connections)
    std::string i2pDataDir; // i2pd data directory (under wallet datadir)
    std::string i2pHostname; // Our .b32.i2p address (available after router startup)
    std::string lastError;

public:
    static CI2PEmbedded* GetInstance();

    CI2PEmbedded();
    ~CI2PEmbedded();

    // Start embedded i2pd router (blocks calling thread briefly during init)
    bool Start(int socksPort = 19100, int samPort = 7656, int serverPort = 0);

    // Request i2pd to shut down
    void Stop();

    // Check if i2pd is running
    bool IsRunning() const { return running.load(); }
    void SetRunning(bool value) { running.store(value); }

    // Get the SOCKS5 proxy address for outbound .i2p connections
    std::string GetSocksProxy() const;
    int GetSocksPort() const { return socksPort; }
    int GetSamPort() const { return samPort; }
    int GetServerPort() const { return serverPort; }
    const std::string& GetDataDir() const { return i2pDataDir; }

    // Get our .b32.i2p destination address
    std::string GetI2PAddress() const { return i2pHostname; }
    std::string GetStartupError() const { return lastError; }
    void SetStartupError(const std::string& value) { lastError = value; }

    // -------------------------------------------------------------------
    //  SAM v3 direct streaming API
    // -------------------------------------------------------------------

    // Create a SAM v3 connection to a .b32.i2p destination.
    // Returns a heap-allocated CI2PSamSocket on success (caller owns it
    // and must CloseSocket / delete), or nullptr on failure.  Use
    // GetLastError() on the returned object for diagnostics.
    CI2PSamSocket* CreateConnection(const std::string& dest_b32, int port);

    // Probe whether the SAM bridge port is accepting TCP connections.
    bool IsSamAvailable() const;
};

// Global init/shutdown hooks (called from init.cpp)
bool StartEmbeddedI2P();
void StopEmbeddedI2P();

#endif // TRIANGLES_I2P_EMBEDDED_H
