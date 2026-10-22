file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "b1ea815cd2383c8ba70564e87a1e05cb7775a7b56afb52665a4b8b4b92826dcc")
    message(FATAL_ERROR "Expected the recorded original completion-transition corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 10)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original completion transition failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
