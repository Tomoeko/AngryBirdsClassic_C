file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "30b2b0ae86080bfbc0271ccacb957e3e67dfa87400d3c2a0f83b61768a67f594")
    message(FATAL_ERROR "Expected the recorded native game joint constructor corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native game joint comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
