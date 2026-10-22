file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "aa7d424300fca3252da2ebcae6b7a838afb9b31fd2c413f282628dd9ccdd5274")
    message(FATAL_ERROR "Expected the recorded original named game raycast corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Game raycast comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
