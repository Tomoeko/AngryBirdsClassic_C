file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "efaf9fca838116975564d359cb3ad3278ef11b2140ef2c8fee0e7e96c23d0e66")
    message(FATAL_ERROR "Expected the recorded original sprite registry corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original sprite registry comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
