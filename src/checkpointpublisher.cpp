// Copyright (c) 2026 The Triangles developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// Signed Checkpoint Publisher (Triangles v5.9.24) — implementation.
//
// See checkpointpublisher.h for the design. This file holds:
//   - The in-memory signed-checkpoint cache (a CCriticalSection-guarded
//     std::map keyed by height; values are block hashes)
//   - The canonical serialization used by both producer and consumer
//   - The JSON parsing/building helpers (small subset, no third-party deps)
//   - The trusted signers list (mirrors IsTrustedSnapshotSigner)

#include "checkpointpublisher.h"

#include <algorithm>
#include <cstdio>
#include <map>
#include <set>
#include <sstream>
#include <vector>

#include "sync.h"
#include "util.h"
#include "base58.h"
#include "key.h"
#include "serialize.h"
#include "net.h"          // for CCriticalSection
#include "main.h"         // for strMessageMagic
#include "bootstrap.h"    // for Bootstrap::DownloadFile

namespace Checkpoints {

// ============================================================================
// Trusted signers
// ============================================================================
//
// Mirrors Bootstrap::TRUSTED_SNAPSHOT_SIGNERS but kept SEPARATE so the two
// lists can be managed independently. The default trust list contains the
// project operator's address. Operators can extend via a future -trustedcheckpointsigner
// conf option (not yet implemented — see Phase 2 in checkpointpublisher.h).
static const char* TRUSTED_CHECKPOINT_SIGNERS[] = {
    "TG8f76yktTxDrT7JJymY3wVAusXiD3fVvX",  // Sami's wallet (DNS2 default)
};
static const size_t NUM_TRUSTED_CHECKPOINT_SIGNERS =
    sizeof(TRUSTED_CHECKPOINT_SIGNERS) / sizeof(TRUSTED_CHECKPOINT_SIGNERS[0]);

bool IsTrustedCheckpointSigner(const std::string& addr)
{
    for (size_t i = 0; i < NUM_TRUSTED_CHECKPOINT_SIGNERS; ++i) {
        if (addr == TRUSTED_CHECKPOINT_SIGNERS[i]) return true;
    }
    return false;
}

// ============================================================================
// In-memory cache of loaded signed checkpoints
// ============================================================================
//
// Guarded by a single CCriticalSection. The cache is small (a few thousand
// entries max — operator publishes one every N=5000 blocks, so for a 2.2M
// chain that's ~440 entries per active signer). Lookup is O(log n).
static CCriticalSection cs_signedCheckpoints;
static std::map<int, std::string> mapSignedCheckpoints;

bool IsKnownSignedCheckpoint(int nHeight, const std::string& hashHex)
{
    LOCK(cs_signedCheckpoints);
    auto it = mapSignedCheckpoints.find(nHeight);
    if (it == mapSignedCheckpoints.end()) return false;
    // case-insensitive compare — JSON parsers sometimes downcase hex
    if (it->second.size() != hashHex.size()) return false;
    for (size_t i = 0; i < it->second.size(); i++) {
        if (std::tolower(static_cast<unsigned char>(it->second[i])) !=
            std::tolower(static_cast<unsigned char>(hashHex[i]))) {
            return false;
        }
    }
    return true;
}

void AddSignedCheckpoints(const std::vector<SignedCheckpoint>& entries)
{
    LOCK(cs_signedCheckpoints);
    for (const auto& e : entries) {
        // Don't overwrite compiled-in mapCheckpoints — that gate runs FIRST
        // in AcceptBlock. The signed set is a SUPPLEMENT, not a replacement.
        mapSignedCheckpoints[e.nHeight] = e.hashHex;
    }
    printf("Checkpoints: added %lu signed-remote checkpoints to cache\n", (unsigned long)entries.size());
}

void ClearSignedCheckpoints()
{
    LOCK(cs_signedCheckpoints);
    mapSignedCheckpoints.clear();
}

// ============================================================================
// Canonical serialization — producer + consumer MUST agree on this byte sequence
// ============================================================================
//
// Format: "<height1>:<hash1>:<ts1>;<height2>:<hash2>:<ts2>;..."
//
// Properties:
//   - Entries in DESCENDING order (tip first)
//   - Lowercase hex, no 0x prefix, no leading zeros
//   - Timestamps are unix seconds, decimal
//   - Field separator ':' — guaranteed not to appear in hex
//   - Entry separator ';' — guaranteed not to appear in either
//   - Trailing newline is NOT part of the signed payload (producers MUST NOT
//     add one to the message before signing; consumers MUST NOT trim it off
//     the fetched JSON's message field before verifying)
//
// This function is PURE — no I/O, no globals. Tested in checkpoint_tests.cpp.
std::string SerializeEntriesForSigning(const std::vector<SignedCheckpoint>& entries)
{
    std::string out;
    for (size_t i = 0; i < entries.size(); i++) {
        if (i > 0) out += ";";
        out += std::to_string(entries[i].nHeight);
        out += ":";
        out += entries[i].hashHex;
        out += ":";
        out += std::to_string(entries[i].nTimestamp);
    }
    return out;
}

// ============================================================================
// Producer — build the JSON document
// ============================================================================
//
// This is intentionally a thin wrapper: the wallet signing happens in the
// caller (rpcwallet.cpp / daemon loop), which has the unlocked key. Here we
// just escape + format.
bool BuildSignedCheckpointsJson(
    const std::vector<SignedCheckpoint>& entries,
    const std::string& signingAddress,
    const std::string& signatureBase64,
    const std::string& message,
    std::string& outJson,
    std::string& strError)
{
    if (entries.empty()) {
        strError = "BuildSignedCheckpointsJson: entries vector is empty";
        return false;
    }
    if (signingAddress.empty()) {
        strError = "BuildSignedCheckpointsJson: signingAddress is empty";
        return false;
    }
    if (signatureBase64.empty()) {
        strError = "BuildSignedCheckpointsJson: signature is empty";
        return false;
    }

    // Sort entries DESCENDING by height — canonical form. Producers and
    // consumers both depend on this so verification is deterministic.
    std::vector<SignedCheckpoint> sorted = entries;
    std::sort(sorted.begin(), sorted.end(),
              [](const SignedCheckpoint& a, const SignedCheckpoint& b) {
                  return a.nHeight > b.nHeight;
              });

    // Build JSON manually — no third-party deps. Format is intentionally
    // simple (no nested objects beyond the entries array).
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"format_version\": 1,\n";
    oss << "  \"signing_address\": \"" << signingAddress << "\",\n";
    oss << "  \"message\": \"" << message << "\",\n";
    oss << "  \"signature\": \"" << signatureBase64 << "\",\n";
    oss << "  \"entries\": [\n";
    for (size_t i = 0; i < sorted.size(); i++) {
        oss << "    {\"height\": " << sorted[i].nHeight
            << ", \"hash\": \"" << sorted[i].hashHex << "\""
            << ", \"timestamp\": " << sorted[i].nTimestamp << "}";
        if (i + 1 < sorted.size()) oss << ",";
        oss << "\n";
    }
    oss << "  ]\n";
    oss << "}\n";

    outJson = oss.str();
    return true;
}

// ============================================================================
// Consumer — verify a JSON document
// ============================================================================

// Small JSON helper — extract a top-level array of objects from the
// "entries" field. We don't need full JSON parsing; the format is fixed.
static std::vector<std::string> ExtractJsonObjectArray(
    const std::string& json, const std::string& field)
{
    std::vector<std::string> objs;
    std::string key = "\"" + field + "\"";
    size_t pos = json.find(key);
    if (pos == std::string::npos) return objs;
    pos += key.size();
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == ':' ||
           json[pos] == '\t' || json[pos] == '\n' || json[pos] == '\r'))
        pos++;
    if (pos >= json.size() || json[pos] != '[') return objs;
    pos++;  // past '['
    while (pos < json.size()) {
        while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' ||
               json[pos] == '\n' || json[pos] == '\r' || json[pos] == ','))
            pos++;
        if (pos >= json.size() || json[pos] == ']') break;
        if (json[pos] != '{') break;
        // Find matching closing brace (shallow — no nested objects in entries)
        int depth = 1;
        size_t start = pos;
        pos++;
        while (pos < json.size() && depth > 0) {
            if (json[pos] == '{') depth++;
            else if (json[pos] == '}') depth--;
            pos++;
        }
        if (depth != 0) break;
        objs.push_back(json.substr(start, pos - start));
    }
    return objs;
}

