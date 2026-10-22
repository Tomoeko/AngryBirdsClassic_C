file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "1bcccd444a70c631b0a45cecc3176ee7b904d1dbcacd2bbac7b47e718b12db76")
    message(FATAL_ERROR "Expected the recorded original Museum bird behavior corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Museum behavior comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
