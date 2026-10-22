file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "13b282808ec748d9caa5e76f1c03c2aa5df183109043a19b13b2be7c047671e4")
    message(FATAL_ERROR "Expected the recorded original camera sequence manager corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original camera sequence comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
