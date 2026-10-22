file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "ac50077387d357afc2e57c69156d9a2b02925e81df0ce7dca33aac77ffde398f")
    message(FATAL_ERROR "Expected the recorded original UI image resource corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original UI resource comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
