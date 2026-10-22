file(SHA256 "${REFERENCE}" actual)
if(NOT actual STREQUAL
    "7a870aa7fa43f3a5b108d66e144e493e483d1afa96bfd5b405a02f6fd28c8725")
    message(FATAL_ERROR "Expected the recorded original UI constructor corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original UI constructor comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
