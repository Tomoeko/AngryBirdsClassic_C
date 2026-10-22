file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "df36c3a38fdb7f636c91c7dcf6ad7cc8fc72e7dd5261c692794cd39282430489")
    message(FATAL_ERROR "Expected the recorded Android distance-query corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}" RESULT_VARIABLE status
    OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 10)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Android distance comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
