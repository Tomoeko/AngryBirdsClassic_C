file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "645189c623c000853cff34e8776b79f468ade777e7ad74f8d89d8560eef07be3")
    message(FATAL_ERROR "Expected the recorded original resource owner corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original resource owner comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
