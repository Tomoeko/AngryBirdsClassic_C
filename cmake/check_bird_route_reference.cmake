file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "997e304c554bbca13edd7a203821ae4d786a792dbbeeeeb6b1f513023458646e")
    message(FATAL_ERROR "Expected the recorded original-Lua bird routing corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 10)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Enclosing bird collision comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
