file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "0158b79dfada02bd4de26a6d796c2d3287199d2dfbad497a916d29b1a39f60d5")
    message(FATAL_ERROR "Expected the recorded native game-contact world corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Native game-contact world comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
