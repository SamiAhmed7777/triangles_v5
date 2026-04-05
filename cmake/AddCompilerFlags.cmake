# cmake/AddCompilerFlags.cmake
# Shared compiler and linker flag configuration for all Triangles targets.

# ── Common warning flags ──
add_compile_options(
    -Wall -Wextra -Wno-ignored-qualifiers
    -Wformat -Wformat-security -Wno-unused-parameter
)

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
    add_link_options(-Wl,-z,relro -Wl,-z,now)
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
