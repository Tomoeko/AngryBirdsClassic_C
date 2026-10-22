file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "d1b8582a9b6ce1f0892e005eb8b7472a05549ba1fb0ae085880480b23d1ef5e6")
    message(FATAL_ERROR "Expected the recorded original bird impact predictions")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Bird impact prediction comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
