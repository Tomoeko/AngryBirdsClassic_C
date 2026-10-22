file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "7d1333abba29e012c9c35c297216280ac48e4fcf4664fc9e2cec85d4e0313185")
    message(FATAL_ERROR "Expected the recorded original level-start flow corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 20)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Level-start flow comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
