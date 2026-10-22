file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "777c781d84d06a080e00497666443aebbc68b33a62e2aae6bfdc2720b05f61a4")
    message(FATAL_ERROR "Expected the recorded native object-friction corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 10)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native object-friction comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
