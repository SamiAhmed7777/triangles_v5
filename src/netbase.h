// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef TRIANGLES_NETBASE_H
#define TRIANGLES_NETBASE_H

#include <string>
#include <vector>

#include "serialize.h"
#include "compat.h"

extern int nConnectTimeout;

#ifdef WIN32
// In MSVC, this is defined as a macro, undefine it to prevent a compile and link error
#undef SetPort
#endif

enum Network
{
    NET_UNROUTABLE,
    NET_IPV4,
    NET_IPV6,
    NET_TOR,
    NET_I2P,

    NET_MAX,
};

extern int nConnectTimeout;
extern int nSocksNegotiationTimeout;
extern bool fNameLookup;

// ═══════════════════════════════════════════════════════════════════════════════
// v5.9.22 hardening: pure helper functions for the HTTPS seed-list path.
// Extracted from net.cpp ThreadHTTPSeedFetch2 so they can be unit-tested
// without the SSL/Tor network stack. All functions are side-effect free and
// operate on std::string/std::vector<std::string> only.
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * Result of dechunking an HTTP/1.1 chunked body. The daemon used to silently
 * treat malformed framing as a zero-length chunk, which dropped the entire
 * seed list. This enum lets the caller distinguish each failure mode and
 * surface it in logs.
 */
enum DechunkResult {
    DECHUNK_OK = 0,              // success
    DECHUNK_EMPTY,               // body is empty
    DECHUNK_NO_CHUNK_TERMINATOR, // missing CRLF after a chunk-size line
    DECHUNK_INVALID_HEX,         // chunk-size line is not valid hex
    DECHUNK_OVERSIZE_CHUNK,      // declared chunk size exceeds remaining input
    DECHUNK_MISSING_DATA_CRLF,   // CRLF missing after a chunk's data
};

/**
 * Decode an HTTP/1.1 Transfer-Encoding: chunked body.
 *
 *   chunked-body  = *chunk last-chunk trailer-part CRLF
 *   chunk         = chunk-size [ chunk-ext ] CRLF chunk-data CRLF
 *   chunk-size    = 1*HEXDIG
 *   last-chunk    = 1*("0") [ chunk-ext ] CRLF
 *   chunk-ext     = *( ";" chunk-ext-name [ "=" chunk-ext-val ] )
 *
 * @param[in]  body     the raw body bytes after the header terminator
 * @param[out] decoded  the dechunked payload on success
 * @return status code (DECHUNK_OK or one of the failure modes)
 *
 * The implementation is intentionally strict: a malformed hex digit, a
 * missing CRLF, or a chunk whose declared size is larger than the remaining
 * input all return an explicit error code rather than silently clamping.
 * Chunk extensions ("a;foo=bar") are preserved (stripped from the size
 * line) so legitimate servers that attach metadata to chunks are still
 * accepted.
 */
int DechunkTransferEncoding(const std::string& body, std::string& decoded);

/**
 * Parse a tolerant HTTPS seed-list body into individual host entries.
 *
 * Accepted per line:
 *   - one or more addresses separated by whitespace, commas, or semicolons
 *   - inline "#" comments (everything after '#' is dropped)
 *   - blank lines
 *   - CRLF or LF line endings
 *
 * Each returned entry is the address string (e.g. "abcd...onion:24112" or
 * "abcd...onion"). Empty/whitespace-only entries are omitted. The result is
 * a list of candidate strings suitable for CNetAddr/CService validation
 * downstream.
 */
std::vector<std::string> ParseSeedListBody(const std::string& body);

/**
 * Validate the -torconnecttimeout / nSocksNegotiationTimeout value.
 *
 * Accepts 5000..180000 ms inclusive. Returns true for in-range, false for
 * out-of-range. This is the central policy so callers and tests stay in
 * sync; do not duplicate the literal numbers elsewhere.
 */
bool IsValidSocksNegotiationTimeout(int nMs);

/** IP address (IPv6, or IPv4 using mapped IPv6 range (::FFFF:0:0/96)) */
class CNetAddr
{
    protected:
        unsigned char ip[16]; // in network byte order
        // For Tor v3 this holds the 32-byte Ed25519 public key. When m_is_i2p is
        // set it instead holds the 32-byte SHA-256 of the I2P destination (the
        // value rendered as the ".b32.i2p" address). A CNetAddr is never both.
        unsigned char tor_v3_pubkey[32];
        bool m_is_tor_v3;
        bool m_is_i2p;

