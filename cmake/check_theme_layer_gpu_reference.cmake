file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "84b7d73bac22763a9f939a381c208883ec60a101f08c4ae1cef56ef884d3bcf7")
    message(FATAL_ERROR "Expected original theme composite/image CPU batch corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original theme image batch comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
