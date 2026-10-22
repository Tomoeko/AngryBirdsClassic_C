file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "670255191c837cd9dcf39ac398b7c7e6a30755dc891df30a471f13587fad00fc")
    message(FATAL_ERROR "Expected the recorded original base level-effect corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Base level-effect comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
