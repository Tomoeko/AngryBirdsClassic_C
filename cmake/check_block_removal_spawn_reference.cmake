file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "14349755e5387a299b79b33beeb04549ab6f0643bcc4dd3988812961e7a09b0c")
    message(FATAL_ERROR "Expected the recorded original removal spawn corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Removal spawn comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
