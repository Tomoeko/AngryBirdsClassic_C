file(SHA256 "${REFERENCE}" reference_hash)
if(KIND STREQUAL "live")
    set(expected_hash
        "9c849081755e60656e4e304917f95339af13a133aa79dffdd2ec6f807026e22f")
    set(options --live)
else()
    set(expected_hash
        "13ba4518d4b436be0a4f5b4d99d440b612050874c45f3bc48f75110b5d45038d")
endif()
if(NOT reference_hash STREQUAL expected_hash)
    message(FATAL_ERROR "Expected the recorded original camera viewport corpus")
endif()
execute_process(COMMAND "${CHECKER}" ${options} "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 10)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original camera viewport comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
