cmake_minimum_required(VERSION 3.16)

# Silence CMP0167 warning (FindBoost removed in CMake 3.30+, use BoostConfig)
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

project(Triangles
    VERSION 5.9.5
    DESCRIPTION "Cryptographic Triangles Wallet"
    LANGUAGES C CXX
)

# ── C++ Standard ──
set(CMAKE_CXX_STANDARD 17)
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

# ── Output directories ──
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# ── Custom module path ──
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# ── User-facing options ──
option(BUILD_QT           "Build triangles-qt (Qt5 GUI wallet)"              ON)
option(BUILD_DAEMON       "Build trianglesd (headless daemon)"               ON)
option(BUILD_TESTS        "Build test_triangles (Boost.Test unit tests)"     ON)
option(USE_UPNP           "Enable UPnP support via miniupnpc"               ON)
option(USE_IPV6           "Enable IPv6 support"                              ON)
option(USE_QRCODE         "Enable QR code generation via libqrencode"        OFF)
option(USE_DBUS           "Enable D-Bus notifications (Linux only)"          ON)
option(USE_ZMQ            "Enable ZMQ publisher support"                     OFF)
option(USE_TOR_EMBEDDED   "Enable embedded Tor library linking"              OFF)
option(USE_O3             "Use -O3 optimization instead of -O2"              OFF)
option(ENABLE_PIE         "Build position-independent executables"           OFF)
option(ENABLE_STATIC      "Prefer static linking (Linux release builds)"     OFF)

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
    filesystem program_options thread chrono
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

if(BUILD_QT)
    find_package(Qt5 5.9 REQUIRED COMPONENTS Core Gui Widgets)
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

# ── Descend into source tree ──
add_subdirectory(src)

# ── Configuration summary ──
message(STATUS "")
message(STATUS "Triangles ${PROJECT_VERSION} build configuration:")
message(STATUS "  Build Qt GUI:       ${BUILD_QT}")
message(STATUS "  Build daemon:       ${BUILD_DAEMON}")
message(STATUS "  Build tests:        ${BUILD_TESTS}")
message(STATUS "  UPnP:               ${USE_UPNP}")
message(STATUS "  IPv6:               ${USE_IPV6}")
message(STATUS "  QR code:            ${USE_QRCODE}")
message(STATUS "  D-Bus:              ${USE_DBUS}")
message(STATUS "  ZMQ:                ${USE_ZMQ}")
message(STATUS "  Embedded Tor:       ${USE_TOR_EMBEDDED}")
message(STATUS "  Static linking:     ${ENABLE_STATIC}")
message(STATUS "  ccache:             ${CCACHE_PROGRAM}")
message(STATUS "  Unity build:        ${ENABLE_UNITY_BUILD}")
message(STATUS "  Precompiled header: ON")
message(STATUS "")
