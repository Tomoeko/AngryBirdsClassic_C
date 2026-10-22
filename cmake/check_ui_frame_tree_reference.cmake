file(SHA256 "${REFERENCE}" actual)
if(NOT actual STREQUAL
    "ffd4c0b04f8cc2a07bd4d95eb8223a23bc28880022f0f8cc1c40d0ec55bc2247")
    message(FATAL_ERROR "Expected the recorded original Frame hierarchy corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original Frame hierarchy comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
