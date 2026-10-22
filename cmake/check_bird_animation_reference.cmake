file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "2462c692b05e7ad362a0f44df1ae455bf8cf46e6d87f7556a81f6807dfb4784e")
    message(FATAL_ERROR "Expected the recorded original bird sprite-animation corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 10)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original bird animation comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
