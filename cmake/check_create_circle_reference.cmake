file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "d1693570d133b92906eadee26e576f4316c944191dc1d785d5cf641174ae82d8")
    message(FATAL_ERROR "Expected the recorded original Android createCircleLua corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 20)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native circle comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
