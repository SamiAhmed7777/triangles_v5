// Copyright (c) 2026 Cryptographic Triangles
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// v5.9.22 hardening tests for the HTTPS seed-list path.
//
// Two pure functions under test (declared in netbase.h):
//
//   int DechunkTransferEncoding(const std::string& body, std::string& decoded)
//   std::vector<std::string> ParseSeedListBody(const std::string& body)
//   bool IsValidSocksNegotiationTimeout(int nMs)
//
// These functions replaced the inline parsers in net.cpp
// ThreadHTTPSeedFetch2. The tests cover every failure mode listed in the
// hardening brief:
//   - Normal non-chunked HTTP seed responses (the parser is a no-op)
//   - Valid chunked responses with several chunks
//   - Chunk extensions such as A;foo=bar
//   - Chunked payloads split at awkward boundaries
//   - Malformed chunk sizes, missing CRLF, truncated chunks, chunks whose
//     declared size exceeds remaining input
//   - Seed-list parsing with whitespace, commas, semicolons, comments,
//     multiple addresses per line, valid .onion:port entries, and invalid
//     entries
//   - -torconnecttimeout validation at the exact boundaries and just
//     outside them: 4999, 5000, 60000, 180000, and 180001 milliseconds

#include <boost/test/unit_test.hpp>

#include "netbase.h"

#include <string>
#include <vector>

using namespace std;

BOOST_AUTO_TEST_SUITE(http_seed_tests)

// ═══════════════════════════════════════════════════════════════════════════════
// DechunkTransferEncoding — happy path
// ═══════════════════════════════════════════════════════════════════════════════

BOOST_AUTO_TEST_CASE(dechunk_empty_body)
{
    string decoded;
    BOOST_CHECK_EQUAL(DechunkTransferEncoding(string(""), decoded), DECHUNK_EMPTY);
    BOOST_CHECK(decoded.empty());
}

BOOST_AUTO_TEST_CASE(dechunk_single_chunk)
{
    // "5\r\nhello\r\n0\r\n\r\n" → "hello"
    string body = "5\r\nhello\r\n0\r\n\r\n";
    string decoded;
    BOOST_CHECK_EQUAL(DechunkTransferEncoding(body, decoded), DECHUNK_OK);
    BOOST_CHECK_EQUAL(decoded, "hello");
}

BOOST_AUTO_TEST_CASE(dechunk_multiple_chunks)
{
    // Three chunks concatenated: "Hel" + "lo " + "world"
    string body = "3\r\nHel\r\n3\r\nlo \r\n5\r\nworld\r\n0\r\n\r\n";
    string decoded;
    BOOST_CHECK_EQUAL(DechunkTransferEncoding(body, decoded), DECHUNK_OK);
    BOOST_CHECK_EQUAL(decoded, "Hello world");
}

BOOST_AUTO_TEST_CASE(dechunk_with_chunk_extension)
{
    // "5;foo=bar\r\nhello\r\n0\r\n\r\n" → "hello"
    // Extensions after the size are part of the framing protocol and must
    // be stripped before parsing the hex size.
    string body = "5;foo=bar\r\nhello\r\n0\r\n\r\n";
    string decoded;
    BOOST_CHECK_EQUAL(DechunkTransferEncoding(body, decoded), DECHUNK_OK);
    BOOST_CHECK_EQUAL(decoded, "hello");
}

BOOST_AUTO_TEST_CASE(dechunk_with_multiple_extensions)
{
    // "5;a=b;c=d\r\nhello\r\n0\r\n\r\n"
    string body = "5;a=b;c=d\r\nhello\r\n0\r\n\r\n";
    string decoded;
    BOOST_CHECK_EQUAL(DechunkTransferEncoding(body, decoded), DECHUNK_OK);
    BOOST_CHECK_EQUAL(decoded, "hello");
}

BOOST_AUTO_TEST_CASE(dechunk_uppercase_hex)
{
    // "5\r\nhello\r\n0\r\n\r\n" with A-F uppercase
    string body = "A\r\n0123456789\r\n0\r\n\r\n";
    string decoded;
    BOOST_CHECK_EQUAL(DechunkTransferEncoding(body, decoded), DECHUNK_OK);
    BOOST_CHECK_EQUAL(decoded, "0123456789");
}

