file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "5ceb7db67a88b58d7831adea8f1dd52bd14c4e4b9b919399a0a064eef9c95b01")
    message(FATAL_ERROR "Expected the recorded original UI image render corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original UI image rendering comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
