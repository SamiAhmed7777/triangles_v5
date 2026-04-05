# cmake/GenerateBuildInfo.cmake
# Sets up a custom target that generates build.h from git describe,
# equivalent to share/genbuild.sh.

set(BUILD_HEADER_DIR "${CMAKE_BINARY_DIR}/generated")
set(BUILD_HEADER     "${BUILD_HEADER_DIR}/build.h")
file(MAKE_DIRECTORY  "${BUILD_HEADER_DIR}")

# Custom command runs on every build to regenerate build.h if git state changed
add_custom_target(generate_build_info ALL
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
        -DOUTPUT_FILE=${BUILD_HEADER}
        -P "${CMAKE_SOURCE_DIR}/cmake/GenerateBuildInfoScript.cmake"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Generating build.h from git describe..."
    BYPRODUCTS "${BUILD_HEADER}"
    VERBATIM
)