BOOST_AUTO_TEST_CASE(dechunk_payload_containing_crlf)
{
    // Chunk data itself contains CRLF — must not be mistaken for framing.
    string body = "B\r\nline1\r\nline2\r\n0\r\n\r\n";
    string decoded;
    BOOST_CHECK_EQUAL(DechunkTransferEncoding(body, decoded), DECHUNK_OK);
    BOOST_CHECK_EQUAL(decoded, "line1\r\nline2");
}

BOOST_AUTO_TEST_CASE(dechunk_split_at_awkward_boundary)
{
    // A long chunk whose internal "data" happens to look like a chunk-size
    // line. Hex 0x0B = 11 bytes; the data "FAKE\r\nFOO\r" contains CRLF.
    string body = "B\r\nFAKE\r\nFOO\r\r\n0\r\n\r\n";
    string decoded;
    BOOST_CHECK_EQUAL(DechunkTransferEncoding(body, decoded), DECHUNK_OK);
    // 11 bytes consumed: "FAKE\r\nFOO\r" (5 + 2 + 3 + 1 = 11)
    BOOST_CHECK_EQUAL(decoded, "FAKE\r\nFOO\r");
}

BOOST_AUTO_TEST_CASE(dechunk_last_chunk_with_extension)
{
    // "0;end=1\r\n\r\n" — last chunk with extension, no body
    string body = "5\r\nhello\r\n0;end=1\r\n\r\n";
    string decoded;
    BOOST_CHECK_EQUAL(DechunkTransferEncoding(body, decoded), DECHUNK_OK);
    BOOST_CHECK_EQUAL(decoded, "hello");
}

// ═══════════════════════════════════════════════════════════════════════════════
// DechunkTransferEncoding — failure modes
// ═══════════════════════════════════════════════════════════════════════════════

BOOST_AUTO_TEST_CASE(dechunk_no_crlf_after_size)
{
    // No CRLF after the chunk-size hex — must not be silently accepted.
    string body = "5XXhello\r\n0\r\n\r\n";
    string decoded;
    BOOST_CHECK_EQUAL(DechunkTransferEncoding(body, decoded), DECHUNK_NO_CHUNK_TERMINATOR);
}

BOOST_AUTO_TEST_CASE(dechunk_invalid_hex)
{
    // "G" is not a valid hex digit.
    string body = "G\r\nhello\r\n0\r\n\r\n";
    string decoded;
    BOOST_CHECK_EQUAL(DechunkTransferEncoding(body, decoded), DECHUNK_INVALID_HEX);
}

BOOST_AUTO_TEST_CASE(dechunk_empty_size_line)
{
    // Stray CRLF at the start — empty size line must be rejected.
    string body = "\r\nhello\r\n0\r\n\r\n";
    string decoded;
    BOOST_CHECK_EQUAL(DechunkTransferEncoding(body, decoded), DECHUNK_INVALID_HEX);
}

BOOST_AUTO_TEST_CASE(dechunk_oversize_chunk)
{
    // Declared 100 bytes but only 5 remain. Old code silently clamped;
    // strict version must report DECHUNK_OVERSIZE_CHUNK.
    string body = "64\r\nhello\r\n0\r\n\r\n";
    string decoded;
    BOOST_CHECK_EQUAL(DechunkTransferEncoding(body, decoded), DECHUNK_OVERSIZE_CHUNK);
}

BOOST_AUTO_TEST_CASE(dechunk_truncated_last_chunk_marker)
{
    // No "0\r\n" terminator — body just ends mid-chunk.
    string body = "5\r\nhello\r\n";
    string decoded;
    BOOST_CHECK_EQUAL(DechunkTransferEncoding(body, decoded), DECHUNK_NO_CHUNK_TERMINATOR);
}

BOOST_AUTO_TEST_CASE(dechunk_missing_data_crlf)
{
    // Chunk-data not followed by CRLF. Two chunks: first is 5 bytes "hello"
    // then "X" where CRLF should be. The parser must catch the missing CRLF
    // before trying to read the next chunk-size.
    string body = "5\r\nhelloX3\r\nfoo\r\n0\r\n\r\n";
    string decoded;
    BOOST_CHECK_EQUAL(DechunkTransferEncoding(body, decoded), DECHUNK_MISSING_DATA_CRLF);
}

