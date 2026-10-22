file(SHA256 "${REFERENCE}" actual)
if(NOT actual STREQUAL
    "c5e1836411f5ca7bdb0d313ca9e54b219053d239377af04b9366d68ecced0883")
    message(FATAL_ERROR "Expected the recorded original plain tostring corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original plain tostring comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
