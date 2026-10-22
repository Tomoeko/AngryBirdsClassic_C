file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "b11dc56aa1c4626f7b03eae5f90abd6d65b6570e737e59313cf1c0b4ef66e79c")
    message(FATAL_ERROR "Expected the recorded original resource cache-hit corpus")
endif()
execute_process(COMMAND "${CHECKER}" --cache "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original resource cache comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
