file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "0f1272a2bf17a95987c8e65540dbdcee4cfe861aaac7e6fe596c3ae9312f7e46")
    message(FATAL_ERROR "Expected the recorded original Lua asset renderer constructors")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Lua asset constructor comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
