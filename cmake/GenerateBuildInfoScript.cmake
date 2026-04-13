# cmake/GenerateBuildInfoScript.cmake
# Called at build time by the custom target in GenerateBuildInfo.cmake.
# Reads the version from clientversion.h (single source of truth) and
# appends git commit info for non-release builds.

# Read existing build.h first line if it exists
set(OLD_LINE "")
if(EXISTS "${OUTPUT_FILE}")
    file(STRINGS "${OUTPUT_FILE}" _lines LIMIT_COUNT 1)
    if(_lines)
        list(GET _lines 0 OLD_LINE)
    endif()
endif()

# ── Read version from clientversion.h ──
file(STRINGS "${SOURCE_DIR}/src/clientversion.h" _ver_lines)
foreach(_line ${_ver_lines})
    if(_line MATCHES "^#define CLIENT_VERSION_MAJOR +([0-9]+)")
        set(VER_MAJOR "${CMAKE_MATCH_1}")
    elseif(_line MATCHES "^#define CLIENT_VERSION_MINOR +([0-9]+)")
        set(VER_MINOR "${CMAKE_MATCH_1}")
    elseif(_line MATCHES "^#define CLIENT_VERSION_REVISION +([0-9]+)")
        set(VER_REVISION "${CMAKE_MATCH_1}")
    elseif(_line MATCHES "^#define CLIENT_VERSION_BUILD +([0-9]+)")
        set(VER_BUILD "${CMAKE_MATCH_1}")
    endif()
endforeach()

set(BASE_VERSION "v${VER_MAJOR}.${VER_MINOR}.${VER_REVISION}.${VER_BUILD}")

# ── Get git commit info (suffix only, not the version number) ──
set(GIT_SUFFIX "")

# Get short commit hash
execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY "${SOURCE_DIR}"
    OUTPUT_VARIABLE GIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
    RESULT_VARIABLE _result
)

if(_result EQUAL 0 AND GIT_HASH)
    # Check if working directory is dirty
    execute_process(
        COMMAND git diff-index --quiet HEAD --
        WORKING_DIRECTORY "${SOURCE_DIR}"
        RESULT_VARIABLE _dirty
    )

    # Check if HEAD is exactly on a tag matching our version
    execute_process(
        COMMAND git describe --tags --exact-match HEAD
        WORKING_DIRECTORY "${SOURCE_DIR}"
        OUTPUT_VARIABLE GIT_TAG
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE _tag_result
    )

    set(_on_release_tag FALSE)
    if(_tag_result EQUAL 0 AND GIT_TAG STREQUAL "${BASE_VERSION}")
        set(_on_release_tag TRUE)
    endif()

    # Only add git suffix for non-release builds (not on exact version tag, or dirty)
    if(NOT _on_release_tag OR NOT _dirty EQUAL 0)
        set(GIT_SUFFIX "-g${GIT_HASH}")
        if(NOT _dirty EQUAL 0)
            set(GIT_SUFFIX "${GIT_SUFFIX}-dirty")
        endif()
    endif()
endif()

set(FULL_VERSION "${BASE_VERSION}${GIT_SUFFIX}")

# Get commit timestamp
execute_process(
    COMMAND git log -n 1 --format=%ci
    WORKING_DIRECTORY "${SOURCE_DIR}"
    OUTPUT_VARIABLE GIT_TIME
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

# Build new content
set(NEW_LINE "#define BUILD_DESC \"${FULL_VERSION}\"")

# Only write if changed
if(NOT "${OLD_LINE}" STREQUAL "${NEW_LINE}")
    file(WRITE "${OUTPUT_FILE}"
        "${NEW_LINE}\n"
        "#define BUILD_DATE \"${GIT_TIME}\"\n"
    )
endif()
