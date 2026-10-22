file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "5932ac804a042f4ac8c67831e62a937a5346dfe6a739bd743fef2bf74fdd40a9")
    message(FATAL_ERROR "Expected the recorded original scene identifier corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Scene identifier comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
