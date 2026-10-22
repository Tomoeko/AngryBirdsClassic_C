file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "d2d893be59fe39867f0ee55658ed210e00cdc808250c964ca3cecd3f2042461d")
    message(FATAL_ERROR "Expected the recorded original bubble contact corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Bubble contact comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
