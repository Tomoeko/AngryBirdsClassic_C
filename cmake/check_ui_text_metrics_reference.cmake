file(SHA256 "${REFERENCE}" actual)
if(NOT actual STREQUAL
    "c867776ee5c81b464b66405803c269a9f6c6a12f966b36d0b42e2e93dddf8dbd")
    message(FATAL_ERROR "Expected the recorded original Text metric corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original Text metric comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
