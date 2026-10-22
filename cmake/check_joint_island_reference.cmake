file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "e5dfab2855a38fc5fee1cd8114c601ac5b1894f3b69ae5617b3bb48e2638fd85")
    message(FATAL_ERROR "Expected the recorded native joint-island corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native joint-island comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