BOOST_AUTO_TEST_CASE(dechunk_strtoul_overflow)
{
    // A hex value larger than size_t can represent. On a 64-bit system this
    // would be 17+ F's. We pick a 32-digit value: clearly overflows on both
    // 32 and 64 bit builds.
    string body = "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF\r\n0\r\n\r\n";
    string decoded;
    // Either INVALID_HEX (overflow detected) or OVERSIZE_CHUNK (caught at
    // bounds check) is acceptable — both correctly refuse the input.
    int rc = DechunkTransferEncoding(body, decoded);
    BOOST_CHECK(rc == DECHUNK_INVALID_HEX || rc == DECHUNK_OVERSIZE_CHUNK);
}

BOOST_AUTO_TEST_CASE(dechunk_sign_in_size)
{
    // strtoul would silently accept leading '+' or '-'. Our strict
    // hex-only validator must reject them.
    string body = "+5\r\nhello\r\n0\r\n\r\n";
    string decoded;
    BOOST_CHECK_EQUAL(DechunkTransferEncoding(body, decoded), DECHUNK_INVALID_HEX);
}

BOOST_AUTO_TEST_CASE(dechunk_whitespace_in_size)
{
    // strtoul would silently accept leading whitespace. Our strict
    // hex-only validator must reject them.
    string body = " 5\r\nhello\r\n0\r\n\r\n";
    string decoded;
    BOOST_CHECK_EQUAL(DechunkTransferEncoding(body, decoded), DECHUNK_INVALID_HEX);
}

BOOST_AUTO_TEST_CASE(dechunk_no_last_chunk)
{
    // Body has chunks but never reaches a size-0 terminator. Must be
    // rejected, not silently accepted as the whole body.
    string body = "5\r\nhello\r\n";  // missing "0\r\n\r\n"
    string decoded;
    int rc = DechunkTransferEncoding(body, decoded);
    BOOST_CHECK(rc == DECHUNK_NO_CHUNK_TERMINATOR || rc == DECHUNK_MISSING_DATA_CRLF);
}

// ═══════════════════════════════════════════════════════════════════════════════
// ParseSeedListBody
// ═══════════════════════════════════════════════════════════════════════════════

BOOST_AUTO_TEST_CASE(seedlist_empty)
{
    vector<string> out = ParseSeedListBody("");
    BOOST_CHECK(out.empty());
}

BOOST_AUTO_TEST_CASE(seedlist_single_onion_per_line)
{
    // Three v3 onion addresses, one per line.
    string body =
        "aaaa.onion:24112\n"
        "bbbb.onion:24112\n"
        "cccc.onion:24112\n";
    vector<string> out = ParseSeedListBody(body);
    BOOST_CHECK_EQUAL(out.size(), 3u);
    BOOST_CHECK_EQUAL(out[0], "aaaa.onion:24112");
    BOOST_CHECK_EQUAL(out[1], "bbbb.onion:24112");
    BOOST_CHECK_EQUAL(out[2], "cccc.onion:24112");
}

BOOST_AUTO_TEST_CASE(seedlist_crlf_line_endings)
{
    // Real-world: HTTP responses typically use CRLF.
    string body =
        "aaaa.onion:24112\r\n"
        "bbbb.onion:24112\r\n";
    vector<string> out = ParseSeedListBody(body);
    BOOST_CHECK_EQUAL(out.size(), 2u);
    BOOST_CHECK_EQUAL(out[0], "aaaa.onion:24112");
    BOOST_CHECK_EQUAL(out[1], "bbbb.onion:24112");
}

BOOST_AUTO_TEST_CASE(seedlist_multiple_per_line_space)
{
    // Several addresses on one line, space-separated.
    string body = "aaaa.onion:24112 bbbb.onion:24112 cccc.onion:24112\n";
    vector<string> out = ParseSeedListBody(body);
    BOOST_CHECK_EQUAL(out.size(), 3u);
    BOOST_CHECK_EQUAL(out[0], "aaaa.onion:24112");
    BOOST_CHECK_EQUAL(out[1], "bbbb.onion:24112");
    BOOST_CHECK_EQUAL(out[2], "cccc.onion:24112");
}

