file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "4e832133ca38e8a7bbce9ffa821a44284977bb21e53ac10f1446aff90da92fad")
    message(FATAL_ERROR "Expected the recorded native symmetric matrix inverse corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 20)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native matrix inverse comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
