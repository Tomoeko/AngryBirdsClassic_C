file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "1dfa7956f108ee54425c6a874cea4842121936b970bc92b6ae3b1380384b4af2")
    message(FATAL_ERROR "Expected the recorded original-Lua bird impact corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}" RESULT_VARIABLE status
    OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 10)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original-Lua bird impact comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
