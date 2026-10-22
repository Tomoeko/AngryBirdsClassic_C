file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "ca4e584ec3dc98d2170086399174f60208dd913b84421b05d1e0c15994e88fbd")
    message(FATAL_ERROR "Expected the recorded original camera mover corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original camera mover comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
