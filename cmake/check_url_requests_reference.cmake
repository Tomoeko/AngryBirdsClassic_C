file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "ab0a7decbd59f49299b38170fa32e3595e31a66b2860cf7cb27f437097fda4d6")
    message(FATAL_ERROR "Expected the recorded original URL request corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 20)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "URL request comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
