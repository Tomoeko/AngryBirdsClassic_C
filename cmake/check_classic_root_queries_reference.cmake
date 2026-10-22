file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "26806827b75d7b83e3bc3061558011b1beae8a2f8c0b8fa5d9101172438f48d6")
    message(FATAL_ERROR "Expected the recorded original root menu/eagle query corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 20)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Root query comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
