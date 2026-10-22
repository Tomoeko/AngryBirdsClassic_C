file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "2d23f804a349987b79e3836c07bdba8c8038fc0de092fb2869e6c16a1c09cec5")
    message(FATAL_ERROR "Expected the recorded original GameLua frame clock corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 10)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native frame clock comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
