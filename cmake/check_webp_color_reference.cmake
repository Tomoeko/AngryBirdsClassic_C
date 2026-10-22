file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "76c361d95bc706091baab1d993ad243b5c69e8640bfa3e8d1a8c581eee9674e7")
    message(FATAL_ERROR "Expected the recorded original WebP RGBA line-pair corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original WebP color comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