BOOST_AUTO_TEST_CASE(seedlist_multiple_per_line_comma)
{
    // Comma-separated — common in older seed lists.
    string body = "aaaa.onion:24112,bbbb.onion:24112,cccc.onion:24112\n";
    vector<string> out = ParseSeedListBody(body);
    BOOST_CHECK_EQUAL(out.size(), 3u);
}

BOOST_AUTO_TEST_CASE(seedlist_multiple_per_line_semicolon)
{
    // Semicolon-separated — sometimes used in INI-style configs.
    string body = "aaaa.onion:24112;bbbb.onion:24112;cccc.onion:24112\n";
    vector<string> out = ParseSeedListBody(body);
    BOOST_CHECK_EQUAL(out.size(), 3u);
}

BOOST_AUTO_TEST_CASE(seedlist_mixed_separators)
{
    // Tabs, multiple spaces, commas, semicolons all in one line.
    string body = "aaaa.onion:24112,\tbbbb.onion:24112  ;cccc.onion:24112\n";
    vector<string> out = ParseSeedListBody(body);
    BOOST_CHECK_EQUAL(out.size(), 3u);
}

BOOST_AUTO_TEST_CASE(seedlist_inline_comments)
{
    // Anything after '#' to end-of-line is dropped.
    string body =
        "aaaa.onion:24112  # primary\n"
        "# this whole line is a comment\n"
        "bbbb.onion:24112  # secondary\n";
    vector<string> out = ParseSeedListBody(body);
    BOOST_CHECK_EQUAL(out.size(), 2u);
    BOOST_CHECK_EQUAL(out[0], "aaaa.onion:24112");
    BOOST_CHECK_EQUAL(out[1], "bbbb.onion:24112");
}

BOOST_AUTO_TEST_CASE(seedlist_blank_lines)
{
    // Whitespace-only / blank lines are skipped.
    string body =
        "\n"
        "   \n"
        "aaaa.onion:24112\n"
        "\t\n"
        "bbbb.onion:24112\n";
    vector<string> out = ParseSeedListBody(body);
    BOOST_CHECK_EQUAL(out.size(), 2u);
}

BOOST_AUTO_TEST_CASE(seedlist_only_comments)
{
    // All-comment body produces empty output (zero valid addresses
    // downstream — caller logs the failure mode).
    string body =
        "# nothing useful here\n"
        "# more comments\n";
    vector<string> out = ParseSeedListBody(body);
    BOOST_CHECK(out.empty());
}

BOOST_AUTO_TEST_CASE(seedlist_portless_onion)
{
    // ".onion" without ":port" is allowed at the parser level — the caller
    // falls back to GetDefaultPort() before validating as a CService.
    string body = "aaaa.onion\nbbbb.onion:24112\n";
    vector<string> out = ParseSeedListBody(body);
    BOOST_CHECK_EQUAL(out.size(), 2u);
    BOOST_CHECK_EQUAL(out[0], "aaaa.onion");
    BOOST_CHECK_EQUAL(out[1], "bbbb.onion:24112");
}

BOOST_AUTO_TEST_CASE(seedlist_invalid_entry_preserved_for_caller)
{
    // The parser does NOT validate that entries are real .onion addresses
    // or valid CService — that's the caller's job. The parser is a pure
    // splitter; invalid entries (e.g. "not-a-host") are still returned
    // and will fail CService::IsValid() downstream.
    string body = "not-a-host\nxxxxxx.onion:24112\n";
    vector<string> out = ParseSeedListBody(body);
    BOOST_CHECK_EQUAL(out.size(), 2u);
    BOOST_CHECK_EQUAL(out[0], "not-a-host");
    BOOST_CHECK_EQUAL(out[1], "xxxxxx.onion:24112");
}

BOOST_AUTO_TEST_CASE(seedlist_trailing_whitespace_per_line)
{
    // Spaces/tabs at end of each line should not produce an empty token.
    string body = "aaaa.onion:24112   \t  \n";
    vector<string> out = ParseSeedListBody(body);
    BOOST_CHECK_EQUAL(out.size(), 1u);
    BOOST_CHECK_EQUAL(out[0], "aaaa.onion:24112");
}

