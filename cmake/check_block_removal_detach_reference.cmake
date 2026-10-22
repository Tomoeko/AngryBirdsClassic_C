file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "bb08f9f5a8461004893723cae3754701558d9b501df9a27cc7f5b7228863b2b9")
    message(FATAL_ERROR "Expected the recorded original removal detach corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Removal detach comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
