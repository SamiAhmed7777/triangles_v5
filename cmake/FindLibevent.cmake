# cmake/FindLibevent.cmake
# Finds libevent headers and library.
#
# User can set EVENT_INCLUDE_PATH and EVENT_LIB_PATH.
#
# Creates imported target: Libevent::Libevent

find_path(Libevent_INCLUDE_DIR
    NAMES event2/event.h
    HINTS
        ${EVENT_INCLUDE_PATH}
        ENV EVENT_INCLUDE_PATH
    PATHS
        /opt/homebrew/include
        /usr/include
        /usr/local/include
        C:/msys64/mingw64/include
)

find_library(Libevent_LIBRARY
    NAMES event libevent
    HINTS
        ${EVENT_LIB_PATH}
        ENV EVENT_LIB_PATH
    PATHS
        /opt/homebrew/lib
        /usr/lib/x86_64-linux-gnu
        /usr/lib
        /usr/local/lib
        C:/msys64/mingw64/lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Libevent
    REQUIRED_VARS Libevent_LIBRARY Libevent_INCLUDE_DIR
)

if(Libevent_FOUND AND NOT TARGET Libevent::Libevent)
    add_library(Libevent::Libevent UNKNOWN IMPORTED)
    set_target_properties(Libevent::Libevent PROPERTIES
        IMPORTED_LOCATION "${Libevent_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${Libevent_INCLUDE_DIR}"
    )
endif()

mark_as_advanced(Libevent_INCLUDE_DIR Libevent_LIBRARY)
