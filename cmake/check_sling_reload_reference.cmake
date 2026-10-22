file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "00e34f6f04d86b40a8a88d5be1dc680cb70d867bd289c371b7eb4e5c401cf7bb")
    message(FATAL_ERROR "Expected the recorded original reload timer corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 10)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original reload comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
