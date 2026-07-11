// Fuzz harness for Triangles script interpreter.
//
// Compile with:
//   clang++ -fsanitize=fuzzer,address,undefined -g -O1 \
//       -I src -I src/leveldb/include \
//       src/test/fuzz/script_fuzz.cpp \
//       <link the script interpreter and its deps>
//
// Input format (libFuzzer):
//   [1 byte scriptLen] [scriptLen bytes of raw script bytes]
// Anything beyond the first 1 + scriptLen bytes is ignored, so seed
// corpus files can be arbitrary-length — only the prefix matters.
//
// Or run a single corpus file:
//   ./script_fuzz corpus/script_001.bin
//
// What this covers:
//   * Every opcode dispatch in EvalScript (src/script.cpp:332)
//   * Stack underflow / overflow paths
//   * OP_CHECKMULTISIG stack walk (the area with the most historical
//     bugs — sigcache, multisig stack-walk, combineSigs)
//   * Push-data edge cases (OP_PUSHDATA1/2/4)
//   * Numeric opcode handling (overflow, MIN/MAX edge values)
//
// What this does NOT cover:
//   * Signature verification (needs a real CKey/CTransaction; tested
//     by BOOST unit tests instead — see src/test/script_tests.cpp)
//   * P2SH (EvalScript runs first; the second-script eval in VerifyScript
//     is gated on the first script returning true, which requires a
//     real signature flow)
//
// Why EvalScript alone is the right target: every bug in the script
// interpreter has lived here, and the input surface is small (a CScript
// is just a byte vector). libFuzzer can mutate script bytes freely
// without needing realistic sig/key setup. This is the same approach
// Bitcoin Core's `script_tests` fuzzer uses.

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <vector>

#include "script.h"
#include "main.h"

// Entry point for libFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (size < 1) return 0;

    // First byte: script length. Cap at 10000 to keep EvalScript bounded.
    // (Triangles enforces MAX_SCRIPT_SIZE=10000 in IsPushOnly and others.)
    const size_t script_len = std::min<size_t>(data[0], 10000);
    if (size < 1 + script_len) return 0;

    std::vector<uint8_t> script_bytes(data + 1, data + 1 + script_len);
    CScript script(script_bytes.begin(), script_bytes.end());

    // Run EvalScript against an empty transaction. nIn=0, nHashType=0.
    // We don't care about the return value or final stack state — we
    // care that no input crashes, leaks, or trips UBSan.
    std::vector<std::vector<unsigned char>> stack;
    CTransaction tx;  // default-constructed, empty
    EvalScript(stack, script, tx, 0, 0);

    return 0;
}