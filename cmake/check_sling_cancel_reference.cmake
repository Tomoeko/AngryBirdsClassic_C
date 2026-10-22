file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "cfab3cfa31eb852986195f9c158c66d85c843a8b2df179e0ff584b1cd1199b46")
    message(FATAL_ERROR "Expected the recorded original sling cancellation corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 10)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Sling cancellation comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
