# cmake/FindMiniupnpc.cmake
# Finds miniupnpc headers and library.
#
# User can set MINIUPNPC_INCLUDE_PATH and MINIUPNPC_LIB_PATH.
#
# Creates imported target: Miniupnpc::Miniupnpc

find_path(Miniupnpc_INCLUDE_DIR
    NAMES miniupnpc/miniupnpc.h
    HINTS
        ${MINIUPNPC_INCLUDE_PATH}
        ENV MINIUPNPC_INCLUDE_PATH
    PATHS
        /opt/homebrew/include
        /usr/include
        /usr/local/include
        C:/msys64/mingw64/include
)

find_library(Miniupnpc_LIBRARY
    NAMES miniupnpc
    HINTS
        ${MINIUPNPC_LIB_PATH}
        ENV MINIUPNPC_LIB_PATH
    PATHS
        /opt/homebrew/lib
        /usr/lib/x86_64-linux-gnu
        /usr/lib
        /usr/local/lib
        C:/msys64/mingw64/lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Miniupnpc
    REQUIRED_VARS Miniupnpc_LIBRARY Miniupnpc_INCLUDE_DIR
)

if(Miniupnpc_FOUND AND NOT TARGET Miniupnpc::Miniupnpc)
    add_library(Miniupnpc::Miniupnpc UNKNOWN IMPORTED)
    set_target_properties(Miniupnpc::Miniupnpc PROPERTIES
        IMPORTED_LOCATION "${Miniupnpc_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${Miniupnpc_INCLUDE_DIR}"
    )
endif()

mark_as_advanced(Miniupnpc_INCLUDE_DIR Miniupnpc_LIBRARY)
