file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "45f7d618dadb40c17a6e5eb21179e06e7a4254b9e54f88a19581257ebe7dcff0")
    message(FATAL_ERROR "Expected the recorded original composite bounds corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original composite bounds comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
