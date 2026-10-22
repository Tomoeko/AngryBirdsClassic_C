file(SHA256 "${REFERENCE}" actual)
if(NOT actual STREQUAL
    "0e53dc7f30d7b2ff2019d310738f129f1d36b3e923c1f9b4367635c844ceea1d")
    message(FATAL_ERROR "Expected the pinned original sling delta trace")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native sling delta comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
