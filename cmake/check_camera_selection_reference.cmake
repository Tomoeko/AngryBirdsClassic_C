file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "5c0020acb7ed6fa7a1b8089afece5db59f2e26459f82c7b1b0b2d5f1f3b24322")
    message(FATAL_ERROR "Expected the original camera snap/zoom selection corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original camera selection comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
