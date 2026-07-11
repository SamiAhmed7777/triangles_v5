// Standalone reproducer for fuzz_script findings.
// Compile:
//   clang++ -std=c++20 -g -I src -I src/secp256k1/include \
//       src/test/fuzz/repro.cpp src/script.cpp \
//       -o repro -lcrypto -lssl
//
// Run:
//   ./repro crash-deadbeef.bin
//
// This is the same driver as script_fuzz.cpp's main() — kept separate so
// the fuzzer harness can be built with -fsanitize=fuzzer (which provides
// its own main) without conflicting.

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <fstream>
#include <vector>
#include <algorithm>
#include "script.h"
#include "main.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (size < 1) return 0;
    const size_t script_len = std::min<size_t>(data[0], 10000);
    if (size < 1 + script_len) return 0;
    std::vector<uint8_t> script_bytes(data + 1, data + 1 + script_len);
    CScript script(script_bytes.begin(), script_bytes.end());
    std::vector<std::vector<unsigned char>> stack;
    CTransaction tx;
    EvalScript(stack, script, tx, 0, 0);
    return 0;
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <corpus-file>\n", argv[0]);
        return 1;
    }
    std::ifstream in(argv[1], std::ios::binary);
    if (!in) {
        std::fprintf(stderr, "cannot open %s\n", argv[1]);
        return 1;
    }
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(in)),
                              std::istreambuf_iterator<char>());
    return LLVMFuzzerTestOneInput(data.data(), data.size());
}