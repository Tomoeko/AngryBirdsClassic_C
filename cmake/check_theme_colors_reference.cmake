file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "78c2737f496a9996115ebdc02e449f17af75e588cb19ae2ddaf3efa632a37788")
    message(FATAL_ERROR "Expected original ThemeComposition color constructors")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original theme color construction failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
