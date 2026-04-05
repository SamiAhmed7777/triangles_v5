set(LEVELDB_CORE_SOURCES
    db/builder.cc
    db/c.cc
    db/db_impl.cc
    db/db_iter.cc
    db/dbformat.cc
    db/dumpfile.cc
    db/filename.cc
    db/log_reader.cc
    db/log_writer.cc
    db/memtable.cc
    db/repair.cc
    db/table_cache.cc
    db/version_edit.cc
    db/version_set.cc
    db/write_batch.cc
    table/block.cc
    table/block_builder.cc
    table/filter_block.cc
    table/format.cc
    table/iterator.cc
    table/merger.cc
    table/table.cc
    table/table_builder.cc
    table/two_level_iterator.cc
    util/arena.cc
    util/bloom.cc
    util/cache.cc
    util/coding.cc
    util/comparator.cc
    util/crc32c.cc
    util/env.cc
    util/filter_policy.cc
    util/hash.cc
    util/histogram.cc
    util/logging.cc
    util/options.cc
    util/status.cc
)

if(WIN32)
    list(APPEND LEVELDB_CORE_SOURCES util/env_windows.cc)
else()
    list(APPEND LEVELDB_CORE_SOURCES util/env_posix.cc)
endif()

add_library(leveldb_lib STATIC ${LEVELDB_CORE_SOURCES})

target_compile_features(leveldb_lib PUBLIC cxx_std_11)

target_include_directories(leveldb_lib
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_compile_definitions(leveldb_lib
    PRIVATE
        HAVE_CRC32C=0
        HAVE_SNAPPY=0
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(leveldb_lib PRIVATE -fno-builtin-memcmp)
endif()

if(WIN32)
    target_compile_definitions(leveldb_lib PRIVATE
        _REENTRANT
        OS_WINDOWS
        LEVELDB_PLATFORM_WINDOWS
        WINVER=0x0500
        __USE_MINGW_ANSI_STDIO=1
    )
    target_link_libraries(leveldb_lib PUBLIC shlwapi)
elseif(APPLE)
    target_compile_definitions(leveldb_lib PRIVATE
        OS_MACOSX
        LEVELDB_PLATFORM_POSIX
    )
    target_link_libraries(leveldb_lib PUBLIC Threads::Threads)
else()
    target_compile_definitions(leveldb_lib PRIVATE
        _REENTRANT
        OS_LINUX
        LEVELDB_PLATFORM_POSIX
    )
    target_link_libraries(leveldb_lib PUBLIC Threads::Threads)
endif()

add_library(leveldb_memenv STATIC helpers/memenv/memenv.cc)

target_compile_features(leveldb_memenv PUBLIC cxx_std_11)

target_include_directories(leveldb_memenv
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/helpers"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_compile_definitions(leveldb_memenv
    PRIVATE
        HAVE_CRC32C=0
        HAVE_SNAPPY=0
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(leveldb_memenv PRIVATE -fno-builtin-memcmp)
endif()

if(WIN32)
    target_compile_definitions(leveldb_memenv PRIVATE
        _REENTRANT
        OS_WINDOWS
        LEVELDB_PLATFORM_WINDOWS
        WINVER=0x0500
        __USE_MINGW_ANSI_STDIO=1
    )
elseif(APPLE)
    target_compile_definitions(leveldb_memenv PRIVATE
        OS_MACOSX
        LEVELDB_PLATFORM_POSIX
    )
else()
    target_compile_definitions(leveldb_memenv PRIVATE
        _REENTRANT
        OS_LINUX
        LEVELDB_PLATFORM_POSIX
    )
endif()

target_link_libraries(leveldb_memenv PUBLIC leveldb_lib)

add_library(leveldb_bundled INTERFACE)

target_link_libraries(leveldb_bundled INTERFACE
    leveldb_lib
    leveldb_memenv
)

target_include_directories(leveldb_bundled INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/helpers"
)
