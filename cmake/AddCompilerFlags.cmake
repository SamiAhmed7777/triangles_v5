# cmake/AddCompilerFlags.cmake
# Shared compiler and linker flag configuration for all Triangles targets.

# ── Common warning flags ──
add_compile_options(
    -Wall -Wextra -Wno-ignored-qualifiers
    -Wformat -Wformat-security -Wno-unused-parameter
)

# Bitcoin-derived source uses C99-style adjacent string-literal concatenation
# for printf format macros: `"%"PRId64`. gcc tolerates this without a space;
# clang promotes `-Wreserved-user-defined-literal` to an error in C++20 mode
# and trips on hundreds of sites in util.cpp, kernel.cpp, etc. Suppress only
# under clang so gcc builds keep the original diagnostic behavior.
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_C_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-Wno-reserved-user-defined-literal)
endif()

# ── Common defines ──
add_compile_definitions(
    BOOST_SPIRIT_THREADSAFE
    BOOST_THREAD_USE_LIB
    BOOST_THREAD_PROVIDES_GENERIC_SHARED_MUTEX_ON_WIN
    BOOST_BIND_GLOBAL_PLACEHOLDERS
    __NO_SYSTEM_INCLUDES
)

# ── Hardening (non-Windows) ──
if(NOT WIN32)
    # Ubuntu bug #691722 workaround: reset before re-enabling
    add_compile_options(-fno-stack-protector)
    add_compile_options(-fstack-protector-all -Wstack-protector)
    add_compile_definitions(_FORTIFY_SOURCE=2)
    # -z relro/now is ELF-only (Linux); macOS linker doesn't support it
    if(NOT APPLE)
        add_link_options(-Wl,-z,relro -Wl,-z,now)
    endif()
endif()

# ── PIE (position-independent executables) ──
if(ENABLE_PIE AND NOT WIN32)
    add_compile_options(-fPIE)
    add_link_options(-pie)
endif()

# ── Optimization override ──
if(USE_O3)
    string(REPLACE "-O2" "-O3" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
    string(REPLACE "-O2" "-O3" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
    string(REPLACE "-O2" "-O3" CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO}")
    string(REPLACE "-O2" "-O3" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
endif()

# ── 32-bit SSE2 ──
if(CMAKE_SYSTEM_PROCESSOR MATCHES "i[3-6]86")
    add_compile_options(-msse2)
endif()

# ── x86-64 baseline ISA (portability across CPU vendors/models) ──
# CRITICAL: Without this, GCC on Intel CI runners (Skylake-X, Ice Lake,
# Sapphire Rapids) emits AVX-512 / AVX10 instructions (vmovdqu8, vpcompressd,
# vpopcntd, etc.) for std::string / memcpy inlining that CRASH with SIGILL
# on AMD EPYC (Milan, Genoa) and older Intel without AVX-512/AVX10.
# x86-64-v2 = baseline from ~2009 (Nehalem): SSE4.2 + POPCNT + CMPXCHG16B.
# Supported on EVERY x86_64 CPU Triangles runs on in production (DNS2, DNS3,
# Hetzner ARM64 excluded — that's a different build). Do NOT raise to v3
# (AVX2) without re-testing on every supported CPU; v3 is fine for most
# modern hardware but adds risk on edge cases (early Ryzen, Atom).
# Override with -DCMAKE_X86_64_BASELINE=OFF to disable (not recommended).
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$" AND NOT WIN32 AND NOT APPLE)
    option(CMAKE_X86_64_BASELINE
        "Compile with -march=x86-64-v2 (SSE4.2 baseline) for portability across CPU vendors"
        ON)
    if(CMAKE_X86_64_BASELINE)
        add_compile_options(-march=x86-64-v2)
        # -mtune=generic tells GCC the binary will run on CPUs other than the
        # build host. Combined with -march=x86-64-v2 above, the scheduler
        # picks instructions from the v2 subset only — no AVX-512 leaks.
        add_compile_options(-mtune=generic)
        # Belt-and-suspenders: explicitly disable AVX-512 / AVX10 / SVE
        # family ISAs that GCC 11+ can otherwise autovectorize into via
        # inlined libstdc++ std::string / std::copy / memcpy paths even when
        # -march=x86-64-v2 is set. Discovered 2026-08-01: v6.1.9 binary built
        # on EPYC 7763 (AVX-512) contained 741 vpbroadcastq EVEX instructions
        # which crash with SIGILL on every production node (KVM EPYC,
        # Ryzen 3600, ARM64) that lacks AVX-512. -mno-avx512f alone is
        # enough to suppress the SIGILL; the -mno-*avx10/sve* siblings
        # future-proof against the next GCC version autovectorizing
        # beyond AVX-512. See references/avx-512-sigill-build-fix.md
        # for the full diagnosis recipe.
        # NB: -mno-avx512*4fmaps / -mno-avx512*4vnniw use NO dash between
        # 'avx512' and the sub-feature (correct: -mno-avx5124fmaps). The
        # -mno-avx512-4fmaps form (with a dash) is rejected by GCC and
        # makes the whole build fail with "unrecognized command-line option".
        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID STREQUAL "GNU")
            add_compile_options(
                -mno-avx512f -mno-avx512pf -mno-avx512er -mno-avx512cd
                -mno-avx512vl -mno-avx512bw -mno-avx512dq -mno-avx512ifma
                -mno-avx512vbmi -mno-avx512vbmi2 -mno-avx512vnni
                -mno-avx512bitalg -mno-avx512vpopcntdq
                -mno-avx5124fmaps -mno-avx5124vnniw -mno-avx512vp2intersect
            )
        endif()
    endif()
endif()

# ── Platform: Windows (MSYS2 MinGW64) ──
if(WIN32)
    add_compile_options(-Wa,-mbig-obj)
    add_compile_options(-Wno-deprecated-declarations -Wno-reserved-user-defined-literal)
    add_link_options(-static -static-libgcc -static-libstdc++)
    add_compile_definitions(WIN32 _MT)
endif()

# ── Platform: macOS ──
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
    add_compile_options(-Wno-reserved-user-defined-literal -Wno-deprecated-declarations)
    add_compile_definitions(MAC_OSX MSG_NOSIGNAL=0)
endif()

# ── Platform: Linux ──
if(UNIX AND NOT APPLE)
    add_compile_definitions(LINUX)
endif()

# ── Static linking (Linux release builds) ──
if(ENABLE_STATIC AND UNIX AND NOT APPLE)
    add_link_options(-static)
endif()