// Extract an integer field from an entry object like:
//   {"height": 12345, "hash": "...", "timestamp": 1700000000}
static int ExtractJsonInt(const std::string& obj, const std::string& field)
{
    std::string key = "\"" + field + "\"";
    size_t pos = obj.find(key);
    if (pos == std::string::npos) return 0;
    pos += key.size();
    while (pos < obj.size() && (obj[pos] == ' ' || obj[pos] == ':' ||
           obj[pos] == '\t')) pos++;
    // Parse a non-negative integer
    int n = 0;
    bool foundAny = false;
    while (pos < obj.size() && obj[pos] >= '0' && obj[pos] <= '9') {
        n = n * 10 + (obj[pos] - '0');
        pos++;
        foundAny = true;
    }
    if (!foundAny) return 0;
    return n;
}

// Extract a string field from a small JSON object — mirrors ExtractJsonString
// in bootstrap.cpp. Duplicated here to keep checkpointpublisher.cpp standalone
// (no link dependency on bootstrap.cpp internals).
static std::string ExtractJsonString(const std::string& obj, const std::string& field)
{
    std::string key = "\"" + field + "\"";
    size_t pos = obj.find(key);
    if (pos == std::string::npos) return "";
    pos += key.size();
    while (pos < obj.size() && (obj[pos] == ' ' || obj[pos] == ':' ||
           obj[pos] == '\t')) pos++;
    if (pos >= obj.size() || obj[pos] != '\"') return "";
    pos++;
    size_t end = obj.find('\"', pos);
    if (end == std::string::npos) return "";
    return obj.substr(pos, end - pos);
}

