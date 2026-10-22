file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "6f135c874a83cdea29cee877b184106e0ecbe0a010891c8dee70473da6830d59")
    message(FATAL_ERROR "Expected the recorded native bitmap-font metric corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 10)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native font metric comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
