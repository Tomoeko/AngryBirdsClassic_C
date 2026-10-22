file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "fd5741818ec09e622be0ab5630ae325fe881f335303c8926adb4950b1dd9da47")
    message(FATAL_ERROR "Expected the recorded original Museum/pause query corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 20)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Museum/pause query comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
