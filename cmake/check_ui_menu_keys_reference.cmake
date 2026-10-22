file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "0a1eda41d412eeaed30ca8f89d1ca67cb8eb0d3e10a863b8710834ef25ca23fc")
    message(FATAL_ERROR "Expected the recorded original menu keyboard corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original menu keyboard comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
