file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "e843ada0fc19713548c754c21a0567c24ab178e855861682237f10a84f6584d3")
    message(FATAL_ERROR "Expected the recorded original removal particles corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Removal particles comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
