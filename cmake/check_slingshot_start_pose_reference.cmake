file(SHA256 "${REFERENCE}" actual)
if(NOT actual STREQUAL
    "5d225deac57e27516c37e15f9e2ebed59eed30720a2fdd3bd863a0ea6986757e")
    message(FATAL_ERROR "Expected the pinned original sling start pose trace")
endif()
execute_process(COMMAND "${CHECKER}" --start "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native sling start pose comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
