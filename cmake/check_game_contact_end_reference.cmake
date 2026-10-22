file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "560114c5f6daba7e0490f2bc37692fb281d4b0b7d06a2898dad68b67f7be7dda")
    message(FATAL_ERROR "Expected the recorded native end-contact corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 10)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native end-contact comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
