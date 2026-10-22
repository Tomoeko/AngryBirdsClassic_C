file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "56c441eab0edf3646ec9772d89a0c880c53b7b222d389c7ea7fc27a6cb94d77d")
    message(FATAL_ERROR "Expected the recorded original Android bundle metadata")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original Android bundle metadata failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
