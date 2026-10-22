file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "7f7e28a2a6ae115b4836d2953dec183e272e99493383732ba2ec601a7411b724")
    message(FATAL_ERROR "Expected the recorded original EventManager corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 10)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Event manager comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
