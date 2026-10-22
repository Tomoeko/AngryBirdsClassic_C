file(SHA256 "${REFERENCE}" actual)
if(NOT actual STREQUAL
    "60a697eedffa98410d044fd3b8f7d29d4b0414e6b6f0bb2741c12616d6656b3d")
    message(FATAL_ERROR "Expected the recorded original full-class image constructor corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original image constructor comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
