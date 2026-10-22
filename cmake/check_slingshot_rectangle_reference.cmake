file(SHA256 "${REFERENCE}" actual)
if(NOT actual STREQUAL
    "34822d501e6d61703ac4409c524ef19585c70ecc3fedc44a5de903ce9e93ee14")
    message(FATAL_ERROR "Expected the pinned original sling rectangle trace")
endif()
execute_process(COMMAND "${CHECKER}" --rectangles "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native sling rectangle comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
