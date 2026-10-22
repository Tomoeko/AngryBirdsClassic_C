file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "a44477045d30b96b62fcf97161ca68432aa6053943fca4f1caf7021a579f275b")
    message(FATAL_ERROR "Expected the recorded original museum glue helper corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Museum glue comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
