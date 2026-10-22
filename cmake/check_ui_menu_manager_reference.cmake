file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "840cbb22f26980e4b1cfeae85409968612589092af02c1a1498879fbf679f3b8")
    message(FATAL_ERROR "Expected the recorded original menu controller corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original menu controller comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
