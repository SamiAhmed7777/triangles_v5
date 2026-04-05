# cmake/GenerateBuildInfoScript.cmake
# Called at build time by the custom target in GenerateBuildInfo.cmake.
# Replicates the logic of share/genbuild.sh.

# Read existing build.h first line if it exists
set(OLD_LINE "")
if(EXISTS "${OUTPUT_FILE}")
    file(STRINGS "${OUTPUT_FILE}" _lines LIMIT_COUNT 1)
    if(_lines)
        list(GET _lines 0 OLD_LINE)
    endif()
endif()

# Try exact tag match first (release builds)
execute_process(
    COMMAND git describe --tags --exact-match
    WORKING_DIRECTORY "${SOURCE_DIR}"
    OUTPUT_VARIABLE GIT_DESC
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
    RESULT_VARIABLE _result
)

# Fall back to tag + commit distance
if(NOT _result EQUAL 0)
    execute_process(
        COMMAND git describe --tags --dirty
        WORKING_DIRECTORY "${SOURCE_DIR}"
        OUTPUT_VARIABLE GIT_DESC
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE _result
    )
    if(NOT _result EQUAL 0)
        set(GIT_DESC "")
    endif()
endif()

# Get commit timestamp
execute_process(
    COMMAND git log -n 1 --format=%ci
    WORKING_DIRECTORY "${SOURCE_DIR}"
    OUTPUT_VARIABLE GIT_TIME
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

# Build new content
if(GIT_DESC)
    set(NEW_LINE "#define BUILD_DESC \"${GIT_DESC}\"")
else()
    set(NEW_LINE "// No build information available")
endif()

# Only write if changed
if(NOT "${OLD_LINE}" STREQUAL "${NEW_LINE}")
    file(WRITE "${OUTPUT_FILE}"
        "${NEW_LINE}\n"
        "#define BUILD_DATE \"${GIT_TIME}\"\n"
    )
endif()
