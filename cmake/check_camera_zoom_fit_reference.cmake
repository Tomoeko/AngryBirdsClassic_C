file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "2809a3290c05e53c47b3ffe9627587e9d6f7c7dab66a4d0d578a5f13f9cb067b")
    message(FATAL_ERROR "Expected the recorded original projected camera zoom-fit corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Camera zoom-fit comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
