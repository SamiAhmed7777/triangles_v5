# CMake toolchain file for cross-compiling Triangles for Windows x64 using MinGW on Linux
# Usage: cmake -DCMAKE_TOOLCHAIN_FILE=cmake/mingw64.cmake -B build-mingw -S .

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# MinGW toolchain
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)

# Search for programs only in the build host directories
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

# Search for libraries and headers only in the staging directory
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# Staging prefix — all dependencies installed here
set(DEP_PREFIX "${CMAKE_SOURCE_DIR}/deps-mingw")

# Windows libraries
set(CMAKE_LIBRARY_PATH "${DEP_PREFIX}/lib")

# Include directories
set(CMAKE_INCLUDE_PATH "${DEP_PREFIX}/include")

# Windows sysroot (MinGW libraries, headers, and tools)
set(MINGW_SYSROOT /usr/x86_64-w64-mingw32)

# Don't search the host system for programs
set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32 ${DEP_PREFIX})

# For find_package(OpenSSL), find_package(Boost), etc.
# Only search deps-mingw and MinGW sysroot — NOT the host system
set(CMAKE_SYSROOT "${MINGW_SYSROOT}")
set(OPENSSL_ROOT_DIR "${DEP_PREFIX}")
set(BOOST_ROOT "${DEP_PREFIX}")
set(CMAKE_PREFIX_PATH "${DEP_PREFIX}")

# Critical: prevent Linux host headers from leaking into MinGW compilation
# The MinGW cross-compiler should ONLY see MinGW and deps headers
set(CMAKE_C_STANDARD_INCLUDE_DIRECTORIES "")
set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES "")

# Add MinGW and deps include paths explicitly
include_directories(BEFORE SYSTEM
  "${DEP_PREFIX}/include"
  "${MINGW_SYSROOT}/include"
  "${MINGW_SYSROOT}/include/c++"
  "${MINGW_SYSROOT}/include/sec_api"
)

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# C++20 for the project
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build settings
set(BUILD_DAEMON ON)
set(BUILD_QT OFF)
set(BUILD_TESTS OFF)
set(USE_UPNP OFF)
set(USE_QRCODE OFF)
set(USE_ZMQ OFF)
set(USE_DBUS OFF)
set(USE_TOR_EMBEDDED OFF)
