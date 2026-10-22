file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "f8500042b8f1bd73cff67f8a719f53e42e401d0751879517e43c2ece27db9626")
    message(FATAL_ERROR "Expected the recorded original Frame draw corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original Frame draw comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
