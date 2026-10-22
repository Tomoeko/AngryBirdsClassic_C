file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "27ab2056884615265c0bae5fde1764cd17fac31a7bc6713a3717d28a11f03ee2")
    message(FATAL_ERROR "Expected the recorded original sized Sprite corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original sized Sprite comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
