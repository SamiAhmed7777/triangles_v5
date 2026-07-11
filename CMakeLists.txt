cmake_minimum_required(VERSION 3.16)

# Silence CMP0167 warning (FindBoost removed in CMake 3.30+, use BoostConfig)
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

project(Triangles
    VERSION 6.0.0
    DESCRIPTION "Cryptographic Triangles Wallet"
    LANGUAGES C CXX
)

# ── C++ Standard ──
# C++20 required: RocksDB headers in MSYS2/Homebrew (8.x+) use `using enum`
# and defaulted operator== on user-defined types, both C++20-only.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)

# ── Build acceleration ──
# ccache: auto-detect and use if available
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    message(STATUS "ccache found: ${CCACHE_PROGRAM}")
else()
    message(STATUS "ccache not found — install it for faster rebuilds")
endif()

# Unity (jumbo) build: batch source files to reduce header parsing overhead
option(ENABLE_UNITY_BUILD "Enable CMake unity (jumbo) builds" OFF)
if(ENABLE_UNITY_BUILD)
    set(CMAKE_UNITY_BUILD ON)
    set(CMAKE_UNITY_BUILD_BATCH_SIZE 8)
endif()

# ── Reproducible-build support ─────────────────────────────────────────────
# REPRODUCIBLE_BUILD=ON strips absolute source paths from the final binary
# via -ffile-prefix-map. Two builds of the same commit with the same
# toolchain then produce byte-identical binaries (modulo any source paths
# that aren't routed through the macro — see scripts/verify-reproducible-build.sh
# for the full verification protocol).
#
# Default ON: this is a security property we want by default. Disable if
# you need stack traces with absolute paths (e.g. debugging a post-mortem).
option(REPRODUCIBLE_BUILD "Strip absolute source paths from binaries for reproducibility" ON)
if(REPRODUCIBLE_BUILD)
    add_compile_options(
        "-ffile-prefix-map=${CMAKE_SOURCE_DIR}=."
        "-ffile-prefix-map=${CMAKE_BINARY_DIR}=."
    )
    # SOURCE_DATE_EPOCH is the canonical reproducible-build env var
    # (https://reproducible-builds.org/docs/source-date-epoch/). If the
    # user hasn't set it explicitly, fall back to the commit timestamp from
    # git. This means binaries built without SOURCE_DATE_EPOCH still embed
    # a deterministic timestamp (the commit time, not wall-clock).
    if(NOT DEFINED ENV{SOURCE_DATE_EPOCH})
        execute_process(
            COMMAND git log -n 1 --format=%ct
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            OUTPUT_VARIABLE SOURCE_DATE_EPOCH
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
        if(NOT SOURCE_DATE_EPOCH)
            set(SOURCE_DATE_EPOCH "1700000000")  # 2023-11-14 fallback
        endif()
    endif()
    message(STATUS "Reproducible build: ON (SOURCE_DATE_EPOCH=${SOURCE_DATE_EPOCH})")
endif()

# ── Output directories ──
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# ── Custom module path ──
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# ── User-facing options ──
option(BUILD_QT           "Build triangles-qt (Qt5 GUI wallet)"              ON)
option(BUILD_DAEMON       "Build trianglesd (headless daemon)"               ON)
option(BUILD_CLI          "Build triangles-cli (JSON-RPC client)"            ON)
option(BUILD_TESTS        "Build test_triangles (Boost.Test unit tests)"     ON)
option(USE_UPNP           "Enable UPnP support via miniupnpc"               ON)
option(USE_IPV6           "Enable IPv6 support"                              ON)
option(USE_QRCODE         "Enable QR code generation via libqrencode"        OFF)
option(USE_DBUS           "Enable D-Bus notifications (Linux only)"          ON)
option(USE_ZMQ            "Enable ZMQ publisher support"                     OFF)
# Triangles is Tor-native.  Tor is REQUIRED — disabling it at build time is
# not a supported configuration.  The 2026-06-23 DNS2 clearnet-fork incident
# (5+ days on a parallel chain because someone flipped -notor=1 for
# troubleshooting and never reverted it) motivated this.  We keep the option
# for legacy recovery workflows, but default it ON and abort the build if
# anyone explicitly disables it.
option(USE_TOR_EMBEDDED   "Enable embedded Tor library linking"              ON)
if(DEFINED USE_TOR_EMBEDDED AND NOT USE_TOR_EMBEDDED)
    message(FATAL_ERROR
        "USE_TOR_EMBEDDED=OFF is not supported. Triangles is Tor-native. "
        "If you need clearnet mode for bootstrap recovery, build with "
        "USE_TOR_EMBEDDED=ON and pass -notor=1 -recovery-mode=1 at runtime "
        "instead.")
endif()
option(USE_O3             "Use -O3 optimization instead of -O2"              OFF)
option(ENABLE_PIE         "Build position-independent executables"           OFF)
option(ENABLE_STATIC      "Prefer static linking (Linux release builds)"     OFF)

# Embedded I2P (i2pd) — runs an I2P router in-process alongside Tor.
# When enabled, Triangles supports dual-network anonymity: Tor (.onion) +
# I2P (.b32.i2p). Disabled by default until seed nodes are deployed.
option(USE_I2P_EMBEDDED   "Enable embedded I2P (i2pd) library linking"       OFF)
set(I2P_SOURCE_ROOT   "" CACHE PATH "Path to i2pd source tree (for USE_I2P_EMBEDDED)")

# Cache variables for custom dependency paths
set(BDB_INCLUDE_PATH "" CACHE PATH "Path to Berkeley DB headers")
set(BDB_LIB_PATH     "" CACHE PATH "Path to Berkeley DB libraries")
set(EVENT_INCLUDE_PATH "" CACHE PATH "Path to libevent headers")
set(EVENT_LIB_PATH     "" CACHE PATH "Path to libevent libraries")
set(MINIUPNPC_INCLUDE_PATH "" CACHE PATH "Path to miniupnpc headers")
set(MINIUPNPC_LIB_PATH     "" CACHE PATH "Path to miniupnpc libraries")
set(TOR_SOURCE_ROOT   "" CACHE PATH "Path to Tor source tree (for USE_TOR_EMBEDDED)")

# ── Compiler/linker flags ──
include(AddCompilerFlags)

# ── Find required dependencies ──
find_package(OpenSSL REQUIRED)
find_package(Boost 1.71 REQUIRED COMPONENTS
    program_options thread chrono
    OPTIONAL_COMPONENTS filesystem system
)
if(BUILD_TESTS)
    find_package(Boost REQUIRED COMPONENTS unit_test_framework)
endif()
find_package(BerkeleyDB REQUIRED)
find_package(Libevent REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)

# ── Find optional dependencies ──
if(USE_UPNP)
    find_package(Miniupnpc REQUIRED)
endif()

if(USE_QRCODE)
    find_package(QRencode REQUIRED)
endif()

if(USE_ZMQ)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(ZMQ REQUIRED IMPORTED_TARGET libzmq)
endif()

# RocksDB is now a hard dependency: backs both the chain database and the
# secure-messaging store (smessage). Probe in order:
#   1. CMake config package (MSYS2, Homebrew, vcpkg, recent Linux)
#   2. pkg-config (some Linux distros, no .cmake files)
#   3. Manual find_path/find_library (Ubuntu 22.04's librocksdb-dev ships
#      neither a CMake config nor a .pc file)
# In all paths, a target named RocksDB::rocksdb is exposed for consumers.
find_package(RocksDB CONFIG QUIET)
if(NOT RocksDB_FOUND)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(RocksDB IMPORTED_TARGET QUIET rocksdb)
    endif()
endif()
if(NOT TARGET RocksDB::rocksdb AND NOT TARGET PkgConfig::RocksDB)
    find_path(ROCKSDB_INCLUDE_DIR
        NAMES rocksdb/db.h
        PATHS /usr/include /usr/local/include
    )
    find_library(ROCKSDB_LIBRARY
        NAMES rocksdb
        PATHS /usr/lib /usr/lib/x86_64-linux-gnu /usr/local/lib
    )
    if(NOT ROCKSDB_INCLUDE_DIR OR NOT ROCKSDB_LIBRARY)
        message(FATAL_ERROR
            "RocksDB not found. Install librocksdb-dev (Ubuntu/Debian), "
            "rocksdb (Homebrew), or mingw-w64-x86_64-rocksdb (MSYS2).")
    endif()
    add_library(RocksDB::rocksdb UNKNOWN IMPORTED)
    set_target_properties(RocksDB::rocksdb PROPERTIES
        IMPORTED_LOCATION "${ROCKSDB_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${ROCKSDB_INCLUDE_DIR}"
    )
    message(STATUS "Found RocksDB (manual probe): ${ROCKSDB_LIBRARY}")
endif()

# Modernization: SQLite3 for the new wallet DB backend.
find_package(SQLite3 REQUIRED)

# Triangles uses RocksDB features that only exist in 7.4+ (XXH3 per-block
# checksum, type 4). Building against an older RocksDB produces a binary
# whose smsgDB Open() fails on any SST file written by RocksDB 7.4+ —
# instead of just bailing, src/smessage.cpp::SecMsgDB::Open now
# quarantines the offending file and recovers. We still fail loudly at
# configure time so this drift doesn't sneak back in unnoticed.

# rocksdb/version.h ships with every RocksDB release (3.x onward) and
# defines ROCKSDB_MAJOR / ROCKSDB_MINOR / ROCKSDB_PATCH. If neither
# find_package nor pkg-config exposed RocksDB_VERSION (e.g. Ubuntu 22.04's
# librocksdb-dev, which ships no CMake config and no .pc file), we can
# still recover the version directly from the header. This closes the
# "manual probe silently allows old RocksDB" gap that let v5.9.24 ship
# linked to librocksdb 6.11.
function(_tri_detect_rocksdb_version_from_header)
    if(RocksDB_VERSION)
        return()
    endif()
    foreach(_dir ${ARGN})
        if(NOT IS_DIRECTORY "${_dir}")
            continue()
        endif()
        set(_vh "${_dir}/rocksdb/version.h")
        if(EXISTS "${_vh}")
            file(STRINGS "${_vh}" _maj REGEX "^#define ROCKSDB_MAJOR ")
            file(STRINGS "${_vh}" _min REGEX "^#define ROCKSDB_MINOR ")
            file(STRINGS "${_vh}" _pat REGEX "^#define ROCKSDB_PATCH ")
            if(_maj AND _min AND _pat)
                string(REGEX MATCH "[0-9]+" _maj "${_maj}")
                string(REGEX MATCH "[0-9]+" _min "${_min}")
                string(REGEX MATCH "[0-9]+" _pat "${_pat}")
                set(RocksDB_VERSION "${_maj}.${_min}.${_pat}")
                set(RocksDB_VERSION "${_maj}.${_min}.${_pat}" PARENT_SCOPE)
                message(STATUS "Detected RocksDB version from version.h: ${RocksDB_VERSION}")
                return()
            endif()
        endif()
    endforeach()
endfunction()

if(NOT RocksDB_VERSION AND TARGET RocksDB::rocksdb)
    get_target_property(_rocksdb_inc RocksDB::rocksdb INTERFACE_INCLUDE_DIRECTORIES)
    if(_rocksdb_inc)
        _tri_detect_rocksdb_version_from_header(${_rocksdb_inc})
    endif()
endif()

if(NOT RocksDB_VERSION AND ROCKSDB_INCLUDE_DIR)
    _tri_detect_rocksdb_version_from_header(${ROCKSDB_INCLUDE_DIR})
endif()

if(RocksDB_VERSION AND RocksDB_VERSION VERSION_LESS "7.4.0")
    message(FATAL_ERROR
        "Triangles requires RocksDB >= 7.4.0 (got ${RocksDB_VERSION}). "
        "Older versions cannot read smsgDB files written by RocksDB 7.4+ "
        "(XXH3 per-block checksum). "
        "On Debian/Ubuntu: install librocksdb-dev >= 7.4 from a backports "
        "repo or build RocksDB from source into /usr/local.")
elseif(NOT RocksDB_VERSION)
    # No version detectable: headers missing entirely, or ROCKSDB_INCLUDE_DIR
    # not pointing at one with rocksdb/version.h. Runtime fallback in
    # SecMsgDB::Open covers the gap; print WARNING so build logs flag it.
    message(WARNING
        "Could not determine RocksDB version (no CMake config, no "
        "pkg-config metadata, and no rocksdb/version.h found). "
        "Triangles prefers RocksDB >= 7.4.0; older versions are recovered "
        "at runtime via SecMsgDB::Open's quarantine fallback.")
endif()

# libsecp256k1 — vendored as a git submodule under src/secp256k1. Provides
# ECDSA signing/verification, pubkey recovery (via the recovery module), and
# ECDH for secure messaging. Configure the submodule's build for our needs:
# only ECDH + recovery, none of the test/benchmark/extra-module bloat, and
# don't install (we link statically against the in-tree target).
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/src/secp256k1/CMakeLists.txt")
    message(FATAL_ERROR
        "src/secp256k1 is empty. Run: git submodule update --init --recursive")
endif()
set(SECP256K1_DISABLE_SHARED            ON  CACHE INTERNAL "")
set(SECP256K1_INSTALL                   OFF CACHE INTERNAL "")
set(SECP256K1_BUILD_BENCHMARK           OFF CACHE INTERNAL "")
set(SECP256K1_BUILD_TESTS               OFF CACHE INTERNAL "")
set(SECP256K1_BUILD_EXHAUSTIVE_TESTS    OFF CACHE INTERNAL "")
set(SECP256K1_BUILD_CTIME_TESTS         OFF CACHE INTERNAL "")
set(SECP256K1_BUILD_EXAMPLES            OFF CACHE INTERNAL "")
set(SECP256K1_ENABLE_MODULE_ECDH        ON  CACHE INTERNAL "")
set(SECP256K1_ENABLE_MODULE_RECOVERY    ON  CACHE INTERNAL "")
set(SECP256K1_ENABLE_MODULE_EXTRAKEYS   OFF CACHE INTERNAL "")
set(SECP256K1_ENABLE_MODULE_SCHNORRSIG  OFF CACHE INTERNAL "")
set(SECP256K1_ENABLE_MODULE_MUSIG       OFF CACHE INTERNAL "")
set(SECP256K1_ENABLE_MODULE_ELLSWIFT    OFF CACHE INTERNAL "")
add_subdirectory(src/secp256k1 EXCLUDE_FROM_ALL)

if(BUILD_QT)
    find_package(Qt5 5.9 REQUIRED COMPONENTS Core Gui Widgets Network)
    find_package(Qt5 COMPONENTS LinguistTools QUIET)
    if(USE_DBUS AND UNIX AND NOT APPLE)
        find_package(Qt5 COMPONENTS DBus QUIET)
        if(NOT Qt5DBus_FOUND)
            message(STATUS "Qt5 DBus not found -- disabling D-Bus notifications")
            set(USE_DBUS OFF CACHE BOOL "" FORCE)
        endif()
    else()
        set(USE_DBUS OFF CACHE BOOL "" FORCE)
    endif()
endif()

# ── Build bundled LevelDB ──
include(BuildLevelDB)

# ── Generate build.h from git describe ──
include(GenerateBuildInfo)

# ── Enable CTest at the TOP level ──
# add_test() is called in src/CMakeLists.txt, but without enable_testing()
# here the top-level build/CTestTestfile.cmake is never generated, so
# `ctest` run from the build root discovers ZERO tests. CI does exactly
# `cd build && ctest`, which means the unit suites were silently not run.
# Calling enable_testing() at the root generates the top-level test file
# that recurses into src/ and registers all four test executables.
if(BUILD_TESTS)
    enable_testing()
endif()

# ── Descend into source tree ──
add_subdirectory(src)

# ── Configuration summary ──
message(STATUS "")
message(STATUS "Triangles ${PROJECT_VERSION} build configuration:")
message(STATUS "  Build Qt GUI:       ${BUILD_QT}")
message(STATUS "  Build daemon:       ${BUILD_DAEMON}")
message(STATUS "  Build CLI:          ${BUILD_CLI}")
message(STATUS "  Build tests:        ${BUILD_TESTS}")
message(STATUS "  UPnP:               ${USE_UPNP}")
message(STATUS "  IPv6:               ${USE_IPV6}")
message(STATUS "  QR code:            ${USE_QRCODE}")
message(STATUS "  D-Bus:              ${USE_DBUS}")
message(STATUS "  ZMQ:                ${USE_ZMQ}")
message(STATUS "  Embedded Tor:       ${USE_TOR_EMBEDDED}")
message(STATUS "  Embedded I2P:       ${USE_I2P_EMBEDDED}")
message(STATUS "  Static linking:     ${ENABLE_STATIC}")
message(STATUS "  ccache:             ${CCACHE_PROGRAM}")
message(STATUS "  Unity build:        ${ENABLE_UNITY_BUILD}")
message(STATUS "  Precompiled header: ON")
message(STATUS "")
