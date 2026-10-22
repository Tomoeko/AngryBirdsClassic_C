file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "be6eff66785309874c1af663f7ba3f4b68209b00f8e9751bd5e68ade2c53caab")
    message(FATAL_ERROR "Expected the recorded original theme constructor corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original theme constructor comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
