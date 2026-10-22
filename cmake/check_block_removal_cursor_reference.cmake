file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "afa2998d39548b897f51177d015eef5c4d1403f96731920b4d0dd5626cfff05b")
    message(FATAL_ERROR "Expected the recorded original removal traversal corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Removal traversal comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
