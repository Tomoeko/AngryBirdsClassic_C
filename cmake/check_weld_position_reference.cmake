file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "d764da8f21990f9e9309e43ba280cb5edb30d082a7b953e6a3c06714f1b72f7f")
    message(FATAL_ERROR "Expected the recorded native weld position corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 20)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native weld position comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
