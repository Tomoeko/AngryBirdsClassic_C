if(KIND STREQUAL "level" OR KIND STREQUAL "created-level")
    file(SHA256 "${LEVEL}" level_hash)
    if(NOT level_hash STREQUAL
        "5f9d3c385be421a95f18ceb038d9d7d7d71eeaa982b000b83856f70d22adf8db")
        message(FATAL_ERROR "Expected the pinned pack1/Level1.lua bytecode")
    endif()
    if(KIND STREQUAL "created-level")
        set(expected_hash
            "2ade2ef35317d25e17b7a85f66e8bc1139a34aec05e232254e0acde825cbfea9")
        set(options --created-level "${LEVEL}")
    else()
        set(expected_hash
            "bcce49b9dfcb14aed78d59d44c8c5608253b45296fbc71dd668c344d3b7bfecb")
        set(options --level "${LEVEL}")
    endif()
elseif(KIND STREQUAL "growth")
    set(expected_hash
        "742cad34d7d0fbed567a448a707935599496b12ba812fc785c32c6c6bdb157e2")
    set(options)
else()
    set(expected_hash
        "170043a8e6a0da805e5257f10e230003d4842e2deea0a081210c72e1cf8316d9")
    set(options)
endif()
file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL expected_hash)
    message(FATAL_ERROR "Expected the recorded native string-order corpus")
endif()
execute_process(COMMAND "${CHECKER}" ${options} "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 10)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native string-order comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
