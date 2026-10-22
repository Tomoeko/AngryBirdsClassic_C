file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "62b98fe3b76961be778e0127cfb67ab1e13f8e7034580ccfae86c9a293b69ae7")
    message(FATAL_ERROR "Expected the recorded original Blues trajectory methods")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 10)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Blues trajectory comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
