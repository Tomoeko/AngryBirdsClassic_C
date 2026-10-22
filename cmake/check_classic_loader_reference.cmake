file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "e50af15560c4045430462927f6f2c7788f8c8d9c8ac41bb60b50d8185d6c59e5")
    message(FATAL_ERROR "Expected the recorded original Loader corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 20)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Loader comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
