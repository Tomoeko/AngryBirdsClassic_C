file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "06297a2085f8156f61f0a2d4ba0ba9387411dd34f8d1485e7aee688ef77da1d2")
    message(FATAL_ERROR "Expected the original VP8 predictor corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original VP8 predictor comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