bool VerifySignedCheckpoints(
    const std::string& jsonText,
    std::vector<SignedCheckpoint>& outEntries,
    std::string& outSigningAddress,
    std::string& strError)
{
    outEntries.clear();
    outSigningAddress.clear();

    // 1. Extract signing fields
    outSigningAddress = ExtractJsonString(jsonText, "signing_address");
    std::string signature = ExtractJsonString(jsonText, "signature");
    std::string message = ExtractJsonString(jsonText, "message");
    if (outSigningAddress.empty() || signature.empty() || message.empty()) {
        strError = "signed-checkpoints JSON missing required top-level fields "
                   "(signing_address/signature/message)";
        return false;
    }

    // 2. Verify signer is trusted
    if (!IsTrustedCheckpointSigner(outSigningAddress)) {
        strError = "signing_address " + outSigningAddress +
                   " is not in the trusted checkpoint signers list";
        return false;
    }

    // 3. Verify the address is well-formed (catches typos early)
    CTrianglesAddress addr(outSigningAddress);
    if (!addr.IsValid()) {
        strError = "signing_address " + outSigningAddress + " is not a valid Triangles address";
        return false;
    }
    CKeyID keyID;
    if (!addr.GetKeyID(keyID)) {
        strError = "signing_address " + outSigningAddress + " does not refer to a key";
        return false;
    }

    // 4. Decode and verify the signature (same code path as verifymessage RPC)
    bool fInvalid = false;
    std::vector<unsigned char> vchSig = DecodeBase64(signature.c_str(), &fInvalid);
    if (fInvalid) {
        strError = "signed-checkpoints signature is not valid base64";
        return false;
    }
    CDataStream ss(SER_GETHASH, 0);
    ss << strMessageMagic;
    ss << message;
    CKey key;
    if (!key.SetCompactSignature(Hash(ss.begin(), ss.end()), vchSig)) {
        strError = "signed-checkpoints signature failed to recover (bad sig or "
                   "message tampered)";
        return false;
    }
    if (key.GetPubKey().GetID() != keyID) {
        strError = "signed-checkpoints signature recovered to a key that does "
                   "not match the claimed signer address";
        return false;
    }

    // 5. Extract entries and verify they match the signed message
    std::vector<std::string> entryObjs = ExtractJsonObjectArray(jsonText, "entries");
    if (entryObjs.empty()) {
        strError = "signed-checkpoints JSON has no entries array or entries is empty";
        return false;
    }
    outEntries.reserve(entryObjs.size());
    for (const auto& obj : entryObjs) {
        SignedCheckpoint e;
        e.nHeight = ExtractJsonInt(obj, "height");
        e.hashHex = ExtractJsonString(obj, "hash");
        e.nTimestamp = ExtractJsonInt(obj, "timestamp");
        if (e.nHeight <= 0 || e.hashHex.empty() || e.nTimestamp <= 0) {
            strError = "malformed entry (height/hash/timestamp invalid): " + obj;
            return false;
        }
        // hashHex sanity: must be exactly 64 lowercase hex chars
        if (e.hashHex.size() != 64) {
            strError = "entry hash at height " + std::to_string(e.nHeight) +
                       " is not 64 chars: " + e.hashHex;
            return false;
        }
        for (char c : e.hashHex) {
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
                strError = "entry hash at height " + std::to_string(e.nHeight) +
                           " contains non-lowercase-hex character";
                return false;
            }
        }
        outEntries.push_back(e);
    }

    // 6. Verify the signed message exactly matches the canonical serialization
    //    of the entries. This is the cross-check that proves the entries
    //    weren't tampered with after signing.
    std::string expectedMessage = SerializeEntriesForSigning(outEntries);
    if (expectedMessage != message) {
        strError = "signed-checkpoints message does not match canonical entry "
                   "serialization — entries were tampered with after signing";
        return false;
    }

    printf("Checkpoints: signed-remote verified — %lu entries signed by %s\n",
           (unsigned long)outEntries.size(), outSigningAddress.c_str());
    return true;
}

