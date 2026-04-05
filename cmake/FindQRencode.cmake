# cmake/FindQRencode.cmake
# Finds libqrencode headers and library.
#
# Creates imported target: QRencode::QRencode

find_path(QRencode_INCLUDE_DIR
    NAMES qrencode.h
    PATHS
        /opt/homebrew/include
        /usr/include
        /usr/local/include
        C:/msys64/mingw64/include
)

find_library(QRencode_LIBRARY
    NAMES qrencode
    PATHS
        /opt/homebrew/lib
        /usr/lib/x86_64-linux-gnu
        /usr/lib
        /usr/local/lib
        C:/msys64/mingw64/lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(QRencode
    REQUIRED_VARS QRencode_LIBRARY QRencode_INCLUDE_DIR
)

if(QRencode_FOUND AND NOT TARGET QRencode::QRencode)
    add_library(QRencode::QRencode UNKNOWN IMPORTED)
    set_target_properties(QRencode::QRencode PROPERTIES
        IMPORTED_LOCATION "${QRencode_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${QRencode_INCLUDE_DIR}"
    )
endif()

mark_as_advanced(QRencode_INCLUDE_DIR QRencode_LIBRARY)
