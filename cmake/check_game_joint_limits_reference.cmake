file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "17562d91a1700b99383faf6b28defe40cee074b41df702a7abc8d20a35120fdb")
    message(FATAL_ERROR "Expected the recorded native joint coordinate/motor-limit corpus")
endif()
execute_process(COMMAND "${CHECKER}" --limits "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native motor-limit comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
