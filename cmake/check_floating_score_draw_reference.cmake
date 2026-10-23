file(SHA256 "${REFERENCE}" actual)
if(NOT actual STREQUAL
    "c546a183792cc65765df5eac42b1254f12da4d54fdc1ee7f9ef3a532c892dc69")
    message(FATAL_ERROR "Expected the pinned original floating-score draw trace")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native floating-score comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