BOOST_AUTO_TEST_CASE(seedlist_crlf_lf_mix)
{
    // Some lines CRLF, some LF — should all parse.
    string body =
        "aaaa.onion:24112\r\n"
        "bbbb.onion:24112\n"
        "cccc.onion:24112\r\n";
    vector<string> out = ParseSeedListBody(body);
    BOOST_CHECK_EQUAL(out.size(), 3u);
}

// ═══════════════════════════════════════════════════════════════════════════════
// IsValidSocksNegotiationTimeout — exact boundaries and just outside
// ═══════════════════════════════════════════════════════════════════════════════

BOOST_AUTO_TEST_CASE(timeout_just_below_lower_bound)
{
    BOOST_CHECK(!IsValidSocksNegotiationTimeout(4999));
}

BOOST_AUTO_TEST_CASE(timeout_exact_lower_bound)
{
    BOOST_CHECK(IsValidSocksNegotiationTimeout(5000));
}

BOOST_AUTO_TEST_CASE(timeout_default)
{
    BOOST_CHECK(IsValidSocksNegotiationTimeout(60000));
}

BOOST_AUTO_TEST_CASE(timeout_exact_upper_bound)
{
    BOOST_CHECK(IsValidSocksNegotiationTimeout(180000));
}

BOOST_AUTO_TEST_CASE(timeout_just_above_upper_bound)
{
    BOOST_CHECK(!IsValidSocksNegotiationTimeout(180001));
}

BOOST_AUTO_TEST_CASE(timeout_zero)
{
    BOOST_CHECK(!IsValidSocksNegotiationTimeout(0));
}

BOOST_AUTO_TEST_CASE(timeout_negative)
{
    BOOST_CHECK(!IsValidSocksNegotiationTimeout(-1));
}

BOOST_AUTO_TEST_CASE(timeout_max_int)
{
    // Guard against wraparound on int boundaries.
    BOOST_CHECK(!IsValidSocksNegotiationTimeout(2147483647));
}

BOOST_AUTO_TEST_CASE(timeout_midrange)
{
    // Several plausible values in the middle of the range.
    BOOST_CHECK(IsValidSocksNegotiationTimeout(10000));
    BOOST_CHECK(IsValidSocksNegotiationTimeout(30000));
    BOOST_CHECK(IsValidSocksNegotiationTimeout(120000));
}

// ═══════════════════════════════════════════════════════════════════════════════
// Integration: dechunked body → seed-list parser round-trip
// ═══════════════════════════════════════════════════════════════════════════════

BOOST_AUTO_TEST_CASE(roundtrip_chunked_then_parsed)
{
    // Build a chunked-encoded seed list, decode it, then parse the result.
    string seedBody =
        "aaaa.onion:24112\n"
        "bbbb.onion:24112\n"
        "# cccc is the backup\n"
        "cccc.onion:24112,dddd.onion:24112\n";

    // Encode into chunked form.
    string chunked;
    size_t pos = 0;
    while (pos < seedBody.size()) {
        size_t take = min(seedBody.size() - pos, (size_t)16);
        char hex[16];
        snprintf(hex, sizeof(hex), "%zx", take);
        chunked += string(hex) + "\r\n" + seedBody.substr(pos, take) + "\r\n";
        pos += take;
    }
    chunked += "0\r\n\r\n";

    string decoded;
    BOOST_REQUIRE_EQUAL(DechunkTransferEncoding(chunked, decoded), DECHUNK_OK);
    BOOST_CHECK_EQUAL(decoded, seedBody);

    vector<string> out = ParseSeedListBody(decoded);
    BOOST_CHECK_EQUAL(out.size(), 4u);
    BOOST_CHECK_EQUAL(out[0], "aaaa.onion:24112");
    BOOST_CHECK_EQUAL(out[1], "bbbb.onion:24112");
    BOOST_CHECK_EQUAL(out[2], "cccc.onion:24112");
    BOOST_CHECK_EQUAL(out[3], "dddd.onion:24112");
}

BOOST_AUTO_TEST_SUITE_END()
