file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "0cbc1aa19f05445f40797920ebfe71bf8d5f77bde1561ec0ca24c7e783287fe6")
    message(FATAL_ERROR "Expected the original VP8 loop filter corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original VP8 loop filter comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
