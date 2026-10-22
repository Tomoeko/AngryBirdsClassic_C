file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "5a03c03ae67198c768e40af0a49e1baf09fd4abdec20e4673d5a0f5f4cbe0ead")
    message(FATAL_ERROR "Expected the recorded original text wrapping corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original wrapping comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
