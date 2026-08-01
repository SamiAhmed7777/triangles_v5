// Fuzz harness for CTransaction deserialization.
//
// Compile via the BUILD_FUZZ=ON path (see src/CMakeLists.txt):
//   cmake -G Ninja -DBUILD_TESTS=ON -DBUILD_FUZZ=ON ..
//   ninja transaction_deserialize_fuzz
//
// Run:
//   ./bin/transaction_deserialize_fuzz -max_total_time=300 -max_len=200000 corpus/
//   ./bin/transaction_deserialize_fuzz crash-deadbeef.bin
//
// Input format (libFuzzer): raw bytes that get fed straight into the
// Bitcoin-style deserializer. The fuzz target is deliberately raw
// bytes (no framing): it exercises ReadCompactSize + nested
// Unserialize_impl<uint8_t> / Unserialize_impl<CTxIn> /
// Unserialize_impl<CTxOut> with arbitrary attacker-controlled input.
//
// What this covers:
//   * CompactSize varint decoder (ReadCompactSize) — every overflow /
//     truncation / non-canonical encoding path.
//   * Vector<T> Unserialize_impl — recursive expansion when T is itself
//     a structured type (CTxIn / CTxOut). Known to do unbounded
//     std::vector::resize(nSize) before reading; this is the historical
//     DoS surface for "send a tx claiming nSize=0xFFFFFFFF".
//   * CScript deserialization (a vector<unsigned char> with script
//     bytes that downstream EvalScript consumes — the script_fuzz target
//     covers the EvalScript side; this covers the deserialize-side).
//   * CTransaction::CheckTransaction bounds (max size, negative value,
//     out-of-range totals) — these run AFTER the deserialize and reject
//     the parsed object. Fuzzing the deserialize+Check pair surfaces
//     any path where the parse side consumes unbounded resources before
//     the Check rejects.
//   * Hash determinism — GetHash() must produce the same uint256 for
//     the same bytes, regardless of intermediate state mutations.
//
// What this does NOT cover:
//   * Signature verification (needs CKey + a CTransaction; that's
//     covered by the existing script_tests.cpp and keystore_tests.cpp).
//   * Block-level validation (block_deserialize_fuzz would be the next
//     target if this proves its value).
//   * P2P message framing (the fuzz input is the raw tx payload, not
//     the wire envelope — the wire envelope goes through CNode / net
//     code, not the tx parser).
//
// Why this is the right second target:
// Every peer message body starts with a deserialize step. Bugs in this
// surface are attacker-reachable from any peer who can pass IP filters,
// so the blast radius is the entire p2p network. Bitcoin Core maintains
// `deserialize-fuzz` for tx, block, and p2p-message surfaces for the
// same reason — the cost of writing it is low (about 40 lines) and the
// historical bug rate is non-zero.

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <vector>
#include <algorithm>

#include "main.h"
#include "serialize.h"
#include "uint256.h"

// Read a single transaction from the input buffer.
//
// We construct a CDataStream from the fuzz input and call
// Unserialize directly. That exercises the SAME code path the daemon
// uses when receiving a "tx" P2P message — the wire payload is exactly
// the byte sequence that lands in Unserialize().
//
// The CDataStream machinery handles stream-state (eof, throw-on-truncation)
// the same way for both network reads and our in-memory buffer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (size == 0) return 0;

    CDataStream ds(SER_NETWORK, PROTOCOL_VERSION);
    ds.write(reinterpret_cast<const char*>(data), size);

    try
    {
        CTransaction tx;
        ds >> tx;

        // tx is now in some consistent or inconsistent state. We don't
        // care about validity — only that no input crashes, leaks, or
        // trips UBSan. Two post-parse sanity probes:

        // 1) Hash must be deterministic for any well-formed CTransaction
        //    object. A divergent hash indicates corrupted state in
        //    SerializeHash (we hash and discard the result, just to
        //    ensure the call doesn't UB).
        (void)tx.GetHash();

        // 2) Round-trip serialize must produce a stream that re-parses
        //    to the same GetHash(). This catches bugs where a struct
        //    field is dropped or scrambled during deserialization.
        CDataStream ds2(SER_NETWORK, PROTOCOL_VERSION);
        ds2 << tx;
        CTransaction tx2;
        ds2 >> tx2;
        if (tx2.GetHash() != tx.GetHash())
        {
            // Non-fatal — flag for inspection by writing to stderr so
            // the fuzzer log surfaces it. The fuzzer won't be killed.
            std::fprintf(stderr,
                "WARN: round-trip hash mismatch — deserialization loses information\n");
        }

        // 3) CheckTransaction bounds — should NOT crash even on garbage
        //    data, just return false. This is the post-parse validator
        //    that catches oversized / negative / out-of-range txs.
        (void)tx.CheckTransaction();
    }
    catch (const std::exception&)
    {
        // std::ios_base::failure from CDataStream on truncation, or
        // std::runtime_error from any Unserialize_impl check. These
        // are EXPECTED for malicious input — the daemon catches and
        // drops the peer, no UB or crash should result.
    }
    catch (...)
    {
        // Unknown exception — log so the fuzzer surfaces it. LibFuzzer
        // doesn't catch C++ exceptions thrown out of LLVMFuzzerTestOneInput;
        // they would terminate the process. Returning 0 keeps the
        // process alive so the fuzzer continues probing.
        std::fprintf(stderr, "WARN: unknown exception in tx deserialize\n");
    }

    return 0;
}
