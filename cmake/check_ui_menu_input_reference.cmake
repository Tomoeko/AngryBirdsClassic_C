file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "a5136da9cc0094a1161ca3ce1456e55e74da9d9bbbeab7c00861d58fdce7d023")
    message(FATAL_ERROR "Expected the recorded original menu pointer routing corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original menu pointer comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
