file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "6ce364debf49d4b66405be9e1132c9e0312f3e1940224a99382b8a2724f7fe5d")
    message(FATAL_ERROR "Expected the recorded original world next corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original world next comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
