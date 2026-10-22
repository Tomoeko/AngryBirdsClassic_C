file(SHA256 "${REFERENCE}" actual)
if(NOT actual STREQUAL
    "d7a0dc85a86a5e1bbb37c0581bd88513a82b492d98581873aab0f6adfcf65942")
    message(FATAL_ERROR "Expected the pinned original mouse sling input trace")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native mouse sling comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
