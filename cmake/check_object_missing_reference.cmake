file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "ee1025a10b2f27f4b5b618ee936d72d0c17c2c9d9af6865c001c4d03dfc1b508")
    message(FATAL_ERROR "Expected the original ObjectCreation missing-definition corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native missing-definition comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
