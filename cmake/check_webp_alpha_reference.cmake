file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "d2d4d4048467d6caa23238ed8ee23244dc4c22cc4efc983728f7a79bf819b1bb")
    message(FATAL_ERROR "Expected the recorded original WebP alpha corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original WebP alpha comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
