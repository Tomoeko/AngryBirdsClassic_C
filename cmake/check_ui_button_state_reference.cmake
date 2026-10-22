file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "6badfc6f93810d9b567b6b923424d7d7ffb67ef0185ca4f29df14a3cfdc562ca")
    message(FATAL_ERROR "Expected the recorded original typed button state corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Original typed button comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
