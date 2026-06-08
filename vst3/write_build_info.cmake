if(NOT DEFINED OUTPUT_FILE)
    message(FATAL_ERROR "OUTPUT_FILE is required")
endif()

if(NOT DEFINED PROJECT_ROOT)
    set(PROJECT_ROOT "")
endif()

if(NOT DEFINED CONFIG)
    set(CONFIG "Unknown")
endif()

if(NOT DEFINED BUNDLE_BINARY)
    set(BUNDLE_BINARY "Unknown")
endif()

string(TIMESTAMP BUILD_LOCAL "%Y-%m-%d %H:%M:%S %Z")
string(TIMESTAMP BUILD_UTC "%Y-%m-%d %H:%M:%S UTC" UTC)

set(GIT_SHA "unknown")
set(GIT_TREE "unknown")
if(PROJECT_ROOT)
    execute_process(
        COMMAND git -C "${PROJECT_ROOT}" rev-parse --short HEAD
        OUTPUT_VARIABLE GIT_SHA_RESULT
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(GIT_SHA_RESULT)
        set(GIT_SHA "${GIT_SHA_RESULT}")
    endif()

    execute_process(
        COMMAND git -C "${PROJECT_ROOT}" status --porcelain
        OUTPUT_VARIABLE GIT_STATUS_RESULT
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(GIT_STATUS_RESULT STREQUAL "")
        set(GIT_TREE "clean")
    else()
        set(GIT_TREE "dirty")
    endif()
endif()

file(WRITE "${OUTPUT_FILE}"
"OverFilter build info
Built local: ${BUILD_LOCAL}
Built UTC: ${BUILD_UTC}
Config: ${CONFIG}
Binary: ${BUNDLE_BINARY}
Git commit: ${GIT_SHA}
Git tree: ${GIT_TREE}
")
