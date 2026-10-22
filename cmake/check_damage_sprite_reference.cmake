file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "6cf66740fa701fcd81b2bea4ace18605fd89f74e4ae90ed3a2a393fafc31b9f6")
    message(FATAL_ERROR "Expected the recorded original damage-sprite selection corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 10)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Damage-sprite selection comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
