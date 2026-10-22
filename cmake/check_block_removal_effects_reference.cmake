file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "f9dbb69948a818ee30de714a2ab964c0097d8634a449a6aaf1eaa6bdf1d02196")
    message(FATAL_ERROR "Expected the recorded original removal effects corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Removal effects comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
