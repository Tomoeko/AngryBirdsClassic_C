file(SHA256 "${REFERENCE}" actual)
if(NOT actual STREQUAL
    "49ae7858b3cf1772c35b27bf7d2916499a69d93f12a19c2b503b0aa4195be1b4")
    message(FATAL_ERROR "Expected the pinned original rubberband world trace")
endif()
execute_process(COMMAND "${CHECKER}" --world "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native rubberband world comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
