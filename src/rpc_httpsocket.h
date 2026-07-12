// Copyright (c) 2026 The Triangles developers.
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// Raw-socket transport for the JSON-RPC / REST HTTP server, replacing the
// previous Boost.Asio implementation. Provides:
//
//   - CSocketIOStream : a std::iostream backed by a connected SOCKET, so the
//     existing HTTP/JSON/SSE/REST code (which reads and writes std::iostream)
//     is unchanged.
//   - ConnectRPCSocket() : client-side connect (used by CallRPC).
//   - BindRPCSockets()   : create listening sockets for the RPC server.
//   - SockaddrToString() : numeric host string for a peer address.
//
// TLS for the RPC port is intentionally not supported here (it was a rarely
// used Boost.Asio::ssl feature). For remote access, front the RPC port with a
// TLS terminator (stunnel / nginx) or reach it over SSH / Tor — the same
// guidance Bitcoin Core adopted when it moved its RPC server off Boost.Asio.

#ifndef TRIANGLES_RPC_HTTPSOCKET_H
#define TRIANGLES_RPC_HTTPSOCKET_H

#include "compat.h"   // SOCKET, closesocket, INVALID_SOCKET, MSG_NOSIGNAL

#include <cstring>
#include <iostream>
#include <streambuf>
#include <string>
#include <vector>

#ifndef WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif

// ── std::streambuf over a connected socket ──────────────────────────────────
class CSocketStreamBuf : public std::streambuf
{
public:
    explicit CSocketStreamBuf(SOCKET s) : m_socket(s)
    {
        setg(m_in, m_in, m_in);  // empty get area to start
    }

protected:
    // Refill the get area with one recv().
    int_type underflow() override
    {
        if (gptr() < egptr())
            return traits_type::to_int_type(*gptr());
        int n = ::recv(m_socket, m_in, static_cast<int>(sizeof(m_in)), 0);
        if (n <= 0)
            return traits_type::eof();  // peer closed or error
        setg(m_in, m_in, m_in + n);
        return traits_type::to_int_type(*gptr());
    }

    // Bulk write (operator<< on strings lands here).
    std::streamsize xsputn(const char* s, std::streamsize n) override
    {
        return SendAll(s, n) ? n : 0;
    }

    int_type overflow(int_type ch) override
    {
        if (traits_type::eq_int_type(ch, traits_type::eof()))
            return traits_type::not_eof(ch);
        char c = static_cast<char>(ch);
        return SendAll(&c, 1) ? ch : traits_type::eof();
    }

    int sync() override { return 0; }  // sends are immediate; nothing buffered

private:
    bool SendAll(const char* s, std::streamsize n)
    {
        std::streamsize sent = 0;
        while (sent < n) {
            int r = ::send(m_socket, s + sent, static_cast<int>(n - sent), MSG_NOSIGNAL);
            if (r <= 0)
                return false;
            sent += r;
        }
        return true;
    }

    SOCKET m_socket;
    char m_in[8192];
};

// std::iostream that owns a CSocketStreamBuf bound to a socket. The socket
// itself is owned by the caller (AcceptedConnection / CallRPC), not closed here.
class CSocketIOStream : public std::iostream
{
public:
    explicit CSocketIOStream(SOCKET s) : std::iostream(nullptr), m_buf(s)
    {
        rdbuf(&m_buf);
    }

private:
    CSocketStreamBuf m_buf;
};

// Numeric (no DNS) host string for a peer sockaddr, e.g. "127.0.0.1" or "::1".
inline std::string SockaddrToString(const struct sockaddr* sa, socklen_t salen)
{
    char host[NI_MAXHOST] = {0};
    if (::getnameinfo(sa, salen, host, sizeof(host), nullptr, 0, NI_NUMERICHOST) != 0)
        return "unknown";
    return std::string(host);
}

// Client connect to host:port. Returns INVALID_SOCKET on failure.
inline SOCKET ConnectRPCSocket(const std::string& host, int port)
{
    struct addrinfo hints;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo* res = nullptr;
    const std::string portStr = std::to_string(port);
    if (::getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res) != 0)
        return INVALID_SOCKET;

    SOCKET hSocket = INVALID_SOCKET;
    for (struct addrinfo* rp = res; rp != nullptr; rp = rp->ai_next) {
        hSocket = ::socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (hSocket == INVALID_SOCKET)
            continue;
        if (::connect(hSocket, rp->ai_addr, static_cast<int>(rp->ai_addrlen)) == 0)
            break;
        closesocket(hSocket);
        hSocket = INVALID_SOCKET;
    }
    ::freeaddrinfo(res);
    return hSocket;
}

inline bool SetRPCSocketTimeouts(SOCKET socket, int timeoutSeconds)
{
#ifdef WIN32
    DWORD timeout = static_cast<DWORD>(timeoutSeconds * 1000);
#else
    struct timeval timeout;
    timeout.tv_sec = timeoutSeconds;
    timeout.tv_usec = 0;
#endif
    const char* value = reinterpret_cast<const char*>(&timeout);
    const socklen_t valueSize = sizeof(timeout);
    return ::setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, value, valueSize) == 0 &&
           ::setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, value, valueSize) == 0;
}

// Create listening sockets for the RPC server. An empty bindAddress binds only
// localhost. A non-empty value binds exactly that address; "*" explicitly
// requests wildcard addresses. IPv4 and IPv6 use separate sockets when the
// selected name resolves to both families.
inline std::vector<SOCKET> BindRPCSockets(int port, const std::string& bindAddress,
                                           std::string& strError)
{
    std::vector<SOCKET> vListen;

    struct addrinfo hints;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    const bool wildcard = bindAddress == "*";
    if (wildcard)
        hints.ai_flags = AI_PASSIVE;

    struct addrinfo* res = nullptr;
    const std::string portStr = std::to_string(port);
    const char* node = wildcard ? nullptr :
        (bindAddress.empty() ? "localhost" : bindAddress.c_str());
    int gai = ::getaddrinfo(node, portStr.c_str(), &hints, &res);
    if (gai != 0) {
        strError = std::string("RPC bind: getaddrinfo failed: ") + gai_strerror(gai);
        return vListen;
    }

    for (struct addrinfo* rp = res; rp != nullptr; rp = rp->ai_next) {
        if (rp->ai_family != AF_INET && rp->ai_family != AF_INET6)
            continue;
        SOCKET s = ::socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (s == INVALID_SOCKET)
            continue;

        int one = 1;
        ::setsockopt(s, SOL_SOCKET, SO_REUSEADDR,
                     reinterpret_cast<const char*>(&one), sizeof(one));
        if (rp->ai_family == AF_INET6) {
            // Keep IPv6 sockets v6-only so a separate IPv4 socket can also bind.
            ::setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY,
                         reinterpret_cast<const char*>(&one), sizeof(one));
        }

        if (::bind(s, rp->ai_addr, static_cast<int>(rp->ai_addrlen)) != 0 ||
            ::listen(s, SOMAXCONN) != 0) {
            closesocket(s);
            continue;
        }
        vListen.push_back(s);
    }
    ::freeaddrinfo(res);

    if (vListen.empty())
        strError = "RPC bind: could not bind any address (port in use?)";
    return vListen;
}

#endif // TRIANGLES_RPC_HTTPSOCKET_H
