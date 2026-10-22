file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "880c3aaef213702a9909f46f91c5d58f82e12652d056fff9e3cdb6f43859ca5a")
    message(FATAL_ERROR "Expected the recorded original native event corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 20)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native event comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
