file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "6b9b21134e4e8d65386c37ba2038d68128a1dac8072480c57019ea667aec85c9")
    message(FATAL_ERROR "Expected the recorded original VP8 arithmetic-field corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original VP8 arithmetic comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
