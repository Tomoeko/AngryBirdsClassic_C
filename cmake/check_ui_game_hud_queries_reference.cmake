file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "164f6cbd3a3035214d1d4b18cd17930a453536d5211ae0aba9c23b2c4f078cbd")
    message(FATAL_ERROR "Expected the recorded original GameHud predicate corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "GameHud predicate comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
