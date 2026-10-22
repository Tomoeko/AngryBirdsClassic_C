file(SHA256 "${REFERENCE}" reference_hash)
if(NOT reference_hash STREQUAL
    "964906b999d0c0095f3071793c5a4c778b7bbdd7607843b1ceeb786dc7031450")
    message(FATAL_ERROR "Expected the recorded original bird damage-refresh corpus")
endif()
execute_process(COMMAND "${CHECKER}" "${REFERENCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics TIMEOUT 30)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Bird damage-refresh comparison failed: ${output}${diagnostics}")
endif()
message(STATUS "${output}")
