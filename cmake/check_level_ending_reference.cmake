file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "3dbbed8c841ec724b3885d2ba247f8c3f23fe94befd90ce1303a9ccefe22f3c6")
    message(FATAL_ERROR "Expected the recorded original ending-delay corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 10)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original ending-delay comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
