file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "e2c0d58a0015c6e2e8c0b175e77db4cfb4c0025278c3c6cd0a6f27cdae887195")
    message(FATAL_ERROR "Expected the recorded original object/teleporter removal corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native object/teleporter comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
