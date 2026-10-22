file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "4f5f40dfa7dfa092b94f5037cc2458d90a80f7f49136fe1952a911503e6e02fa")
    message(FATAL_ERROR "Expected the recorded original clipped UI text corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original clipped-text comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
