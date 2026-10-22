file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "647b59accb99ddbb26ea3c9bae788f40a3be777e8976c0aa7d1ba1177c27b175")
    message(FATAL_ERROR "Expected the recorded original bird specialty corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Bird specialty comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
