# cmake/BuildLevelDB.cmake
# Builds the bundled LevelDB via its native CMake sub-build.
# Exposes leveldb_lib, leveldb_memenv, leveldb_bundled, and build_leveldb.

set(LEVELDB_SOURCE_DIR "${CMAKE_SOURCE_DIR}/src/leveldb")
set(LEVELDB_BINARY_DIR "${CMAKE_BINARY_DIR}/leveldb")

if(NOT TARGET leveldb_lib)
    add_subdirectory("${LEVELDB_SOURCE_DIR}" "${LEVELDB_BINARY_DIR}")
endif()

if(NOT TARGET build_leveldb)
    add_custom_target(build_leveldb DEPENDS leveldb_lib leveldb_memenv)
endif()
