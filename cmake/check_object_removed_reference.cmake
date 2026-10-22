file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "0c96b16abc2b99b108cc02cac409f856a9cc0b0af8354b2ccf38b4cef44a3c7f")
    message(FATAL_ERROR "Expected the recorded original object-removed corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Object-removed comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