// ============================================================================
// Network fetch — keep it simple. The signed-checkpoints doc is tiny (~5 KB
// for a year of entries at 5000-block intervals), so a plain HTTP GET is
// fine. We DO NOT go through Tor for this fetch: the bootstrap server is
// already a known clearnet endpoint (same model as the existing UTXO
// snapshot download, which uses ConnectDirectTCP per bootstrap.cpp).
// ============================================================================
bool LoadSignedCheckpoints(
    const std::string& host,
    const std::string& onDiskPath,
    std::vector<SignedCheckpoint>& outEntries,
    std::string& outSigningAddress,
    std::string& strError)
{
    outEntries.clear();
    outSigningAddress.clear();

    std::string jsonText;

    // Path A: use on-disk copy if it exists (lets the daemon start even when
    // the bootstrap server is unreachable, as long as we have a recent copy).
    if (!onDiskPath.empty()) {
        FILE* f = fopen(onDiskPath.c_str(), "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            long sz = ftell(f);
            fseek(f, 0, SEEK_SET);
            if (sz > 0 && sz < 10 * 1024 * 1024) {  // 10 MB cap — sanity
                jsonText.resize(sz);
                size_t got = fread(&jsonText[0], 1, sz, f);
                jsonText.resize(got);
            }
            fclose(f);
            if (!jsonText.empty()) {
                printf("Checkpoints: loaded on-disk signed-checkpoints from %s (%lu bytes)\n",
                       onDiskPath.c_str(), (unsigned long)jsonText.size());
            }
        }
    }

    // Path B: fetch from bootstrap server. We always try this — if it
    // succeeds, prefer the freshest doc over the on-disk copy.
    if (host.empty()) {
        strError = "LoadSignedCheckpoints: no host provided and no on-disk copy found";
        return !jsonText.empty();  // if we have disk content, still try to verify it
    }

    // Use Bootstrap::DownloadFile — already handles clearnet HTTPS, timeouts,
    // and redirects. We do NOT proxy through Tor.
    if (Bootstrap::DownloadFile(host, "signed-checkpoints.json",
                                 std::filesystem::temp_directory_path() / "signed-checkpoints.json.tmp",
                                 nullptr, strError,
                                 /*noProxy=*/true, /*portOverride=*/-1,
                                 /*maxDownloadBytes=*/10 * 1024 * 1024)) {
        std::filesystem::path tmp = std::filesystem::temp_directory_path() / "signed-checkpoints.json.tmp";
        FILE* f = fopen(tmp.string().c_str(), "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            long sz = ftell(f);
            fseek(f, 0, SEEK_SET);
            if (sz > 0 && sz < 10 * 1024 * 1024) {
                jsonText.resize(sz);
                size_t got = fread(&jsonText[0], 1, sz, f);
                jsonText.resize(got);
            }
            fclose(f);
        }
        std::error_code ec;
        std::filesystem::remove(tmp, ec);

        if (!jsonText.empty()) {
            printf("Checkpoints: fetched fresh signed-checkpoints from %s (%lu bytes)\n",
                   host.c_str(), (unsigned long)jsonText.size());
            // Persist to disk for next startup (only if onDiskPath was given)
            if (!onDiskPath.empty()) {
                FILE* f2 = fopen(onDiskPath.c_str(), "wb");
                if (f2) {
                    fwrite(jsonText.data(), 1, (unsigned long)jsonText.size(), f2);
                    fclose(f2);
                    printf("Checkpoints: persisted signed-checkpoints to %s\n", onDiskPath.c_str());
                }
            }
        }
    } else {
        printf("Checkpoints: WARNING — fetch from %s failed (%s)",
               host.c_str(), strError.c_str());
        if (jsonText.empty()) {
            strError = "could not fetch signed-checkpoints and no on-disk copy: " + strError;
            return false;
        }
        printf(" — falling back to on-disk copy\n");
        strError.clear();
    }

    // Verify whatever we ended up with
    return VerifySignedCheckpoints(jsonText, outEntries, outSigningAddress, strError);
}

} // namespace Checkpoints
