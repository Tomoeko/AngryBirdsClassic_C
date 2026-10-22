file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "967365dde8f1a6526ad2481173e6bdbe8bc193f3c163bf095f80b3207e1e3058")
    message(FATAL_ERROR "Expected the recorded original ImageButton state corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original ImageButton comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
