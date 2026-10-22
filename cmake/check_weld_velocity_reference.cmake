file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "42ea82fa75d3c7f7cadd7590b8508531728b8003ea7123071dc813b3ebe4255f")
    message(FATAL_ERROR "Expected the recorded native weld velocity corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 20)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native weld velocity comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
