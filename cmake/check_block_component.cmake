file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "3f5d8d1955bea03a0efed919f78fedc8f470e9a0066e18ec6de9f18e553a85f2")
    message(FATAL_ERROR "Expected the recorded original Android component predicates")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 15)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native component comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
