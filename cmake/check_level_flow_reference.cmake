file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "390726c460d6fe92d2fb69bbb9e597673e8a8a62efea07e15e2088482ea251d2")
    message(FATAL_ERROR "Expected the recorded original level-flow corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 10)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original level-flow comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
