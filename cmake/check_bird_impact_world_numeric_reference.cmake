file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "332ed89cd643e651ce316a9342d68831a089986299cdf01b12cdb019ba5b0be3")
    message(FATAL_ERROR "Expected the recorded original vertical impact prediction corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Vertical impact prediction comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
