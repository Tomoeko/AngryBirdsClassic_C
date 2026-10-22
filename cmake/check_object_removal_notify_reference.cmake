file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "3c0cb05c1d26db34130f2600dfd88ed6441a5274994995388df1a7fe607852ef")
    message(FATAL_ERROR "Expected the recorded original removal notification corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Removal notification comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
