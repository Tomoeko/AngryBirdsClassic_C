file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "9e61320b5fb185e722ef89782876aa4619628cf5f428591b2dbb16cfb7b935fa")
    message(FATAL_ERROR "Expected the original playAudio request reference")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original audio-play comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
