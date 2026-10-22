file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "88f0cadd8f0a60fc5dd2e095ef5ca6237ef0e34f539cde90a5ca8ec50b048f62")
    message(FATAL_ERROR "Expected the recorded original shot listener corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 10)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Shot listener comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
