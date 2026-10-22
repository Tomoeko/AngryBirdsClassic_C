file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "5ca067ad722a4cefe6df9a5e77942d7f696538826458097fb59e8d80919f19d7")
    message(FATAL_ERROR "Expected the recorded original behavior construction corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Behavior construction comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
