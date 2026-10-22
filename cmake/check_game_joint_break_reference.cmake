file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "e7abe85c31cc751d042a0ffb909fdb7413bc7aba236d8ed3d363a21cd6bf1075")
    message(FATAL_ERROR "Expected the recorded native deferred joint-breakage corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native joint-breakage comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
