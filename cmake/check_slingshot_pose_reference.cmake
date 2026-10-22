file(SHA256 "${REFERENCE}" actual)
if(NOT actual STREQUAL
    "da7cb734f198018d3382b63f1d419cdd2b9aee393cc0d784712931729f722d3c")
    message(FATAL_ERROR "Expected the pinned original sling pose trace")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native sling pose comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
