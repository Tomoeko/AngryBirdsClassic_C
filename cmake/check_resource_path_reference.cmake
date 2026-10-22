file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "8a7377c9cdc6e130317ede8b2d6a621dcc773eee670f8bf3dcae5375c93117ba")
    message(FATAL_ERROR "Expected the recorded original resource path corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original resource path comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
