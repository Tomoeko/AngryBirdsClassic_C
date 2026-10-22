file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "6dad58a5770dd782da27f528fa745b27d37ff7a68c5a1a3a194711b45ace066d")
    message(FATAL_ERROR "Expected the recorded original UI frame geometry corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original UI frame geometry comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
