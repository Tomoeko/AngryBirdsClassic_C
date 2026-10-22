file(SHA256 "${REFERENCE}" actual)
if(NOT actual STREQUAL
    "894e4b4a5c0a39ca38fe3f9a7ef2bb1a85e3b1b107edb12919f56d4bca3af03a")
    message(FATAL_ERROR "Expected the pinned original sling draw trace")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native sling draw comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
