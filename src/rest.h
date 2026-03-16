// Copyright (c) 2024 The Triangles developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef TRIANGLES_REST_H
#define TRIANGLES_REST_H

#include <string>
#include <map>

class CBlock;
class CBlockIndex;
class CTransaction;

// REST API response builder with CORS headers
std::string HTTPReplyREST(int nStatus, const std::string& strMsg,
                          const std::string& contentType = "application/json");

// Main REST request router
// Returns true if the URI was handled as a REST request, false if it should fall through to RPC
bool HandleRESTRequest(const std::string& strMethod,
                       const std::string& strURI,
                       const std::string& strBody,
                       std::map<std::string, std::string>& mapHeaders,
                       std::string& strReply,
                       std::string& strContentType,
                       int& nStatus);

// Check if a URI is a REST API path
bool IsRESTPath(const std::string& strURI);

// Check rate limit for an IP address. Returns true if allowed.
bool CheckRESTRateLimit(const std::string& strIP);

#endif // TRIANGLES_REST_H
