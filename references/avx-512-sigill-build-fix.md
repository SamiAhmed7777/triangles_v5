# AVX-512 SIGILL build fix — `-mno-avx512f` belt-and-suspenders

**TL;DR:** GCC 11+ on an AVX-512-capable CI runner will emit AVX-512
instructions in libstdc++-inlined `std::string` / `std::copy` / `memcpy` code
paths even when `-march=x86-64-v2 -mtune=generic` is set globally. The
resulting binary crashes with `SIGILL (Illegal instruction)` on every
production node that lacks AVX-512 (KVM EPYC, Ryzen 3600, ARM64, anything
pre-Skylake-X). The fix is to add `-mno-avx512f -mno-avx512*` to the
global compile options. **Don't trust `-march=x86-64-v2` alone** — it sets
the baseline ISA but does not prevent auto-vectorization from emitting
higher-ISA instructions.

## Symptom (v6.1.9, 2026-07-31)

DNS2 attempted to install the v6.1.9 `.deb`. Daemon started and died
immediately with `status=4/ILL` (illegal instruction), before reaching
`main()`. The systemd journal showed:

```
Aug 01 04:38:28 vmi3080415 trianglesd[367821]: status=4/ILL
```

The daemon was previously working on v6.1.4.0. The only thing that
changed was the binary.

## Diagnosis recipe (15 minutes)

```bash
# 1. Reproduce the crash under gdb so you can see the failing instruction
systemctl stop trianglesd
sleep 3
gdb --batch \
  -ex "set startup-with-shell off" \
  -ex "run -datadir=/root/.triangles -conf=/root/.triangles/triangles.conf" \
  -ex "info symbol \$pc" \
  -ex "x/3i \$pc" \
  -ex "x/8bx \$pc-4" \
  /usr/lib/cryptographic-triangles/trianglesd 2>&1 | tail -15
```

You will see something like:

```
Program received signal SIGILL, Illegal instruction.
0x00005555556bbe49 in ?? ()
No symbol matches $pc.
=> 0x5555556bbe49:	vpbroadcastq %rax,%xmm0
   0x5555556bbe4f:	sub    %r14,%rdx
   0x5555556bbe52:	test   %rdx,%rdx
0x5555556bbe45:	0x08	0x49	0x89	0xc4	0x62	0xf2	0xfd	0x08
```

The bytes `0x62 0xf2 0xfd 0x08` are the **EVEX prefix** — an AVX-512
encoding. The disassembled instruction `vpbroadcastq %rax, %xmm0` is
the broadcast form, which uses EVEX even when the destination is XMM.

## Why this happens

The Triangles cmake file `cmake/AddCompilerFlags.cmake` already sets
`-march=x86-64-v2 -mtune=generic` for `x86_64 && NOT WIN32 && NOT APPLE`:

```cmake
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$" AND NOT WIN32 AND NOT APPLE)
    option(CMAKE_X86_64_BASELINE "..." ON)
    if(CMAKE_X86_64_BASELINE)
        add_compile_options(-march=x86-64-v2)
        add_compile_options(-mtune=generic)
    endif()
endif()
```

`-march=x86-64-v2` sets the **baseline ISA** to ~Nehalem (SSE4.2 + POPCNT +
CMPXCHG16B). GCC should not emit anything higher. In practice GCC 11.4 +
`-O3` + libstdc++ inlining of `std::string::operator=`, `std::copy`, and
`memcpy` patterns from libstdc++ headers that contain `#pragma GCC
push_options` blocks for AVX-512 detection — together they emit
`vpbroadcastq` EVEX instructions into user code via header inlining.

The instruction comes from **libstdc++ inlining**, not from any
Triangles-specific source. The disassembly shows the inlined function
is in a region marked as `std::string::operator=(std::string&&) + 0x2610`
because the symbol table merges the entire `.text` into the closest
named symbol — but the AVX-512 instruction itself is in a Triangles
translation unit (the call chain eventually reaches it from
`main.cpp`/`net.cpp` via `std::string` operations on the onion/I2P
addrman paths).

## The fix

Add an explicit `-mno-avx512*` family block to
`cmake/AddCompilerFlags.cmake` inside the existing
`CMAKE_X86_64_BASELINE` block:

```cmake
if(CMAKE_X86_64_BASELINE)
    add_compile_options(-march=x86-64-v2)
    add_compile_options(-mtune=generic)
    # Belt-and-suspenders: GCC 11+ can autovectorize libstdc++
    # std::string / std::copy / memcpy paths into AVX-512 EVEX
    # instructions even when -march=x86-64-v2 is set. Force-disable
    # the whole AVX-512 family so a CI runner's EPYC 7763 (or any
    # AVX-512-capable build host) cannot leak AVX-512 into a binary
    # that needs to run on KVM EPYC, Ryzen 3000, or ARM64.
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID STREQUAL "GNU")
        add_compile_options(
            -mno-avx512f -mno-avx512pf -mno-avx512er -mno-avx512cd
            -mno-avx512vl -mno-avx512bw -mno-avx512dq -mno-avx512ifma
            -mno-avx512vbmi -mno-avx512vbmi2 -mno-avx512vnni
            -mno-avx512bitalg -mno-avx512vpopcntdq -mno-avx512-4fmaps
            -mno-avx512-4vnniw -mno-avx512vp2intersect
        )
    endif()
endif()
```

`-mno-avx512f` is the critical one (it's the foundation of the family).
The others cover AVX-512 sub-features GCC may emit. The clang-equivalent
of this is `-mno-avx512f -mno-avx512fp16 -mno-avx512pf -mno-avx512er
-mno-avx512cd -mno-avx512vl -mno-avx512bw -mno-avx512dq -mno-avx512ifma`
but this Triangles fix is GCC-only because the existing code already
guards on `CMAKE_CXX_COMPILER_ID STREQUAL "GNU"`.

## Verify the fix landed in the new binary

```bash
# Build, install, then check for EVEX-encoded instructions
objdump -d /usr/lib/cryptographic-triangles/trianglesd 2>/dev/null \
  | grep -c "vpbroadcastq"
# Expected: 0  (was 741 before the fix)

# Also check for any other EVEX-encoded instructions
objdump -d /usr/lib/cryptographic-triangles/trianglesd 2>/dev/null \
  | grep -E "vpcompress|vpdpwssd|vpdpbusd|gfni|vaes|vpclmulqdq" | head
# Expected: empty
```

The smoke test that should have caught this: **add a job to the
`Build All Platforms` workflow that runs the resulting trianglesd
binary on a non-AVX-512 runner before publishing artifacts.** Catches
this class of bug forever.

## Why this wasn't caught before

GitHub Actions' hosted `ubuntu-22.04` runner is an AMD EPYC 7763 (Zen 3,
AVX-512 capable). Every CI build worked because the runner has the
required ISA. No unit test actually runs the produced binary, so the
build-vs-run gap is invisible until the binary ships to a CPU without
AVX-512 (which is most production hardware, including KVM-virtualized
EPYC, Ryzen 3000/5000 series, and ARM64 nodes). The fix is both the
cmake `-mno-avx512f` belt and a CI smoke-test step that executes the
binary on a non-AVX-512 runner.

## Files changed for v6.2.0

- `cmake/AddCompilerFlags.cmake` — added the `-mno-avx512*` block
- `src/clientversion.h` — bumped to 6.2.0.0
- All version-bearing files updated by `./scripts/bump-version.sh 6.2.0`

## Pitfall — don't do these things

- **Don't just add `-march=x86-64-v2`** without also adding
  `-mno-avx512*`. The march alone is not enough on GCC 11+ with libstdc++
  inlining. The behavior was verified locally: `-march=x86-64-v2` alone
  still produced 741 AVX-512 instructions in the test build.
- **Don't add `-fno-tree-vectorize`** to "fix" the symptom. That would
  regress performance across the whole daemon. `-mno-avx512f` is the
  surgical fix.
- **Don't use `set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mno-avx512f")`**.
  `add_compile_options` is the correct API — it propagates to subdirectory
  targets (libsecp256k1, libtor, etc.) that were the actual sources of
  the AVX-512 in earlier sessions.

## Cross-references

- The Triangles release v6.1.9 was the first release with the staking-
  selfheal fix (`f69f087 [grade=B] fix(staking): carve out caught-up
  nodes from IBD gate so chain can self-heal`). v6.1.9 was the binary
  that exhibited this bug; v6.2.0 carries both the staking fix AND this
  build-portability fix.
- The git history for this fix is the v6.2.0 release.
