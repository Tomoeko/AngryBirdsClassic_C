file(SHA256 "${REFERENCE}" actual)
if(NOT actual STREQUAL
    "e4af773867013a6a4eefc7d25804594d00ef280bc6984b88843745377043a8b4")
    message(FATAL_ERROR "Expected the pinned original Mighty Eagle timer trace")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native Mighty Eagle timer comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
