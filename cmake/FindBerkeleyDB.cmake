# cmake/FindBerkeleyDB.cmake
# Finds Berkeley DB C++ headers and library.
#
# User can set BDB_INCLUDE_PATH and BDB_LIB_PATH to guide search.
#
# Creates imported target: BerkeleyDB::BerkeleyDB
# Sets: BerkeleyDB_FOUND, BerkeleyDB_INCLUDE_DIR, BerkeleyDB_LIBRARY

find_path(BerkeleyDB_INCLUDE_DIR
    NAMES db_cxx.h
    HINTS
        ${BDB_INCLUDE_PATH}
        ENV BDB_INCLUDE_PATH
    PATHS
        /opt/homebrew/opt/berkeley-db@5/include
        /opt/homebrew/opt/berkeley-db/include
        /usr/include/db5
        /usr/local/include/db5
        /usr/include
        /usr/local/include
        C:/msys64/mingw64/include
)

find_library(BerkeleyDB_LIBRARY
    NAMES db_cxx db_cxx-5 db_cxx-5.3 db_cxx-4.8
    HINTS
        ${BDB_LIB_PATH}
        ENV BDB_LIB_PATH
    PATHS
        /opt/homebrew/opt/berkeley-db@5/lib
        /opt/homebrew/opt/berkeley-db/lib
        /usr/lib/x86_64-linux-gnu
        /usr/lib
        /usr/local/lib
        C:/msys64/mingw64/lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(BerkeleyDB
    REQUIRED_VARS BerkeleyDB_LIBRARY BerkeleyDB_INCLUDE_DIR
)

if(BerkeleyDB_FOUND AND NOT TARGET BerkeleyDB::BerkeleyDB)
    add_library(BerkeleyDB::BerkeleyDB UNKNOWN IMPORTED)
    set_target_properties(BerkeleyDB::BerkeleyDB PROPERTIES
        IMPORTED_LOCATION "${BerkeleyDB_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${BerkeleyDB_INCLUDE_DIR}"
    )
endif()

mark_as_advanced(BerkeleyDB_INCLUDE_DIR BerkeleyDB_LIBRARY)
