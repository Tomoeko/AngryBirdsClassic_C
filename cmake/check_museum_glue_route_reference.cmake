file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "12a78e9372f9d9dca3872e61a2277941804c283ad78011669e151231ab005227")
    message(FATAL_ERROR "Expected the recorded original post-damage glue corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Post-damage glue comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
