# cmake/BuildLevelDB.cmake
# Builds the bundled LevelDB via its native CMake sub-build.
# Exposes leveldb_lib, leveldb_memenv, leveldb_bundled, and build_leveldb.

set(LEVELDB_SOURCE_DIR "${CMAKE_SOURCE_DIR}/src/leveldb")
set(LEVELDB_BINARY_DIR "${CMAKE_BINARY_DIR}/leveldb")

if(NOT TARGET leveldb_lib)
    add_subdirectory("${LEVELDB_SOURCE_DIR}" "${LEVELDB_BINARY_DIR}")
endif()

# Pin bundled LevelDB to C++17. It only needs C++11 (declared via its own
# target_compile_features) but inherits CMAKE_CXX_STANDARD=20 from the
# top-level project, where some of its atomic-enum syntax
# (std::memory_order::memory_order_relaxed) becomes a hard error.
foreach(_leveldb_target leveldb_lib leveldb_memenv)
    if(TARGET ${_leveldb_target})
        set_target_properties(${_leveldb_target} PROPERTIES
            CXX_STANDARD 17
            CXX_STANDARD_REQUIRED ON
            CXX_EXTENSIONS OFF
        )
    endif()
endforeach()

if(NOT TARGET build_leveldb)
    add_custom_target(build_leveldb DEPENDS leveldb_lib leveldb_memenv)
endif()
