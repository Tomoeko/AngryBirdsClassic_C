file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "794b845a42c68c2ca4d670f941ea6b80fe517d231f5ff2ef118850bae8385c25")
    message(FATAL_ERROR "Expected the recorded native contact glue queue corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native contact glue comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