    public:
        CNetAddr();
        CNetAddr(const struct in_addr& ipv4Addr);
        explicit CNetAddr(const char *pszIp, bool fAllowLookup = false);
        explicit CNetAddr(const std::string &strIp, bool fAllowLookup = false);
        void Init();
        void SetIP(const CNetAddr& ip);
        bool SetSpecial(const std::string &strName); // for Tor and I2P addresses
        bool IsIPv4() const;    // IPv4 mapped address (::FFFF:0:0/96, 0.0.0.0/0)
        bool IsIPv6() const;    // IPv6 address (not mapped IPv4, not Tor/I2P)
        bool IsRFC1918() const; // IPv4 private networks (10.0.0.0/8, 192.168.0.0/16, 172.16.0.0/12)
        bool IsRFC3849() const; // IPv6 documentation address (2001:0DB8::/32)
        bool IsRFC3927() const; // IPv4 autoconfig (169.254.0.0/16)
        bool IsRFC3964() const; // IPv6 6to4 tunnelling (2002::/16)
        bool IsRFC4193() const; // IPv6 unique local (FC00::/15)
        bool IsRFC4380() const; // IPv6 Teredo tunnelling (2001::/32)
        bool IsRFC4843() const; // IPv6 ORCHID (2001:10::/28)
        bool IsRFC4862() const; // IPv6 autoconfig (FE80::/64)
        bool IsRFC6052() const; // IPv6 well-known prefix (64:FF9B::/96)
        bool IsRFC6145() const; // IPv6 IPv4-translated address (::FFFF:0:0:0/96)
        bool IsTor() const;
        bool IsTorV3() const;
        bool IsI2P() const;
        bool IsLocal() const;
        bool IsRoutable() const;
        bool IsValid() const;
        bool IsMulticast() const;
        enum Network GetNetwork() const;
        std::string ToString() const;
        std::string ToStringIP() const;
        unsigned int GetByte(int n) const;
        uint64_t GetHash() const;
        bool GetInAddr(struct in_addr* pipv4Addr) const;
        std::vector<unsigned char> GetGroup() const;
        int GetReachabilityFrom(const CNetAddr *paddrPartner = NULL) const;
        void print() const;

#ifdef USE_IPV6
        CNetAddr(const struct in6_addr& pipv6Addr);
        bool GetIn6Addr(struct in6_addr* pipv6Addr) const;
#endif

        friend bool operator==(const CNetAddr& a, const CNetAddr& b);
        friend bool operator!=(const CNetAddr& a, const CNetAddr& b);
        friend bool operator<(const CNetAddr& a, const CNetAddr& b);

        IMPLEMENT_SERIALIZE
            (
             READWRITE(FLATDATA(ip));
             READWRITE(FLATDATA(tor_v3_pubkey));
             READWRITE(m_is_tor_v3);
             READWRITE(m_is_i2p);
            )
};

/** A combination of a network address (CNetAddr) and a (TCP) port */
class CService : public CNetAddr
{
    protected:
        unsigned short port; // host order

    public:
        CService();
        CService(const CNetAddr& ip, unsigned short port);
        CService(const struct in_addr& ipv4Addr, unsigned short port);
        CService(const struct sockaddr_in& addr);
        explicit CService(const char *pszIpPort, int portDefault, bool fAllowLookup = false);
        explicit CService(const char *pszIpPort, bool fAllowLookup = false);
        explicit CService(const std::string& strIpPort, int portDefault, bool fAllowLookup = false);
        explicit CService(const std::string& strIpPort, bool fAllowLookup = false);
        void Init();
        void SetPort(unsigned short portIn);
        unsigned short GetPort() const;
        bool GetSockAddr(struct sockaddr* paddr, socklen_t *addrlen) const;
        bool SetSockAddr(const struct sockaddr* paddr);
        friend bool operator==(const CService& a, const CService& b);
        friend bool operator!=(const CService& a, const CService& b);
        friend bool operator<(const CService& a, const CService& b);
        std::vector<unsigned char> GetKey() const;
        std::string ToString() const;
        std::string ToStringPort() const;
        std::string ToStringIPPort() const;
        void print() const;

#ifdef USE_IPV6
        CService(const struct in6_addr& ipv6Addr, unsigned short port);
        CService(const struct sockaddr_in6& addr);
#endif

        IMPLEMENT_SERIALIZE
            (
             CService* pthis = const_cast<CService*>(this);
             READWRITE(FLATDATA(ip));
             READWRITE(FLATDATA(tor_v3_pubkey));
             READWRITE(m_is_tor_v3);
             READWRITE(m_is_i2p);
             unsigned short portN = htons(port);
             READWRITE(portN);
             if (fRead)
                 pthis->port = ntohs(portN);
            )
};

typedef std::pair<CService, int> proxyType;

enum Network ParseNetwork(std::string net);
void SplitHostPort(std::string in, int &portOut, std::string &hostOut);
bool SetProxy(enum Network net, CService addrProxy, int nSocksVersion = 5);
bool GetProxy(enum Network net, proxyType &proxyInfoOut);
bool IsProxy(const CNetAddr &addr);
bool SetNameProxy(CService addrProxy, int nSocksVersion = 5);
bool HaveNameProxy();
bool LookupHost(const char *pszName, std::vector<CNetAddr>& vIP, unsigned int nMaxSolutions = 0, bool fAllowLookup = true);
bool LookupHostNumeric(const char *pszName, std::vector<CNetAddr>& vIP, unsigned int nMaxSolutions = 0);
bool Lookup(const char *pszName, CService& addr, int portDefault = 0, bool fAllowLookup = true);
bool Lookup(const char *pszName, std::vector<CService>& vAddr, int portDefault = 0, bool fAllowLookup = true, unsigned int nMaxSolutions = 0);
bool LookupNumeric(const char *pszName, CService& addr, int portDefault = 0);
bool ConnectSocket(const CService &addr, SOCKET& hSocketRet, int nTimeout = nConnectTimeout);
bool ConnectSocketByName(CService &addr, SOCKET& hSocketRet, const char *pszDest, int portDefault = 0, int nTimeout = nConnectTimeout);

#endif
