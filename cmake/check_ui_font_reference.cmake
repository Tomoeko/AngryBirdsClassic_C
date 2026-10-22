file(SHA256 "${REFERENCE}" actual)
if(NOT actual STREQUAL
    "9d7200dd5969f93b313133e960303fd6e1f7e8bb5992179f472bbe1cd2a00459")
    message(FATAL_ERROR "Expected the recorded original Text font getter corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original Text font getter comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
