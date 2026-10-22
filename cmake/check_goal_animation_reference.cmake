file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "796a47e4d942dad8442332f20cbdf472df207e1dbc47df27d9edb375df866517")
    message(FATAL_ERROR "Expected the recorded original goal animation corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 10)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original goal animation comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
